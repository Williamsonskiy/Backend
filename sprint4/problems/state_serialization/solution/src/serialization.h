#pragma once
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>
#include "app.h"

namespace model {
template <typename Archive>
void serialize(Archive& ar, FoundObject& obj, [[maybe_unused]] const unsigned version) {
    ar & obj.id;
    ar & obj.type;
}
template <typename Archive>
void serialize(Archive& ar, LostObject& obj, [[maybe_unused]] const unsigned version) {
    ar & obj.id;
    ar & obj.type;
    ar & obj.pos.x;
    ar & obj.pos.y;
}
template <typename Archive>
void serialize(Archive& ar, Point2D& pt, [[maybe_unused]] const unsigned version) {
    ar & pt.x & pt.y;
}
template <typename Archive>
void serialize(Archive& ar, Speed2D& sp, [[maybe_unused]] const unsigned version) {
    ar & sp.ux & sp.uy;
}
} // namespace model

namespace serialization {

class DogRepr {
public:
    DogRepr() = default;
    explicit DogRepr(const model::Dog& dog)
        : id_(dog.GetId())
        , name_(dog.GetName())
        , pos_(dog.GetPosition())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirection())
        , score_(dog.GetScore())
        , bag_content_(dog.GetBag()) {
    }
    
    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog{id_, name_, pos_};
        dog.SetSpeed(speed_);
        dog.SetDirection(direction_);
        dog.AddScore(score_);
        for (const auto& item : bag_content_) {
            dog.PutToBag(item);
        }
        return dog;
    }
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & id_;
        ar & name_;
        ar & pos_;
        ar & speed_;
        int dir = static_cast<int>(direction_);
        ar & dir;
        direction_ = static_cast<model::Direction>(dir);
        ar & score_;
        ar & bag_content_;
    }
private:
    model::Dog::Id id_ = 0;
    std::string name_;
    model::Point2D pos_{0.0, 0.0};
    model::Speed2D speed_{0.0, 0.0};
    model::Direction direction_ = model::Direction::NORTH;
    size_t score_ = 0;
    std::vector<model::FoundObject> bag_content_;
};

class GameSessionRepr {
public:
    GameSessionRepr() = default;
    explicit GameSessionRepr(const model::GameSession& session) {
        for (const auto& dog : session.GetDogs()) {
            dogs_.emplace_back(DogRepr(dog));
        }
        for (const auto& [id, lo] : session.GetLostObjects()) {
            lost_objects_.push_back(lo);
        }
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & dogs_;
        ar & lost_objects_;
    }

    std::vector<DogRepr> dogs_;
    std::vector<model::LostObject> lost_objects_;
};

class TokenPlayerRepr {
public:
    TokenPlayerRepr() = default;
    TokenPlayerRepr(const std::string& token, const std::string& map_id, size_t dog_id)
        : token_(token), map_id_(map_id), dog_id_(dog_id) {}
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & token_ & map_id_ & dog_id_;
    }
    
    std::string token_;
    std::string map_id_;
    size_t dog_id_ = 0;
};

class AppRepr {
public:
    AppRepr() = default;
    explicit AppRepr(app::App& app) {
        for (const auto& map : app.GetGame().GetMaps()) {
            if (auto* session = app.GetGameMutable().GetSession(map.GetId())) {
                sessions_[*map.GetId()] = GameSessionRepr(*session);
            }
        }
        for (const auto& [token, player] : app.GetTokens()) {
            tokens_.emplace_back(token, *player->GetSession()->GetMap()->GetId(), player->GetDog()->GetId());
        }
    }

    void Restore(app::App& app) const {
        for (const auto& [map_id_str, session_repr] : sessions_) {
            model::Map::Id map_id{map_id_str};
            std::deque<model::Dog> dogs;
            for (const auto& dog_repr : session_repr.dogs_) {
                dogs.push_back(dog_repr.Restore());
            }
            std::map<size_t, model::LostObject> lost_objects;
            for (const auto& lo : session_repr.lost_objects_) {
                lost_objects[lo.id] = lo;
            }
            app.GetGameMutable().LoadSession(map_id, std::move(dogs), std::move(lost_objects));
        }
        for (const auto& tp_repr : tokens_) {
            app.RestoreToken(tp_repr.token_, model::Map::Id{tp_repr.map_id_}, tp_repr.dog_id_);
        }
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & sessions_;
        ar & tokens_;
    }

    std::map<std::string, GameSessionRepr> sessions_;
    std::vector<TokenPlayerRepr> tokens_;
};

inline void SaveState(app::App& app, const std::filesystem::path& state_file) {
    try {
        AppRepr repr(app);
        std::filesystem::path temp_file = state_file;
        temp_file += ".tmp";
        {
            std::ofstream out(temp_file);
            boost::archive::text_oarchive oa(out);
            oa << repr;
        }
        std::filesystem::rename(temp_file, state_file);
    } catch (const std::exception& e) {
        std::cerr << "Failed to save state: " << e.what() << std::endl;
    }
}

inline void LoadState(app::App& app, const std::filesystem::path& state_file) {
    AppRepr repr;
    {
        std::ifstream in(state_file);
        if (!in.is_open()) {
            throw std::runtime_error("Failed to open state file for reading");
        }
        boost::archive::text_iarchive ia(in);
        ia >> repr;
    }
    repr.Restore(app);
}

} // namespace serialization
