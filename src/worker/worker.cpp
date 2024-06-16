#include <sys/types.h>
#include <sys/signalfd.h>
#include <sys/epoll.h>
#include <cerrno>
#include <poll.h>
#include <signal.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <event.h>



#include "config/config.h"
#include "worker/worker.h"
#include "common/common.h"
#include "log/log.h"
#include "databasefactory/databasefactory.h"

#include <string>

namespace bench{
    Connection *conns[BENCH_MAX_CONN];
    std::mutex conns_mutex;

    // done
    // fd can be read
    SocketState socket_read_handler(int fd, short which, Connection *conn){
        BENCH_ASSERT((which & EV_READ) > 0) << which;
        Worker* worker = (Worker*) conn->worker;
        char* buf = worker->request_buf + worker->req_ind;
        bool complete = false;

        // read all can be read
        if(worker->req_ind == 0){
            int count = read(fd, buf, BenchConfig::config->max_msg_size);
            if(count <= 0){
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    return INCOMPLETE;
                }
                return CLOSED;            
            }
            if(count > 0){
                if(buf[count - 1] == MSG_TERMINATER_CHAR){
                    complete = true;
                }
                worker->req_ind += count;
                buf += count;
            }
        }
        while(!complete){
            int count = read(fd, buf, 1);
            if (count <= 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    return INCOMPLETE;
                }
                return CLOSED;
            }
            if(count > 0){
                if (buf[0] == MSG_TERMINATER_CHAR) {
                    break;
                }
                worker->req_ind += 1;
                buf += 1;
                BENCH_ASSERT(worker->req_ind < BenchConfig::config->max_msg_size);                
            }            
        }
        return COMPLETE;
    }

    // done
    // fd can be writen
    SocketState socket_write_handler(int fd, Connection *conn){
        BENCH_ASSERT(conn->response_size < BenchConfig::config->max_msg_size);
        Worker* worker = (Worker*) conn->worker;
        struct iovec iovec_array[1];
        iovec_array[0].iov_base = conn->response_buf + conn->response_ind;
        iovec_array[0].iov_len = conn->response_size - conn->response_ind;        
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = &iovec_array[0];
        msg.msg_iovlen = 1;
        int n = 0;
        int total = 0;
        do {
            iovec_array[0].iov_base = (char *) (iovec_array[0].iov_base) + n;
            iovec_array[0].iov_len -= n;
            n = sendmsg(fd, &msg, MSG_NOSIGNAL);
            if (n <= 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    BENCH_LOG(WARNING) << "memstore[" << worker->thread_id_
                                      << "]: "
                                      << "write socket would block fd: "
                                      << fd
                                      << " "
                                      << strerror(errno);
                    conn->state = ConnState::WRITE;
                    conn->UpdateEventFlags(EV_WRITE | EV_PERSIST);
                    return INCOMPLETE;
                }
                return CLOSED;
            }
            conn->response_ind += n;
            total = conn->response_ind;
            // store->stats.nwrites++;
        } while (total < conn->response_size);
        return COMPLETE;                        
    }

    // done
    // adjust after write
    void write_socket_complete(int fd, Connection *conn){
        Worker *worker = (Worker*) worker;
        conn->UpdateEventFlags(EV_READ | EV_PERSIST);
        worker->request_buf[0] = '~';
        conn->state = READ;
        worker->req_ind = 0;
        conn->response_ind = 0;
    }

    // done
    // specific operations to address requests
    bool process_socket_get(int fd, Connection *conn, char *request_buf, uint32_t server_cfg_id){
        Worker* worker = (Worker*) conn->worker;
        uint64_t int_key = 0;
        uint32_t nkey = str_to_int(request_buf, &int_key) - 1; // key长度
        uint64_t hv = keyhash(request_buf, nkey); // key值

        Fragment *frag = BenchConfig::home_fragment(hv, server_cfg_id);
        BENCH_ASSERT(frag) << fmt::format("cfg:{} key:{}", server_cfg_id, hv);

        if(!frag->is_ready_){
            {
                std::unique_lock<std::mutex> lock(frag->is_ready_mutex_);
                frag->is_ready_signal_.wait(lock,
                    [frag](){
                        bool ready = frag->is_ready_;
                        return ready;
                    }
                );
            }
            // frag->is_ready_mutex_.lock();
            // while(!frag->is_ready_){
            //     frag->is_ready_signal_.wait();
            // }
            // frag->is_ready_mutex_.unlock();
        }

        Database *db = reinterpret_cast<Database*>(frag->db);
        BENCH_ASSERT(db);
        std::string value = db->get(std::string(request_buf, nkey));

        conn->response_buf = worker->buf;
        uint32_t response_size = 0;
        char* response_buf = conn->response_buf;
        uint32_t cfg_size = int_to_str(response_buf, server_cfg_id);
        response_size += cfg_size;
        response_buf += cfg_size;
        uint32_t value_size = int_to_str(response_buf, value.size());
        response_size += value_size;
        response_buf += value_size;
        memcpy(response_buf, value.data(), value.size());
        response_buf[0] = MSG_TERMINATER_CHAR;
        response_size += 1;
        conn->response_size = response_size;
        
        BENCH_ASSERT(conn->response_size < BenchConfig::config->max_msg_size);
        
        return true;
    }

    // done
    bool process_socket_put(int fd, Connection *conn, char *request_buf, uint32_t server_cfg_id){
        Worker* worker = (Worker *)conn->worker;
        char* buf = request_buf;
        char *ckey;
        uint64_t key = 0;
        ckey = buf; // key第一个字符位置
        int nkey = str_to_int(buf, &key) - 1; // 读出key和长度
        buf += nkey + 1;
        uint64_t nval;
        buf += str_to_int(buf, &nval); // val长度
        char* val = buf; // val第一个字符位置
        uint64_t hv = keyhash(ckey, nkey);
        Fragment* frag = BenchConfig::home_fragment(hv, server_cfg_id);
        BENCH_ASSERT(frag) << fmt::format("cfg:{} key:{}", server_cfg_id, hv);
        if (!frag->is_ready_) {
            {
                std::unique_lock<std::mutex> lock(frag->is_ready_mutex_);
                frag->is_ready_signal_.wait(lock,
                    [frag](){
                        bool ready = frag->is_ready_;
                        return ready;
                    }
                );
            }
            // frag->is_ready_mutex_.lock();
            // while (!frag->is_ready_) {
            //     frag->is_ready_signal_.wait();
            // }
            // frag->is_ready_mutex_.unlock();
        }

        auto* db = reinterpret_cast<Database*>(frag->db);
        BENCH_ASSERT(db) << fmt::format("cfg:{} key:{}", server_cfg_id, hv);

        db->put(std::string(ckey, nkey), std::string(val, nval));
        
        char *response_buf = worker->buf;
        uint32_t response_size = 0;
        uint32_t cfg_size = int_to_str(response_buf, server_cfg_id);
        response_buf += cfg_size;
        response_size += cfg_size;
        response_buf[0] = MSG_TERMINATER_CHAR;
        response_size += 1;
        conn->response_buf = worker->buf;
        conn->response_size = response_size;
        return true;
    }

    // done
    bool process_socket_scan(int fd, Connection *conn, char *request_buf, uint32_t server_cfg_id){
        Worker* worker = (Worker*) conn->worker;
        char* startkey;
        uint64_t key = 0;
        char *buf = request_buf;
        startkey = buf;
        int nkey = str_to_int(buf, &key) - 1;
        buf += nkey + 1;
        uint64_t nrecords;
        buf += str_to_int(buf, &nrecords);
        std::string skey(startkey, nkey);

        BENCH_LOG(DEBUG) << fmt::format("memstore[{}]: scan fd:{} key:{} nkey:{} nrecords:{}", worker->thread_id_, fd, skey, nkey, nrecords);
        uint64_t hv = keyhash(startkey, nkey);
        auto cfg = BenchConfig::config->cfgs[server_cfg_id];
        Fragment *frag = BenchConfig::home_fragment(hv, server_cfg_id);
        BENCH_ASSERT(frag) << fmt::format("cfg:{} key:{}", server_cfg_id, hv);        

        // uint64_t hv = keyhash(startkey, nkey);
        // auto cfg = BenchConfig::config->cfgs[server_cfg_id];
        // Fragment* frag = BenchConfig::home_fragment(hv, server_cfg_id);
        // BENCH_ASSERT(frag) << fmt::format("cfg:{} key:{}", server_cfg_id, hv);
    
        int pivot_db_id = frag->dbid;
        int read_records = 0;
        uint64_t prior_last_key = -1;
        uint64_t scan_size = 0;

        conn->response_buf = worker->buf;
        char* response_buf = conn->response_buf;
        uint32_t cfg_size = int_to_str(response_buf, server_cfg_id);
        response_buf += cfg_size;
        scan_size += cfg_size;

        while(read_records < nrecords && pivot_db_id < cfg->fragments.size()){
            frag = cfg->fragments[pivot_db_id];
            if(prior_last_key != -1 && prior_last_key != frag->range.key_start){
                break;
            }
            if(frag->server_id != BenchConfig::config->my_server_id){
                break;
            }
            if(!frag->is_ready_){
                {
                    std::unique_lock<std::mutex> lock(frag->is_ready_mutex_);
                    frag->is_ready_signal_.wait(lock,
                        [frag](){
                            bool ready = frag->is_ready_;
                            return ready;
                        }
                    );
                }
            }

            // scan

            reinterpret_cast<Database*>(frag->db)->scan(startkey, read_records, nrecords, response_buf, scan_size);
            prior_last_key = frag->range.key_end;
            pivot_db_id += 1;
        }                
    
        BENCH_LOG(DEBUG) << fmt::format("Scan size:{}", scan_size);
        conn->response_buf[scan_size] = MSG_TERMINATER_CHAR;
        scan_size += 1;
        conn->response_size = scan_size;
        BENCH_ASSERT(conn->response_size < BenchConfig::config->max_msg_size);
        return true;
    }

