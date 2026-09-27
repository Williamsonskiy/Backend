#pragma once

#include "player_tokens.h"

#include <string>
#include <utility>

namespace app {

class JoinGameUseCase {
public:
    struct Result {
        std::string auth_token;
        Player::Id player_id;
    };

    JoinGameUseCase(Players& players, PlayerTokens& tokens)
        : players_{players}
        , tokens_{tokens} {
    }

    Result Join(model::GameSession& session, std::string user_name) {
        model::Dog& dog = session.AddDog(user_name);
        Player& player = players_.Add(std::move(user_name), session, dog.GetId());
        std::string token = tokens_.AddPlayer(player);

        return Result{
            .auth_token = std::move(token),
            .player_id = player.GetId()
        };
    }

private:
    Players& players_;
    PlayerTokens& tokens_;
};

}  // namespace app
