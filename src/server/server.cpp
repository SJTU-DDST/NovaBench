#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <signal.h>
#include <fmt/core.h>
#include <fcntl.h>

#include "server/server.h"
#include "config/config.h"
#include "common/common.h"
#include "databasefactory/databasefactory.h"

namespace bench{

    void start(Worker *store) {
        store->Start();
    }

    BenchServer::BenchServer(int port){
        Configuration* cfg = BenchConfig::config->cfgs[0];
        
        // create file for storage
        for(int i = 0; i < cfg->fragments.size(); i++){
            std::string db_path = DBName(BenchConfig::config->db_path, cfg->fragments[i]->dbid); 
            mkdir(db_path.c_str(), 0777);
        }

        // create database according to request
        for(int db_index = 0; db_index < cfg->fragments.size(); db_index++){
            dbs_.push_back(DatabaseFactory::createDatabase(BenchConfig::config->database, DBName(BenchConfig::config->db_path, cfg->fragments[db_index]->dbid)));
        }

        // set db in fragment
        for (int db_index = 0; db_index < cfg->fragments.size(); db_index++) {
            BenchConfig::config->cfgs[0]->fragments[db_index]->db = dbs_[db_index];
        }

        // set connection workers
        for(int i = 0; i < BenchConfig::config->num_conn_workers; i++){
            workers.push_back(new Worker(i));
        }

        // recover? no need
        // if(BenchConfig::config->recover_dbs){
        //     for (int db_index = 0; db_index < cfg->fragments.size(); db_index++){
        //         //recover
        //     }
        // }

        for(int i = 0; i < BenchConfig::config->num_conn_workers; i++){
            worker_threads.emplace_back(start, workers[i]);
        }
        current_worker_id_ = 0;
        usleep(1000000);
    }

    void make_socket_non_blocking(int sockfd) {
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        }
    }

    void on_accept(int fd, short which, void* arg){
        auto *server = (BenchServer *)arg;
        BENCH_ASSERT(fd == server->listen_fd_);
        BENCH_LOG(DEBUG) << "new connection " << fd;

        int client_fd;
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        client_fd = accept(fd, (struct sockaddr *) &client_addr, &client_len);
        BENCH_ASSERT(client_fd < BENCH_MAX_CONN) << client_fd
                                               << " not enough connections";
        BENCH_ASSERT(client_fd >= 0) << client_fd;
        make_socket_non_blocking(client_fd);
        BENCH_LOG(DEBUG) << "register " << client_fd;

        Worker *store = server->workers[server->current_worker_id_];
        if (BenchConfig::config->num_conn_workers == 1) {
            server->current_worker_id_ = 0;
        } else {
            server->current_worker_id_ =
                    (server->current_worker_id_ + 1) % BenchConfig::config->num_conn_workers;
        }

        store->conn_mu.lock();
        store->conn_queue.push_back(client_fd);
        store->conn_mu.unlock();
    }

    void BenchServer::Start(){
        SetupListener();
        struct event event{};
        struct event_config *ev_config;
        ev_config = event_config_new();
        BENCH_ASSERT(event_config_set_flag(ev_config, EVENT_BASE_FLAG_NOLOCK) == 0);
        BENCH_ASSERT(event_config_avoid_method(ev_config, "poll") == 0);
        BENCH_ASSERT(event_config_avoid_method(ev_config, "select") == 0);
        BENCH_ASSERT(event_config_set_flag(ev_config, EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST) == 0);
        base = event_base_new_with_config(ev_config);

        if(!base){
            fprintf(stderr, "Can't allocate event base\n");
            exit(1);
        }

        BENCH_LOG(INFO) << "Using Libevent with backend method " << event_base_get_method(base);
        const int f = event_base_get_features(base);
        if ((f & EV_FEATURE_ET)) {
            BENCH_LOG(INFO) << "Edge-triggered events are supported.";
        }
        if ((f & EV_FEATURE_O1)) {
            BENCH_LOG(INFO) << "O(1) event notification is supported.";
        }
        if ((f & EV_FEATURE_FDS)) {
            BENCH_LOG(INFO) << "All FD types are supported.";
        }

        memset(&event, 0, sizeof(struct event));
        BENCH_ASSERT(event_assign(&event, base, listen_fd_, EV_READ | EV_PERSIST, on_accept, (void *) this) == 0); // 客户端连接请求
        BENCH_ASSERT(event_add(&event, 0) == 0) << listen_fd_;
        BENCH_ASSERT(event_base_loop(base, 0) == 0) << listen_fd_;
        BENCH_LOG(INFO) << "started";        
    }

    void BenchServer::SetupListener(){
        int one = 1;
        struct linger ling = {0, 0};
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        BENCH_ASSERT(fd != -1) << "create socket failed";

        /**********************************************************
         * internet socket address structure: our address and port
         *********************************************************/
        struct sockaddr_in sin{};
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = INADDR_ANY;
        sin.sin_port = htons(nport_);

        /**********************************************************
         * bind socket to address and port
         *********************************************************/
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *) &one, sizeof(one));
        setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *) &ling, sizeof(ling));
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *) &one, sizeof(one));

        int ret = bind(fd, (struct sockaddr *) &sin, sizeof(sin));
        BENCH_ASSERT(ret != -1) << "bind port failed";

        /**********************************************************
         * put socket into listening state
         *********************************************************/
        ret = listen(fd, 65536);
        BENCH_ASSERT(ret != -1) << "listen socket failed";
        listen_fd_ = fd;
        make_socket_non_blocking(listen_fd_);
    }

}