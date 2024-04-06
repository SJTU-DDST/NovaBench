#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <signal.h>

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
            mkdir(db_path, 0777);
        }

        // create database according to request
        for(int db_index = 0; db_index < cfg->fragments.size(); db_index++){
            dbs_.push_back(DatabaseFactory::createDatabase(BenchConfig::config->database));
        }

        // set db in fragment
        for (int db_index = 0; db_index < cfg->fragments.size(); db_index++) {
            NovaConfig::config->cfgs[0]->fragments[db_index]->db = dbs_[db_index];
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
            worker_threads.emplace_back(start, worker_threads[i]);
        }
        current_worker_id_ = 0;
        usleep(1000000);
    }
}