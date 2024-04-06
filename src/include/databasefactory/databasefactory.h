#ifndef DATABASEFACTORY_H
#define DATABASEFACTORY_H

#include <string>

#include "databasefactory/database.h"
#include "databasefactory/leveldbdatabase.h"
#include "databasefactory/rocksdbdatabase.h"
#include "common/common.h"
#include "log/log.h"

namespace bench{
    class DatabaseFactory {
    public:
        static Database* createDatabase(const std::string& type, const std::string& db_path) {
            if (type == "leveldb") {
                return new LevelDBDatabase(db_path);
            } else if (type == "rocksdb") {
                return new RocksDBDatabase(db_path);
            } else {
                BENCH_ASSERT(false) << "unknown database type";
            }
        }
    };    
}



#endif