// done
    // check what request this is
    bool process_socket_request_handler(int fd, Connection *conn){
        auto worker = (Worker *) conn->worker;
        char* request_buf = worker->request_buf;
        char msg_type = request_buf[0];
        request_buf++;
        uint32_t server_cfg_id = BenchConfig::config->current_cfg_id;
        if (msg_type == RequestType::GET || msg_type == RequestType::REQ_SCAN ||msg_type == RequestType::PUT){
            uint64_t client_cfg_id = 0;
            request_buf += str_to_int(request_buf, &client_cfg_id);
            if (client_cfg_id != server_cfg_id) {
                char *response_buf = worker->buf;
                int len = int_to_str(response_buf, server_cfg_id);
                response_buf += len;
                response_buf[0] = MSG_TERMINATER_CHAR;
                conn->response_buf = worker->buf;
                conn->response_size = len + 1;
                return true;
            }
        }
        if (msg_type == RequestType::GET) {
            return process_socket_get(fd, conn, request_buf, server_cfg_id);
        } else if (msg_type == RequestType::REQ_SCAN) {
            return process_socket_scan(fd, conn, request_buf, server_cfg_id);
        } else if (msg_type == RequestType::PUT) {
            return process_socket_put(fd, conn, request_buf, server_cfg_id);
        } else if (msg_type == RequestType::REINITIALIZE_QP) {
            // 不可能
            BENCH_ASSERT(false) << "not supposed to be here";
        } else if (msg_type == RequestType::CLOSE_STOC_FILES) {
            // 不可能
            BENCH_ASSERT(false) << "not supposed to be here";
        } else if (msg_type == RequestType::STATS) {
            // 不可能
            BENCH_ASSERT(false) << "not supposed to be here";
        } else if (msg_type == RequestType::CHANGE_CONFIG) {
            // 不可能
            BENCH_ASSERT(false) << "not supposed to be here";
        } else if (msg_type == RequestType::QUERY_CONFIG_CHANGE) {
            // 不可能
            BENCH_ASSERT(false) << "not supposed to be here";
        }
        BENCH_ASSERT(false) << msg_type;
        return false;
    }

    // done
    // a connected client's request
    void event_handler(int fd, short which, void *arg){
        auto* conn = (Connection*) arg;
        BENCH_ASSERT(fd == conn->fd) << fd << ":" << conn->fd;
        SocketState state;
        Worker* worker = (Worker*) conn->worker;

        if(conn->state == ConnState::READ){
            state = socket_read_handler(fd, which, conn);
            if(state == COMPLETE){
                bool reply = process_socket_request_handler(fd, conn);
                if(reply){
                    socket_write_handler(fd, conn);
                    if(state == COMPLETE){
                        write_socket_complete(fd, conn);
                    }
                }
            }        
        }else{
            BENCH_ASSERT((which & EV_WRITE) > 0);
            state = socket_write_handler(fd, conn);
            if(state == COMPLETE){
                write_socket_complete(fd, conn);
            }
        }
        
        if(state == CLOSED){
            BENCH_ASSERT(event_del(&conn->event) == 0) << fd;
            close(fd);
        }
    }

