#pragma once
#include "model.h"
#include <random>
#include <string>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <iomanip>

namespace app {

using Token = std::string;

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
    explicit App(model::Game& game) : game_(game) {}

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

    const model::Game& GetGame() const { return game_; }

private:
    model::Game& game_;
    PlayerTokens tokens_;
    std::unordered_map<Token, std::unique_ptr<Player>> player_tokens_;
};

} // namespace app