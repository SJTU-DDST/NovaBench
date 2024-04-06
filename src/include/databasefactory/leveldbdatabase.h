#ifndef LEVELDBDATABASE_H
#define LEVELDBDATABASE_H

#include <string>

#include "databasefactory/database.h"
#include "leveldb/db.h"
#include "log/log.h"

//tbd
namespace bench{
    class LevelDBDatabase : public IDatabase {
    public:
        LevelDBDatabase(const std::string& db_path){
            leveldb::Options options;
            options.create_if_missing = true;
            leveldb::Status status = leveldb::DB::Open(options, db_path, &db);
            BENCH_ASSERT(status.ok());
        }

        ~LevelDBDatabase(){
            delete db;
        }

        void put(const std::string& key, const std::string& value) override {
            // 实际的LevelDB put操作
            leveldb::Status status = db->put(leveldb::WriteOptions(), key, value);
            BENCH_ASSERT(status.ok());
        }

        std::string get(const std::string& key) override {
            // 实际的LevelDB get操作
            std::string val;
            leveldb::Status status = db->Get(leveldb::ReadOptions(), key, &val);
            BENCH_ASSERT(status.ok());
            return val;
        }

        leveldb::DB* db;
    };    
}


#endif