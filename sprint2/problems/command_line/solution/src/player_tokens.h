#pragma once

#include "players.h"

#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>

namespace app {

class PlayerTokens {
public:
    using Token = std::string;

    Token AddPlayer(Player& player) {
        Token token = GenerateToken();
        token_to_player_[token] = &player;
        return token;
    }

    Player* FindPlayerByToken(const Token& token) const {
        if (auto it = token_to_player_.find(token); it != token_to_player_.end()) {
            return it->second;
        }
        return nullptr;
    }

private:
    Token GenerateToken() {
        std::ostringstream out;

        out << std::hex << std::setfill('0')
            << std::setw(16) << generator1_()
            << std::setw(16) << generator2_();

        return out.str();
    }

    std::random_device random_device_;

    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};

    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};

    std::unordered_map<Token, Player*> token_to_player_;
};

}  // namespace app
