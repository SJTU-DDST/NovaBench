#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <thread>

#include "server/server.h"
#include "databasefactory/database.h"
#include "worker/worker.h"

namespace bench{
    struct BenchServer{

        // for connection
        int nport_;
        int listen_fd_ = -1;

        // db
        std::vector<Database*> dbs_;

        // connection workers and threads
        std::vector<Worker*> workers;

        // thread pool
        std::vector<thread> worker_threads;

        // index
        int current_worker_id_;

        // func
        BenchServer(int port); //ip 端口号

        void Start();
        
        void SetupListener();
    }
}


#endif