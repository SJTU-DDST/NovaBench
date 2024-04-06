#ifndef WORKER_H
#define WORKER_H

#include <event.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>

#include "common/common.h"
#include "config/config.h"
#include "log/log.h"

namespace bench{

    void event_handler(int fd, short which, void *arg);

    struct Worker{
        // for record
        timeval start{};
        timeval read_start{};
        timeval write_start{};
        int thread_id_ = 0;

        // for connection
        int listen_fd_ = -1;            /* listener descriptor      */
        int epoll_fd_ = -1;      /* used for all notification*/
        std::mutex mutex_;
        struct event_base *base = nullptr;
        int nconns = 0;
        mutex conn_mu;
        vector<int> conn_queue;
        vector<Connection *> conns;

        // for statistics not for now
        // Stats stats;
        // Stats prev_stats;
        unsigned int rand_seed = 0;        

        // message receive and send
        char *request_buf = nullptr;
        char *buf = nullptr;
        uint32_t req_ind = 0;

        Worker(int thread_id):thread_id_(thread_id){
            BENCH_LOG(INFO) << "memstore[" << thread_id << "]: "<< "create conn thread :" << thread_id;
            rand_seed = thread_id;

            request_buf = (char*) malloc(BenchConfig::config->max_msg_size);
            buf = (char *) malloc(BenchConfig::config->max_msg_size);

            BENCH_ASSERT(request_buf != NULL);
            BENCH_ASSERT(buf != NULL);
            
            memset(request_buf, 0, BenchConfig->max_msg_size);
            memset(buf, 0, BenchConfig->max_msg_size);
        }

        void Start();
    }

}

#endif