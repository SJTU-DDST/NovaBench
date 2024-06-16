#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gflags/gflags.h>

#include "config/config.h"
#include "server/server.h"

using namespace std;
using namespace bench;

// storage
DEFINE_string(db_path, "/tmp/db", "where to put data");

// server
DEFINE_string(all_servers, "localhost:11211", "A list of servers");//server的list
DEFINE_int64(server_id, -1, "Server id.");//当前这个server的id

// msg_size
DEFINE_uint64(max_msg_size, 0, "max msg size");//用户消息的最大大小

// // partition number 这个直接在config里面搞
// DEFINE_int32(partitions, 1, "how many instances in a server");//一个服务器起多少个

// value length
DEFINE_uint64(use_fixed_value_size, 0, "Fixed value size.");

// config path
DEFINE_string(ltc_config_path, "need value", "The path that stores the configuration.");//ltc分布的config

// connection workers
DEFINE_uint64(num_conn_workers, 0, "Number of connection worker threads");//服务器端用于连接的thread数量

// levels
DEFINE_int32(level, 2, "Number of levels.");//level的数量

// recovery 没用
DEFINE_bool(recover_dbs, false, "Enable recovery");//是否开启recovery

// access pattern ..没啥用
// DEFINE_string(client_access_pattern, "uniform", "Client access pattern used to report load imbalance across subranges.");//用户访问模式

// which database
DEFINE_string(database, "leveldb", "database used to support");// 用leveldb或者rocksdb

BenchConfig *BenchConfig::config; // static in config.h

void StartServer(){
    int port = BenchConfig::config->servers[BenchConfig::config->my_server_id].port;

    if (!FLAGS_recover_dbs) {
        int ret = system(fmt::format("exec rm -rf {}/*", BenchConfig::config->db_path).data());
    }
    
    mkdirs(BenchConfig::config->db_path.data());

    auto* bench_server = new BenchServer(port);

    bench_server->Start();
}

int main(int argc, char *argv[]){
    // gflag parsing
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    // check supports
    int i;
    const char **methods = event_get_supported_methods();
    printf("Starting Libevent %s.  Available methods are:\n",
           event_get_version());
    for (i = 0; methods[i] != NULL; ++i) {
        printf("    %s\n", methods[i]);
    }
    // check necessity
    if (FLAGS_server_id == -1) {
        exit(0);
    }
    // parse args
    std::vector<gflags::CommandLineFlagInfo> flags;
    gflags::GetAllFlags(&flags);
    for(const auto &flag : flags){
        printf("%s=%s\n,", flag.name.c_str(), flag.current_value.c_str());
    }

    BenchConfig::config = new BenchConfig();
    BenchConfig::config->max_msg_size = FLAGS_max_msg_size;
    BenchConfig::config->db_path = FLAGS_db_path;
    BenchConfig::config->recover_dbs = FLAGS_recover_dbs; //没用
    BenchConfig::config->servers = convert_hosts(FLAGS_all_servers);
    BenchConfig::config->my_server_id = FLAGS_server_id;
    BenchConfig::config->num_conn_workers = FLAGS_num_conn_workers;
    // BenchConfig::config->client_access_pattern = FLAGS_client_access_pattern;
    BenchConfig::config->level = FLAGS_level;
    // BenchConfig::config->partitions = FLAGS_partitions;
    BenchConfig::config->database = FLAGS_database;

    BenchConfig::ReadFragments(FLAGS_ltc_config_path);
    BENCH_LOG(INFO) << fmt::format("{} configurations", BenchConfig::config->cfgs.size());
    for (auto c : BenchConfig::config->cfgs) {
        BENCH_LOG(INFO) << c->DebugString();
    }

    // StartServer();
    return 0;
}