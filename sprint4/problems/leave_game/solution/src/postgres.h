#pragma once
#include <pqxx/pqxx>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>

namespace postgres {

class ConnectionPool {
public:
    ConnectionPool(size_t capacity, const std::string& connection_string)
        : capacity_{capacity}, connection_string_{connection_string} {
        for (size_t i = 0; i < capacity_; ++i) {
            pool_.emplace_back(std::make_unique<pqxx::connection>(connection_string_));
        }
    }

    struct ConnectionWrapper {
        std::unique_ptr<pqxx::connection> conn;
        ConnectionPool* pool;

        ~ConnectionWrapper() {
            if (conn && pool) {
                pool->ReturnConnection(std::move(conn));
            }
        }
        pqxx::connection* operator->() { return conn.get(); }
        pqxx::connection& operator*() { return *conn; }
    };

    ConnectionWrapper GetConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !pool_.empty(); });
        auto conn = std::move(pool_.back());
        pool_.pop_back();
        return {std::move(conn), this};
    }

private:
    void ReturnConnection(std::unique_ptr<pqxx::connection> conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push_back(std::move(conn));
        cond_var_.notify_one();
    }

    size_t capacity_;
    std::string connection_string_;
    std::vector<std::unique_ptr<pqxx::connection>> pool_;
    std::mutex mutex_;
    std::condition_variable cond_var_;
};

struct Record {
    std::string name;
    int score;
    double play_time;
};

class Database {
public:
    Database(const std::string& db_url) : pool_(4, db_url) {
        auto conn = pool_.GetConnection();
        pqxx::work w(*conn);
        w.exec(
            "CREATE TABLE IF NOT EXISTS retired_players ("
            "id SERIAL PRIMARY KEY, "
            "name VARCHAR(100) NOT NULL, "
            "score INTEGER NOT NULL, "
            "play_time_ms BIGINT NOT NULL);"
        );
        w.exec("CREATE INDEX IF NOT EXISTS retired_players_sort_idx ON retired_players (score DESC, play_time_ms ASC, name ASC);");
        w.commit();
    }

    void SaveRecord(const std::string& name, int score, long long play_time_ms) {
        auto conn = pool_.GetConnection();
        pqxx::work w(*conn);
        w.exec_params(
            "INSERT INTO retired_players (name, score, play_time_ms) VALUES ($1, $2, $3);",
            name, score, play_time_ms
        );
        w.commit();
    }

    std::vector<Record> GetRecords(int offset, int limit) {
        auto conn = pool_.GetConnection();
        pqxx::read_transaction r(*conn);
        auto result = r.exec_params(
            "SELECT name, score, play_time_ms FROM retired_players "
            "ORDER BY score DESC, play_time_ms ASC, name ASC "
            "LIMIT $1 OFFSET $2;",
            limit, offset
        );

        std::vector<Record> records;
        records.reserve(result.size());
        for (const auto& row : result) {
            records.push_back({
                row["name"].as<std::string>(),
                row["score"].as<int>(),
                row["play_time_ms"].as<long long>() / 1000.0
            });
        }
        return records;
    }

private:
    ConnectionPool pool_;
};

} // namespace postgres
