#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <vector>
#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <event.h>
#include <list>
#include <stdexcept>

#include "log/log.h"

namespace bench{

// range arragements
    struct RangePartition {
        uint64_t key_start;
        uint64_t key_end;
    }; 

// db arrangements
    struct Fragment{
        RangePartition range;
        uint32_t dbid;
        uint32_t server_id;
        
        void *db = nullptr;
        std::atomic_bool is_ready_;
        std::mutex is_ready_mutex_;
        std::condition_variable is_ready_signal_;

        Fragment();
        std::string DebugString();
    }

// server info
    struct Host {
        uint32_t server_id;
        string ip;
        int port;

        std::string DebugString() const;
    };

// for reading configuration
    std::vector<std::string> SplitByDelimiter(std::string *s, std::string delimiter);

// for reading configuration
    std::vector<uint32_t> SplitByDelimiterToInt(std::string *s, std::string delimiter);

// for reading commandline server info
    vector<Host> convert_hosts(string hosts_str);

// build file directory
    void mkdirs(const char *dir);

// obtain specific location for each db
    std::string DBName(const std::string &dbname, uint32_t index);

// process key
    uint64_t keyhash(const char *key, uint64_t nkey);

// client connection
    enum ConnState {
        READ, WRITE
    };

    enum SocketState {
        INCOMPLETE, COMPLETE, CLOSED
    };

    class Connection {
    public:
        int fd;
        int req_size;
        int response_ind;
        uint32_t response_size;
        char *response_buf = nullptr; // A pointer points to the response buffer.
        ConnState state;
        void *worker;
        struct event event;
        int event_flags;
        uint32_t number_get_retries = 0;

        void Init(int f, void *store);
        void UpdateEventFlags(int new_flags);
    };

    SocketState socket_read_handler(int fd, short which, Connection *conn);

    bool process_socket_request_handler(int fd, Connection *conn);

    void write_socket_complete(int fd, Connection *conn);

    SocketState socket_write_handler(int fd, Connection *conn);

    enum RequestType : char {
        GET = 'g',
        PUT = 'p',
        VERIFY_LOAD = 'v',
        REQ_SCAN = 'r',
        REDIRECT = 'r',
        GET_INDEX = 'i',
        EXISTS = 'h',
        MISS = 'm',
        REINITIALIZE_QP = 'a',
        CLOSE_STOC_FILES = 'c',
        STATS = 's',
        CHANGE_CONFIG = 'b',
        QUERY_CONFIG_CHANGE = 'R',
    };

    static RequestType char_to_req_type(char c) {
        switch (c) {
            case 'g':
                return GET;
            case 'p' :
                return PUT;
            case 'r' :
                return REDIRECT;
            case 'i':
                return GET_INDEX;
        }
        BENCH_ASSERT(false) << "Unknown request type " << c;
    }    

#define TERMINATER_CHAR '!'
#define MSG_TERMINATER_CHAR '\n'
#define BENCH_MAX_CONN 1000000


    uint32_t int_to_str(char *str, uint64_t x);

    uint32_t str_to_int(const char *str, uint64_t *out, uint32_t nkey = 0);

}

#endif