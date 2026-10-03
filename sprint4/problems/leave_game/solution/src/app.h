#pragma once
#include "model.h"
#include "postgres.h"
#include <boost/json.hpp>
#include <random>
#include <string>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <utility>
#include <functional>

namespace app {

using Token = std::string;

struct MapExtraData {
    boost::json::array loot_types;
};

class PlayerTokens {
public:
    Token GenerateToken() {
        std::stringstream ss;
        ss << std::hex << std::setfill('0')
           << std::setw(16) << generator1_()
           << std::setw(16) << generator2_();
        return ss.str();
    }
private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] { return random_device_(); }()};
    std::mt19937_64 generator2_{[this] { return random_device_(); }()};
};

class Player {
public:
    Player(model::GameSession* session, model::Dog* dog)
        : session_(session), dog_(dog) {}

    model::Dog::Id GetId() const { return dog_->GetId(); }
    const std::string& GetName() const { return dog_->GetName(); }
    model::GameSession* GetSession() const { return session_; }
    model::Dog* GetDog() const { return dog_; }

private:
    model::GameSession* session_;
    model::Dog* dog_;
};

class App {
public:
    using SaveStateCallback = std::function<void()>;

    explicit App(model::Game& game, bool auto_tick, std::unordered_map<std::string, MapExtraData> extra_data = {}) 
        : game_(game), auto_tick_(auto_tick), extra_data_(std::move(extra_data)) {}

    bool IsAutoTick() const { return auto_tick_; }

    void SetDatabase(std::shared_ptr<postgres::Database> db) { db_ = std::move(db); }
    std::shared_ptr<postgres::Database> GetDatabase() const { return db_; }

    const MapExtraData* GetMapExtraData(const std::string& map_id) const {
        if (auto it = extra_data_.find(map_id); it != extra_data_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    std::pair<Token, model::Dog::Id> JoinGame(const std::string& player_name, const model::Map::Id& map_id) {
        auto* session = game_.GetSession(map_id);
        if (!session) {
            session = game_.AddSession(map_id);
        }
        
        auto* dog = session->AddDog(player_name);
        model::Dog::Id player_id = dog->GetId();
        
        auto player = std::make_unique<Player>(session, dog);
        Token token = tokens_.GenerateToken();
        player_tokens_[token] = std::move(player);
        
        return {token, player_id};
    }

    Player* GetPlayerByToken(const Token& token) const {
        if (auto it = player_tokens_.find(token); it != player_tokens_.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    const std::unordered_map<Token, std::unique_ptr<Player>>& GetTokens() const {
        return player_tokens_;
    }

    model::Game& GetGameMutable() { return game_; }
    const model::Game& GetGame() const { return game_; }

    void RestoreToken(const std::string& token, const model::Map::Id& map_id, size_t dog_id) {
        auto* session = game_.GetSession(map_id);
        if (!session) return;
        auto* dog = session->GetDogById(dog_id);
        if (!dog) return;
        
        auto player = std::make_unique<Player>(session, dog);
        player_tokens_[token] = std::move(player);
    }

    void SetSaveStateCallback(SaveStateCallback cb) {
        save_state_callback_ = std::move(cb);
    }

    void SetSavePeriod(int ms) {
        save_period_ = std::chrono::milliseconds(ms);
    }

    void Tick(std::chrono::milliseconds delta) {
        game_.Tick(delta);

        double retirement_ms = game_.GetDogRetirementTime() * 1000.0;
        std::vector<Token> retired_tokens;
        
        for (const auto& [token, player] : player_tokens_) {
            if (player->GetDog()->GetIdleTime().count() >= retirement_ms) {
                retired_tokens.push_back(token);
            }
        }

        for (const auto& token : retired_tokens) {
            auto* player = player_tokens_[token].get();
            auto* dog = player->GetDog();
            
            if (db_) {
                db_->SaveRecord(dog->GetName(), dog->GetScore(), dog->GetPlayTime().count());
            }
            
            player->GetSession()->RemoveDog(dog->GetId());
            player_tokens_.erase(token);
        }

        if (save_period_.count() > 0) {
            time_since_save_ += delta;
            if (time_since_save_ >= save_period_) {
                if (save_state_callback_) {
                    save_state_callback_();
                }
                time_since_save_ = std::chrono::milliseconds{0};
            }
        }
    }

private:
    model::Game& game_;
    bool auto_tick_; 
    PlayerTokens tokens_;
    std::unordered_map<Token, std::unique_ptr<Player>> player_tokens_;
    std::unordered_map<std::string, MapExtraData> extra_data_;
    std::shared_ptr<postgres::Database> db_;

    SaveStateCallback save_state_callback_;
    std::chrono::milliseconds save_period_{0};
    std::chrono::milliseconds time_since_save_{0};
};

} // namespace app
