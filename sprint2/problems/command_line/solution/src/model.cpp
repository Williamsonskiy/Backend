#include "model.h"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <utility>

namespace model {
using namespace std::literals;

std::string DirectionToString(Direction direction) {
    switch (direction) {
        case Direction::NORTH:
            return "U";
        case Direction::SOUTH:
            return "D";
        case Direction::WEST:
            return "L";
        case Direction::EAST:
            return "R";
    }

    return "U";
}

Road::Road(HorizontalTag, Point start, Coord end_x) noexcept
    : start_{start}
    , end_{end_x, start.y} {
}

Road::Road(VerticalTag, Point start, Coord end_y) noexcept
    : start_{start}
    , end_{start.x, end_y} {
}

bool Road::IsHorizontal() const noexcept {
    return start_.y == end_.y;
}

bool Road::IsVertical() const noexcept {
    return start_.x == end_.x;
}

Point Road::GetStart() const noexcept {
    return start_;
}

Point Road::GetEnd() const noexcept {
    return end_;
}

Building::Building(Rectangle bounds) noexcept
    : bounds_{bounds} {
}

const Rectangle& Building::GetBounds() const noexcept {
    return bounds_;
}

Office::Office(Id id, Point position, Offset offset) noexcept
    : id_{std::move(id)}
    , position_{position}
    , offset_{offset} {
}

const Office::Id& Office::GetId() const noexcept {
    return id_;
}

Point Office::GetPosition() const noexcept {
    return position_;
}

Offset Office::GetOffset() const noexcept {
    return offset_;
}

Map::Map(Id id, std::string name) noexcept
    : id_(std::move(id))
    , name_(std::move(name)) {
}

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

double Map::GetDogSpeed() const noexcept {
    return dog_speed_;
}

void Map::SetDogSpeed(double dog_speed) noexcept {
    dog_speed_ = dog_speed;
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

Dog::Dog(Id id, std::string name, Position position)
    : id_{id}
    , name_{std::move(name)}
    , position_{position} {
}

Dog::Id Dog::GetId() const noexcept {
    return id_;
}

const std::string& Dog::GetName() const noexcept {
    return name_;
}

Position Dog::GetPosition() const noexcept {
    return position_;
}

Speed Dog::GetSpeed() const noexcept {
    return speed_;
}

Direction Dog::GetDirection() const noexcept {
    return direction_;
}

void Dog::SetPosition(Position position) noexcept {
    position_ = position;
}

void Dog::SetSpeed(Speed speed) noexcept {
    speed_ = speed;
}

void Dog::SetDirection(Direction direction) noexcept {
    direction_ = direction;
}

void Dog::Stop() noexcept {
    speed_ = {0.0, 0.0};
}

GameSession::GameSession(const Map& map, bool randomize_spawn_points)
    : map_{map}
    , randomize_spawn_points_{randomize_spawn_points} {
}

Dog& GameSession::AddDog(std::string name) {
    Dog::Id id = next_dog_id_++;
    Position position = GenerateStartPosition();

    auto [it, inserted] = dogs_.emplace(id, Dog{id, std::move(name), position});
    return it->second;
}

Dog* GameSession::FindDog(Dog::Id id) {
    if (auto it = dogs_.find(id); it != dogs_.end()) {
        return &it->second;
    }
    return nullptr;
}

const Map& GameSession::GetMap() const noexcept {
    return map_;
}

const std::unordered_map<Dog::Id, Dog>& GameSession::GetDogs() const noexcept {
    return dogs_;
}

void GameSession::Update(double delta_seconds) {
    for (auto& [dog_id, dog] : dogs_) {
        MoveDog(dog, delta_seconds);
    }
}

Position GameSession::GenerateStartPosition() const {
    if (randomize_spawn_points_) {
        return GenerateRandomPosition();
    }

    const auto& roads = map_.GetRoads();

    if (roads.empty()) {
        return {0.0, 0.0};
    }

    const Point start = roads.front().GetStart();
    return {static_cast<double>(start.x), static_cast<double>(start.y)};
}

Position GameSession::GenerateRandomPosition() const {
    const auto& roads = map_.GetRoads();

    if (roads.empty()) {
        return {0.0, 0.0};
    }

    static thread_local std::mt19937 generator{std::random_device{}()};

    std::uniform_int_distribution<size_t> road_distribution{0, roads.size() - 1};
    const Road& road = roads[road_distribution(generator)];

    const Point start = road.GetStart();
    const Point end = road.GetEnd();

    if (road.IsHorizontal()) {
        const int min_x = std::min(start.x, end.x);
        const int max_x = std::max(start.x, end.x);
        std::uniform_real_distribution<double> x_distribution{
            static_cast<double>(min_x),
            static_cast<double>(max_x)
        };

        return {x_distribution(generator), static_cast<double>(start.y)};
    }

    const int min_y = std::min(start.y, end.y);
    const int max_y = std::max(start.y, end.y);
    std::uniform_real_distribution<double> y_distribution{
        static_cast<double>(min_y),
        static_cast<double>(max_y)
    };

    return {static_cast<double>(start.x), y_distribution(generator)};
}

void GameSession::MoveDog(Dog& dog, double delta_seconds) const {
    const Position current = dog.GetPosition();
    const Speed speed = dog.GetSpeed();

    if (speed.dx == 0.0 && speed.dy == 0.0) {
        return;
    }

    Position target{
        current.x + speed.dx * delta_seconds,
        current.y + speed.dy * delta_seconds
    };

    Position limited = LimitPositionToRoads(current, target);

    dog.SetPosition(limited);

    if (limited.x != target.x || limited.y != target.y) {
        dog.Stop();
    }
}

Position GameSession::LimitPositionToRoads(Position current, Position target) const {
    Position result = target;

    bool found_road = false;

    double min_x = current.x;
    double max_x = current.x;
    double min_y = current.y;
    double max_y = current.y;

    for (const Road& road : map_.GetRoads()) {
        const Point start = road.GetStart();
        const Point end = road.GetEnd();

        if (road.IsHorizontal()) {
            const double road_y = static_cast<double>(start.y);
            const double road_min_x = static_cast<double>(std::min(start.x, end.x)) - ROAD_HALF_WIDTH;
            const double road_max_x = static_cast<double>(std::max(start.x, end.x)) + ROAD_HALF_WIDTH;
            const double road_min_y = road_y - ROAD_HALF_WIDTH;
            const double road_max_y = road_y + ROAD_HALF_WIDTH;

            if (current.x >= road_min_x && current.x <= road_max_x
                && current.y >= road_min_y && current.y <= road_max_y) {
                found_road = true;
                min_x = std::min(min_x, road_min_x);
                max_x = std::max(max_x, road_max_x);
                min_y = std::min(min_y, road_min_y);
                max_y = std::max(max_y, road_max_y);
            }
        } else if (road.IsVertical()) {
            const double road_x = static_cast<double>(start.x);
            const double road_min_x = road_x - ROAD_HALF_WIDTH;
            const double road_max_x = road_x + ROAD_HALF_WIDTH;
            const double road_min_y = static_cast<double>(std::min(start.y, end.y)) - ROAD_HALF_WIDTH;
            const double road_max_y = static_cast<double>(std::max(start.y, end.y)) + ROAD_HALF_WIDTH;

            if (current.x >= road_min_x && current.x <= road_max_x
                && current.y >= road_min_y && current.y <= road_max_y) {
                found_road = true;
                min_x = std::min(min_x, road_min_x);
                max_x = std::max(max_x, road_max_x);
                min_y = std::min(min_y, road_min_y);
                max_y = std::max(max_y, road_max_y);
            }
        }
    }

    if (!found_road) {
        return current;
    }

    result.x = std::clamp(result.x, min_x, max_x);
    result.y = std::clamp(result.y, min_y, max_y);

    return result;
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Game::Maps& Game::GetMaps() const noexcept {
    return maps_;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

void Game::SetRandomizeSpawnPoints(bool value) noexcept {
    randomize_spawn_points_ = value;
}

GameSession& Game::FindOrCreateSession(const Map& map) {
    auto it = std::find_if(sessions_.begin(), sessions_.end(),
        [&map](const GameSession& session) {
            return &session.GetMap() == &map;
        });

    if (it != sessions_.end()) {
        return *it;
    }

    sessions_.emplace_back(map, randomize_spawn_points_);
    return sessions_.back();
}

void Game::Update(double delta_seconds) {
    for (auto& session : sessions_) {
        session.Update(delta_seconds);
    }
}

}  // namespace model
