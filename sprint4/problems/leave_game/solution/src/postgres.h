#pragma once
#include <vector>
#include <string>
#include <memory>

namespace postgres {

struct Record {
    std::string name;
    int score;
    double play_time;
};

class DatabaseImpl;

class Database {
public:
    explicit Database(const std::string& db_url);
    ~Database();

    void SaveRecord(const std::string& name, int score, long long play_time_ms);
    std::vector<Record> GetRecords(int offset, int limit);

private:
    std::unique_ptr<DatabaseImpl> impl_;
};

} // namespace postgres
