#ifndef DATABASE_H
#define DATABASE_H

#include <string>

namespace bench{
    class Database {
    public:
        virtual ~Database() = default;
        virtual void put(const std::string& key, const std::string& value) = 0;
        virtual std::string get(const std::string& key) = 0;
    };
}



#endif