// done
// new client connection
    void new_conn_handler(int fd, short which, void *arg){
        Worker* worker = (Worker*) arg;
        conns_mutex.lock();
        worker->conn_mu.lock();
        worker->nconns += worker->conn_queue.size();
        if(worker->conn_queue.size() != 0){
            BENCH_LOG(DEBUG) << "memstore[" << worker->thread_id_ << "]: conns "<< worker->nconns;        
        }
        for(int i = 0; i < worker->conn_queue.size(); i++){
            int client_fd = worker->conn_queue[i];
            Connection *conn = new Connection();
            conn->Init(client_fd, worker);
            BENCH_ASSERT(client_fd < BENCH_MAX_CONN) << "memstore[" << worker->thread_id_ << "]: too large " << client_fd;
            conns[client_fd] = conn;
            BENCH_LOG(DEBUG) << "memstore[" << worker->thread_id_<< "]: connected " << client_fd;
            BENCH_ASSERT(event_assign(&conn->event, worker->base, client_fd, EV_READ | EV_PERSIST, event_handler, conn) == 0) << fd;
            BENCH_ASSERT(event_add(&conn->event, 0) == 0) << client_fd;
        }
        worker->conn_queue.clear();
        worker->conn_mu.unlock();
        conns_mutex.unlock();
    }

