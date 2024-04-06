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

#include "config/config,h"
#include "worker/worker.h"
#include "common/common.h"
#include "log/log.h"
#include "databasefactory/databasefactory.h"

namespace bench{
    Connection *conns[BENCH_MAX_CONN];
    std::mutex conns_mutex;

    // fd can be read
    SocketState socket_read_handler(int fd, short which, Connection *conn){
        NOVA_ASSERT((which & EV_READ) > 0) << which;
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
                BENCH_ASSERT(worker->req_ind < BENCHConfig::config->max_msg_size);                
            }            
        }
        return COMPLETE;
    }

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
            store->stats.nwrites++;
        } while (total < conn->response_size);
        return COMPLETE;                        
    }

    // adjust after write
    void write_socket_complete(int fd, Connection *conn){
        Worker *worker = (Worker*) worker;
        conn->UpdateEventFlags(EV_READ | EV_PERSIST);
        worker->request_buf[0] = '~';
        conn->state = READ;
        worker->req_ind = 0;
        conn->response_ind = 0;
    }

    // specific operations to address requests
    bool process_socket_get(int fd, Connection *conn, char *request_buf, uint32_t server_cfg_id){
        Worker* worker = (Worker*) conn->worker;
        uint64_t int_key = 0;
        uint32_t nkey = str_to_int(request_buf, &int_key) - 1;
        uint64_t hv = keyhash(request_buf, nkey);

        Fragment *frag = BenchConfig::home_fragment(hv, server_cfg_id);
        BENCH_ASSERT(frag) << fmt::format("cfg:{} key:{}", server_cfg_id, hv);

        if(!frag->is_ready_){
            frag->is_ready_mutex_.lock();
            while(!frag->is_ready_){
                frag->is_ready_signal_.wait();
            }
            frag->is_ready_mutex_.unlock();
        }

        Database *db = reinterpret_cast<Database*>(frag->db);
        BENCH_ASSERT(db);
        std::string value;
        db->get();

        con.response_buf = worker->buf;
        uint32_t response_size = 0;
        char* response_buf = conn->response_buf;
        
    }


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
            return process_reintialize_qps(fd, conn);
        } else if (msg_type == RequestType::CLOSE_STOC_FILES) {
            return process_close_stoc_files(fd, conn);
        } else if (msg_type == RequestType::STATS) {
            return process_socket_stats_request(fd, conn);
        } else if (msg_type == RequestType::CHANGE_CONFIG) {
            return process_socket_change_config_request(fd, conn);
        } else if (msg_type == RequestType::QUERY_CONFIG_CHANGE) {
            return process_socket_query_ready_request(fd, conn);
        }
        BENCH_ASSERT(false) << msg_type;
        return false;
    }

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

// new client connection
    void new_conn_handler(int fd, short which, void *arg){
        Worker* worker = (Worker*) arg;
        conns_mutex.lock();
        worker->conn_mu.lock();
        worker->nconns += worker->conn_queue.size();
        if(worker->conn_queue() != 0){
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
            NOVA_ASSERT(event_add(&new_conn_timer_event, &tv) == 0);
        }

        BENCH_ASSERT(event_base_loop(base, 0) == 0);
        BENCH_LOG(INFO) << "started";
    }

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

// refresh event flag
    void Connection::UpdateEventFlags(int new_flags) {
        if (event_flags == new_flags) {
            return;
        }
        event_flags = new_flags;
        NOVA_ASSERT(event_del(&event) == 0) << fd;
        NOVA_ASSERT(
                event_assign(&event, ((Worker *) worker)->base, fd,
                             new_flags,
                             event_handler,
                             this) ==
                0) << fd;
        NOVA_ASSERT(event_add(&event, 0) == 0) << fd;
    }
}