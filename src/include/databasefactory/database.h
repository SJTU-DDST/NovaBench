#ifndef DATABASE_H
#define DATABASE_H

#include <string>

namespace bench{
    class Database {
    public:
        virtual ~Database() = default;
        virtual void put(const std::string& key, const std::string& value) = 0;
        virtual std::string get(const std::string& key) = 0;
        virtual void scan(const std::string& startkey, int& read_records, uint64_t& nrecords, char* response_buf, uint64_t& scan_size) = 0;
    };
}



#endif