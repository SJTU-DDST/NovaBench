#ifndef CONFIG_H
#define CONFIG_H

#include <sstream>
#include <string>
#include <fstream>
#include <list>
#include <fmt/core.h>
#include <thread>
#include <syscall.h>
#include <atomic>
#include <map>

#include "common/common.h"

namespace bench{
    using namespace std;

    // config file abstraction
    struct Configuration {
        uint32_t cfg_id = 0;

        uint64_t start_time_in_seconds = 0;
        uint64_t start_time_us = 0;

        std::vector<Fragment*> fragments;
        std::vector<uint32_t> ltc_servers;
        std::vector<uint32_t> stoc_servers;
        std::vector<uint32_t> servers;
        std::vector<uint32_t> server_ids;

        int num_conn_workers = 0;

        std::string DebugString();
    }

    struct BenchConfig {
        vector<Host> servers;
        int my_server_id = 0;

        int max_msg_size = 0;

        uint64_t load_default_value_size = 0;
        std::string db_path;

        bool recover_dbs = false;
        std::string client_access_pattern;
        bool enable_detailed_db_stats = false;

        int partitions = 1;
        std::string database = "leveldb";

        int level = 0;
        std::vector<Configuration *> cfgs;
        std::atomic_uint_fast32_t current_cfg_id;
        std::mutex m;
        std::map<std::thread::id, pid_t> threads;
        static BenchConfig *config;
        
        BenchConfig() {
            current_cfg_id = 0;
        }

        static void ReadFragments(const std::string &path){
            std::string line;
            ifstream file;
            file.open(path);

            Configuration *cfg = nullptr;
            uint32_t cfg_id = 0;
            while(std::getline(file, line)){
                if(line.find("config") != std::string::npos){
                    cfg = new Configuration;
                    cfg->cfg_id = cfg_id;
                    cfg_id++;
                    config->cfgs.push_back(cfg);
// 原来的ltc
                    BENCH_ASSERT(std::getline(file, line));
                    cfg->ltc_servers = SplitByDelimiterToInt(&line, ",");
// 原来的stoc
                    BENCH_ASSERT(std::getline(file, line));
                    cfg->stoc_servers = SplitByDelimiterToInt(&line, ",");
// 组织为统一的
                    cfg->servers = cfg->ltc_servers;
                    cfg->servers.insert(cfg->servers.end(), cfg->stoc_servers.begin(), cfg->stoc_servers.end());
// 原来的启动时间
                    cfg->start_time_in_seconds = std::stoi(line); 
// server_id 这个不太有意义了
                    for(int i = 0; i < cfg->servers.size(); i++){
                        cfg->server_ids.push_back(cfg->servers[i]);
                    }
                    continue;
                }
                BENCH_LOG(INFO) << fmt::format("Read config line: {}", line);
// fragment                
                auto *frag = new Fragment();
                std::vector<std::string> tokens = SplitByDelimiter(&line, ",");
                frag->range.key_start = std::stoll(tokens[0]);
                frag->range.key_end = std::stoll(tokens[1]);
                frag->ltc_server_id = std::stoll(tokens[2]);
                frag->dbid = std::stoi(tokens[3]);
// 第一个config直接有效
                if(cfg->cfg_id == 0){
                    frag->is_ready_ = true;
                    frag->is_complete_ = true;
                }
// 这里基本不存在
                int nreplicas = (tokens.size() - 4);
                for(int i = 0; i < nreplicas; i++){
                    ;
                }
                cfg->fragments.push_back(frag);
            }
        }

// 这里是属于哪个服务器 而不是属于服务器中的具体数据库
        static Fragment* home_fragment(uint64_t key, uint32_t server_cfg_id){
            Fragment *home = nullptr;
            Configuration *cfg = config->cfgs[server_cfg_id];
            uint32_t l = 0;
            uint32_t r = cfg->fragments.size() - 1;

            while(l <= r){
                uint32_t m = l + (r - l) / 2;
                home = cfg->fragments[m];
                if (key >= home->range.key_start && key < home->range.key_end) {
                    return home;
                }
                if (key >= home->range.key_end){
                    l = m + 1;
                }
                else{
                    r = m - 1;
                }   
            }
            return nullptr;
        }
    }
}



#endif