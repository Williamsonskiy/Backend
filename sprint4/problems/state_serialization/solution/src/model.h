--- START OF FILE src/model.h ---
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <string_view>
#include <chrono>
#include <cmath>
#include <map>
#include "tagged.h"
#include "loot_generator.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point { Coord x, y; };
struct Size { Dimension width, height; };
struct Rectangle { Point position; Size size; };
struct Offset { Dimension dx, dy; };

struct Point2D { double x, y; };
struct Speed2D { double ux, uy; };

struct LostObject {
    size_t id;
    size_t type;
    Point2D pos;
};

struct FoundObject {
    size_t id;
    size_t type;
};

enum class Direction { NORTH, SOUTH, WEST, EAST };

constexpr std::string_view DirectionToString(Direction dir) {
    switch (dir) {
        case Direction::NORTH: return "U";
        case Direction::SOUTH: return "D";
        case Direction::WEST:  return "L";
        case Direction::EAST:  return "R";
    }
    return "U";
}

class Road {
    struct HorizontalTag { explicit HorizontalTag() = default; };
    struct VerticalTag { explicit VerticalTag() = default; };
public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};
    Road(HorizontalTag, Point start, Coord end_x) noexcept : start_{start}, end_{end_x, start.y} {}
    Road(VerticalTag, Point start, Coord end_y) noexcept : start_{start}, end_{start.x, end_y} {}
    bool IsHorizontal() const noexcept { return start_.y == end_.y; }
    bool IsVertical() const noexcept { return start_.x == end_.x; }
    Point GetStart() const noexcept { return start_; }
    Point GetEnd() const noexcept { return end_; }
private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept : bounds_{bounds} {}
    const Rectangle& GetBounds() const noexcept { return bounds_; }
private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;
    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}, position_{position}, offset_{offset} {}
    const Id& GetId() const noexcept { return id_; }
    Point GetPosition() const noexcept { return position_; }
    Offset GetOffset() const noexcept { return offset_; }
private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name, double dog_speed = 1.0, size_t num_loot_types = 0, size_t bag_capacity = 3) noexcept 
        : id_(std::move(id)), name_(std::move(name)), dog_speed_(dog_speed), num_loot_types_(num_loot_types), bag_capacity_(bag_capacity) {}
        
    const Id& GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }
    const Buildings& GetBuildings() const noexcept { return buildings_; }
    const Roads& GetRoads() const noexcept { return roads_; }
    const Offices& GetOffices() const noexcept { return offices_; }
    double GetDogSpeed() const noexcept { return dog_speed_; }
    size_t GetNumLootTypes() const noexcept { return num_loot_types_; }
    size_t GetBagCapacity() const noexcept { return bag_capacity_; }

    void AddLootValue(size_t value) { loot_values_.push_back(value); }
    size_t GetLootValue(size_t type) const { return type < loot_values_.size() ? loot_values_[type] : 0; }

    void AddRoad(const Road& road);
    void AddBuilding(const Building& building) { buildings_.emplace_back(building); }
    void AddOffice(Office office);

    void GetRoadBounds(const Road& road, double& min_x, double& max_x, double& min_y, double& max_y) const;
    bool IsPointInRoad(Point2D p, const Road& road) const;
    std::vector<const Road*> GetRoadsContaining(Point2D p) const;

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;
    Id id_;
    std::string name_;
    double dog_speed_;
    size_t num_loot_types_;
    size_t bag_capacity_;
    std::vector<size_t> loot_values_;
    Roads roads_;
    Buildings buildings_;
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    std::unordered_map<int, std::vector<size_t>> horizontal_roads_;
    std::unordered_map<int, std::vector<size_t>> vertical_roads_;
};

class Dog {
public:
    using Id = size_t;
    Dog(Id id, std::string name, Point2D pos) 
        : id_(id), name_(std::move(name)), pos_(pos), speed_({0.0, 0.0}), dir_(Direction::NORTH), score_(0) {}
        
    Id GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    Point2D GetPosition() const { return pos_; }
    Speed2D GetSpeed() const { return speed_; }
    Direction GetDirection() const { return dir_; }
    const std::vector<FoundObject>& GetBag() const { return bag_; }
    size_t GetScore() const { return score_; }

