template <typename Request, typename Send>
    void HandleJoinGame(Request&& req, Send&& send) {
        if (req.method() != http::verb::post) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", req, "POST"));
        }

        std::string user_name;
        std::string map_id_str;
        try {
            json::value jv = json::parse(req.body());
            if (!jv.is_object()) {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", req));
            }
            const auto& obj = jv.as_object();
            if (!obj.contains("userName") || !obj.contains("mapId")) {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", req));
            }
            user_name = json::value_to<std::string>(obj.at("userName"));
            map_id_str = json::value_to<std::string>(obj.at("mapId"));
        } catch (...) {
            return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", req));
        }

        if (user_name.empty()) {
            return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid name", req));
        }

        model::Map::Id map_id{map_id_str};
        if (!app_.GetGame().FindMap(map_id)) {
            return send(MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found", req));
        }

        auto [token, player_id] = app_.JoinGame(user_name, map_id);
        json::object response_obj = {
            {"authToken", token},
            {"playerId", *player_id}
        };

        send(MakeJsonResponse(http::status::ok, json::serialize(response_obj), req));
    }

    template <typename Request, typename Send>
    void HandleGetPlayers(Request&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req, "GET, HEAD"));
        }

        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is missing", req));
        }

        std::string_view auth_header = auth_it->value();
        if (!auth_header.starts_with("Bearer ") || auth_header.size() != 39) {
            return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is missing", req));
        }

        std::string token_str(auth_header.substr(7));
        app::Player* player = app_.GetPlayerByToken(token_str);
        if (!player) {
            return send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", req));
        }

        json::object players_obj;
        for (const auto& dog : player->GetSession()->GetDogs()) {
            players_obj[std::to_string(*dog.GetId())] = json::object{{"name", dog.GetName()}};
        }

        auto res = MakeJsonResponse(http::status::ok, json::serialize(players_obj), req);
        if (req.method() == http::verb::head) {
            res.body().clear();
            res.content_length(json::serialize(players_obj).size());
        }
        send(std::move(res));
    }
