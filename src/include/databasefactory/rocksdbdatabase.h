#ifndef ROCKSDBDATABASE_H
#define ROCKSDBDATABASE_H

#include <string>

#include "databasefactory/database.h"
#include "rocksdb/db.h"
// #include "rocksdb/slice.h"
// #include "rocksdb/options.h"
#include "log/log.h"

namespace bench{
    class RocksDBDatabase : public IDatabase {
    public:
        RocksDBDatabase(const std::string& db_path){
            rocksdb::Options options;
            options.IncreaseParallelism();
            options.OptimizeLevelStyleCompaction();
            options.create_if_missing = true;
            rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db);
            BENCH_ASSERT(status.ok());
        }

        ~RocksDBDatabase(){
            delete db;
        }

        void put(const std::string& key, const std::string& value) override {
            // 实际的RocksDB put操作
            rocksdb::Status status = db->put(rocksdb::WriteOptions(), key, value);
            BENCH_ASSERT(status.ok());            
        }

        std::string get(const std::string& key) override {
            // 实际的RocksDB get操作
            std::string val;
            rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &val);
            BENCH_ASSERT(status.ok());
            return val;            
        }

        rocksdb::DB* db;
    };    
}


#endif