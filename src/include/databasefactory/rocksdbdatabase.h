#ifndef ROCKSDBDATABASE_H
#define ROCKSDBDATABASE_H

#include <string>

#include "databasefactory/database.h"
#include "rocksdb/db.h"
// #include "rocksdb/slice.h"
// #include "rocksdb/options.h"
#include "log/log.h"
#include "common/common.h"

namespace bench{
class RocksDBDatabase : public Database {
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
        rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key, value);
        BENCH_ASSERT(status.ok());            
    }

    std::string get(const std::string& key) override {
        // 实际的RocksDB get操作
        std::string val;
        rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &val);
        BENCH_ASSERT(status.ok());
        return val;            
    }

    void scan(const std::string& startkey, int& read_records, uint64_t& nrecords, char* response_buf, uint64_t& scan_size) override {
        rocksdb::Iterator *iterator = db->NewIterator(rocksdb::ReadOptions());
        iterator->Seek(startkey);
        while(iterator->Valid() && read_records < nrecords){
            rocksdb::Slice key = iterator->key();
            rocksdb::Slice value = iterator->value();
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

    rocksdb::DB* db;
};    
}


#endif