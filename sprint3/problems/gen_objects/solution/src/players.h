#pragma once

#include "model.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace app {

class Player {
public:
    using Id = std::uint32_t;

    Player(Id id, std::string name, model::GameSession& session, model::Dog::Id dog_id)
        : id_{id}
        , name_{std::move(name)}
        , session_{session}
        , dog_id_{dog_id} {
    }

    Id GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    model::GameSession& GetSession() const noexcept {
        return session_;
    }

    model::Dog::Id GetDogId() const noexcept {
        return dog_id_;
    }

private:
    Id id_;
    std::string name_;
    model::GameSession& session_;
    model::Dog::Id dog_id_;
};

class Players {
public:
    Player& Add(std::string name, model::GameSession& session, model::Dog::Id dog_id) {
        Player::Id id = next_player_id_++;
        auto [it, inserted] = players_.emplace(id, Player{id, std::move(name), session, dog_id});
        return it->second;
    }

    Player* FindById(Player::Id id) {
        if (auto it = players_.find(id); it != players_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    std::vector<Player*> FindBySession(model::GameSession& session) {
        std::vector<Player*> result;

        for (auto& [id, player] : players_) {
            if (&player.GetSession() == &session) {
                result.push_back(&player);
            }
        }

        return result;
    }

private:
    Player::Id next_player_id_ = 0;
    std::unordered_map<Player::Id, Player> players_;
};

}  // namespace app
