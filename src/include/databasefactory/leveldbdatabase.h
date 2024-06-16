#ifndef LEVELDBDATABASE_H
#define LEVELDBDATABASE_H

#include <string>

#include "databasefactory/database.h"
#include "leveldb/db.h"
#include "log/log.h"
#include "common/common.h"

//tbd
namespace bench{
class LevelDBDatabase : public Database {
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
        leveldb::Status status = db->Put(leveldb::WriteOptions(), key, value);
        BENCH_ASSERT(status.ok());
    }

    std::string get(const std::string& key) override {
        // 实际的LevelDB get操作
        std::string val;
        leveldb::Status status = db->Get(leveldb::ReadOptions(), key, &val);
        BENCH_ASSERT(status.ok());
        return val;
    }

    void scan(const std::string& startkey, int& read_records, uint64_t& nrecords, char* response_buf, uint64_t& scan_size) override {
        leveldb::Iterator *iterator = db->NewIterator(leveldb::ReadOptions());
        iterator->Seek(startkey);
        while(iterator->Valid() && read_records < nrecords){
            leveldb::Slice key = iterator->key();
            leveldb::Slice value = iterator->value();
            scan_size += nint_to_str(key.size()) + 1;
            scan_size += key.size();
            scan_size += nint_to_str(value.size()) + 1;
            scan_size += value.size();

            response_buf += int_to_str(response_buf, key.size());
            memcpy(response_buf, key.data(), key.size());
            response_buf += key.size();
            response_buf += int_to_str(response_buf, value.size());
            memcpy(response_buf, value.data(), value.size());
            response_buf += value.size();
            read_records++;
            iterator->Next();
        }

        delete iterator;
    }

    leveldb::DB* db;
};    
}


#endif