    void SetPosition(Point2D pos) { pos_ = pos; }
    void SetSpeed(Speed2D speed) { speed_ = speed; }
    void SetDirection(Direction dir) { dir_ = dir; }
    
    void PutToBag(FoundObject item) { bag_.push_back(item); }
    void EmptyBag() { bag_.clear(); }
    void AddScore(size_t points) { score_ += points; }

    void Move(std::string_view move_cmd, double speed) {
        if (move_cmd == "U") {
            dir_ = Direction::NORTH;
            speed_ = {0.0, -speed};
        } else if (move_cmd == "D") {
            dir_ = Direction::SOUTH;
            speed_ = {0.0, speed};
        } else if (move_cmd == "L") {
            dir_ = Direction::WEST;
            speed_ = {-speed, 0.0};
        } else if (move_cmd == "R") {
            dir_ = Direction::EAST;
            speed_ = {speed, 0.0};
        } else if (move_cmd == "") {
            speed_ = {0.0, 0.0};
        }
    }

private:
    Id id_;
    std::string name_;
    Point2D pos_;
    Speed2D speed_;
    Direction dir_;
    std::vector<FoundObject> bag_;
    size_t score_;
};

class GameSession {
public:
    explicit GameSession(const Map* map, bool random_spawn, double loot_period, double loot_probability) 
        : map_(map), random_spawn_(random_spawn),
          loot_generator_(std::chrono::milliseconds(static_cast<int>((loot_period > 0 ? loot_period : 1.0) * 1000)), loot_probability) {}

    const Map* GetMap() const { return map_; }
    
    Dog* AddDog(const std::string& name);
    void Tick(std::chrono::milliseconds delta);
    
    const std::deque<Dog>& GetDogs() const { return dogs_; }
    const std::map<size_t, LostObject>& GetLostObjects() const { return lost_objects_; }

    Dog* GetDogById(size_t id) {
        for (auto& dog : dogs_) {
            if (dog.GetId() == id) return &dog;
        }
        return nullptr;
    }

    void LoadState(std::deque<Dog> dogs, std::map<size_t, LostObject> lost_objects) {
        dogs_ = std::move(dogs);
        lost_objects_ = std::move(lost_objects);
        for (const auto& dog : dogs_) {
            if (dog.GetId() >= dog_id_counter_) {
                dog_id_counter_ = dog.GetId() + 1;
            }
        }
        for (const auto& [id, obj] : lost_objects_) {
            if (id >= lost_object_id_counter_) {
                lost_object_id_counter_ = id + 1;
            }
        }
    }

private:
    Point2D GetSpawnPosition() const;
    Point2D GetRandomRoadPosition() const;

    const Map* map_;
    bool random_spawn_;
    std::deque<Dog> dogs_;
    size_t dog_id_counter_ = 0;

    loot_gen::LootGenerator loot_generator_;
    std::map<size_t, LostObject> lost_objects_;
    size_t lost_object_id_counter_ = 0;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void SetRandomizedSpawn(bool random_spawn) { random_spawn_ = random_spawn; }
    void SetLootParameters(double period, double probability) {
        loot_period_ = period;
        loot_probability_ = probability;
    }

    void AddMap(Map map);
    const Maps& GetMaps() const noexcept { return maps_; }
    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }
    GameSession* GetSession(const Map::Id& map_id);
    GameSession* AddSession(const Map::Id& map_id);
    
    void Tick(std::chrono::milliseconds delta);

    void LoadSession(const Map::Id& map_id, std::deque<Dog> dogs, std::map<size_t, LostObject> lost_objects) {
        auto* session = GetSession(map_id);
        if (!session) {
            session = AddSession(map_id);
        }
        if (session) {
            session->LoadState(std::move(dogs), std::move(lost_objects));
        }
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    bool random_spawn_ = false;
    double loot_period_ = 5.0;
    double loot_probability_ = 0.5;
    
    std::deque<GameSession> sessions_;
    MapIdToIndex map_id_to_session_index_;
};

}  // namespace model
--- END OF FILE ---
