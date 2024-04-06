#include "common/common.h"
#include "common/log.h"

namespace bench{
    Fragment::Fragment() : is_ready_(false) {}

    std::string Fragment::Debugstring() {
        return fmt::format("[{},{}): {}-{}", range.key_start, range.key_end, server_id, dbid);
    }

    std::string Host::DebugString() const {
        return fmt::format("{}:{}:{}", server_id, ip, port);
    }

    std::vector<std::string> SplitByDelimiter(std::string *s, std::string delimiter) {
        size_t pos = 0;
        std::string token;
        std::vector<std::string> tokens;
        while ((pos = s->find(delimiter)) != std::string::npos) {
            token = s->substr(0, pos);
            tokens.push_back(token);
            s->erase(0, pos + delimiter.length());
        }
        if (!s->empty()) {
            tokens.push_back(*s);
        }
        return tokens;
    }

    std::vector<uint32_t>
    SplitByDelimiterToInt(std::string *s, std::string delimiter) {
        size_t pos = 0;
        std::string token;
        std::vector<uint32_t> tokens;
        while ((pos = s->find(delimiter)) != std::string::npos) {
            token = s->substr(0, pos);
            tokens.push_back(std::stoi(token));
            s->erase(0, pos + delimiter.length());
        }
        if (!s->empty()) {
            tokens.push_back(std::stoi(*s));
        }
        return tokens;
    }

    vector<Host> convert_hosts(string hosts_str) {
        BENCH_LOG(INFO) << hosts_str;
        std::vector<Host> hosts;
        std::stringstream ss_hosts(hosts_str);
        uint32_t host_id = 0;
        while (ss_hosts.good()) {
//读入一个ip:port
            string host_str;
            getline(ss_hosts, host_str, ',');

            if (host_str.empty()) {
                continue;
            }
            std::vector<std::string> ip_port;
            std::stringstream ss_ip_port(host_str);
//ipheport提取出来放进去
            while (ss_ip_port.good()) {
                std::string substr;
                getline(ss_ip_port, substr, ':');
                ip_port.push_back(substr);
            }
            Host host = {};
            host.server_id = host_id;
            host.ip = ip_port[0];
            host.port = atoi(ip_port[1].c_str());
            hosts.push_back(host);
            host_id++;
        }
        return hosts;
    }

    void mkdirs(const char *dir) {
        char tmp[1024];
        char *p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp), "%s", dir);
        len = strlen(tmp);
        if (tmp[len - 1] == '/')
            tmp[len - 1] = 0;
        for (p = tmp + 1; *p; p++) {
            if (*p == '/') {
                *p = 0;
                mkdir(tmp, 0777);
                *p = '/';
            }
        }
        mkdir(tmp, 0777);
    }

    std::string DBName(const std::string &dbname, uint32_t index) {
        return dbname + "/" + std::to_string(index);
    }

    uint32_t int_to_str(char *str, uint64_t x) {
        uint32_t len = 0, p = 0;
        do {
            str[len] = static_cast<char>((x % 10) + '0');
            x = x / 10;
            len++;
        } while (x);
        int q = len - 1;
        char temp;
        while (p < q) {
            temp = str[p];
            str[p] = str[q];
            str[q] = temp;
            p++;
            q--;
        }
        str[len] = TERMINATER_CHAR;
        return len + 1;
    }

    uint32_t str_to_int(const char *str, uint64_t *out, uint32_t nkey = 0) {
        if (str[0] == MSG_TERMINATER_CHAR) {
            return 0;
        }
        uint32_t len = 0;
        uint64_t x = 0;
        while (str[len] != TERMINATER_CHAR) {
            if (nkey != 0 && len == nkey) {
                break;
            }
            if (str[len] > '9' || str[len] < '0') {
                break;
            }
            x = x * 10 + (str[len] - '0');
            len += 1;
        }
        *out = x;
        return len + 1;
    }    

    uint64_t keyhash(const char *key, uint64_t nkey) {
        uint64_t hv = 0;
        str_to_int(key, &hv, nkey);
        return hv;
    }

}