// done
// connection thread begin
    void Worker::Start(){
        BENCH_LOG(DEBUG) << "memstore[" << thread_id_ << "]: "
                            << "starting mem worker";
        
        // regular see if there is new connection
        struct event new_conn_timer_event;

        // basic config
        struct event_config *ev_config;
        ev_config = event_config_new();
        BENCH_ASSERT(event_config_set_flag(ev_config, EVENT_BASE_FLAG_NOLOCK) == 0);
        BENCH_ASSERT(event_config_avoid_method(ev_config, "poll") == 0);
        BENCH_ASSERT(event_config_avoid_method(ev_config, "select") == 0);
        BENCH_ASSERT(event_config_set_flag(ev_config, EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST) == 0);
        base = event_base_new_with_config(ev_config);
        if (!base){
            BENCH_ASSERT(false) << "Can't allocate event base\n";
        }
        BENCH_LOG(DEBUG) << "Using Libevent with backend method " << event_base_get_method(base);
        const int f = event_base_get_features(base);
        if((f & EV_FEATURE_ET)) {
            BENCH_LOG(DEBUG) << "Edge-triggered events are supported.";
        }
        if((f & EV_FEATURE_O1)) {
            BENCH_LOG(DEBUG) << "O(1) event notification is supported.";
        }
        if ((f & EV_FEATURE_FDS)) {
            BENCH_LOG(DEBUG) << "All FD types are supported.";
        }

        // check new connection every 2 seconds
        {
            struct timeval tv;
            tv.tv_sec = 2;
            tv.tv_usec = 0;
            memset(&new_conn_timer_event, 0, sizeof(struct event));
            BENCH_ASSERT(
                    event_assign(&new_conn_timer_event, base, -1, EV_PERSIST,
                                    new_conn_handler, (void *) this) == 0); // 这里没有监听任何fd，只是定时调用 其实就是每个worker
            BENCH_ASSERT(event_add(&new_conn_timer_event, &tv) == 0);
        }

        BENCH_ASSERT(event_base_loop(base, 0) == 0);
        BENCH_LOG(INFO) << "started";
    }

// done
// init a connection
    void Connection::Init(int f, void *store) {
        fd = f;
        req_size = -1;
        response_ind = 0;
        response_size = 0;
        state = READ;
        this->worker = store;
        event_flags = EV_READ | EV_PERSIST;
    }

// done
// refresh event flag
    void Connection::UpdateEventFlags(int new_flags) {
        if (event_flags == new_flags) {
            return;
        }
        event_flags = new_flags;
        BENCH_ASSERT(event_del(&event) == 0) << fd;
        BENCH_ASSERT(
                event_assign(&event, ((Worker *) worker)->base, fd,
                             new_flags,
                             event_handler,
                             this) ==
                0) << fd;
        BENCH_ASSERT(event_add(&event, 0) == 0) << fd;
    }
}