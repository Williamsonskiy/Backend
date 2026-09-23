#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <string_view>
#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point { Coord x, y; };
struct Size { Dimension width, height; };
struct Rectangle { Point position; Size size; };
struct Offset { Dimension dx, dy; };

struct Point2D { double x, y; };
struct Speed2D { double ux, uy; };

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

    Map(Id id, std::string name, double dog_speed = 1.0) noexcept 
        : id_(std::move(id)), name_(std::move(name)), dog_speed_(dog_speed) {}
        
    const Id& GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }
    const Buildings& GetBuildings() const noexcept { return buildings_; }
    const Roads& GetRoads() const noexcept { return roads_; }
    const Offices& GetOffices() const noexcept { return offices_; }
    double GetDogSpeed() const noexcept { return dog_speed_; }

    void AddRoad(const Road& road) { roads_.emplace_back(road); }
    void AddBuilding(const Building& building) { buildings_.emplace_back(building); }
    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;
    Id id_;
    std::string name_;
    double dog_speed_;
    Roads roads_;
    Buildings buildings_;
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Dog {
public:
    using Id = size_t;
    Dog(Id id, std::string name, Point2D pos) 
        : id_(id), name_(std::move(name)), pos_(pos), speed_({0.0, 0.0}), dir_(Direction::NORTH) {}
        
    Id GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    Point2D GetPosition() const { return pos_; }
    Speed2D GetSpeed() const { return speed_; }
    Direction GetDirection() const { return dir_; }

    void SetSpeed(Speed2D speed) { speed_ = speed; }
    void SetDirection(Direction dir) { dir_ = dir; }

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
};

class GameSession {
public:
    explicit GameSession(const Map* map) : map_(map) {}
    const Map* GetMap() const { return map_; }
    
    Dog* AddDog(const std::string& name);
    
    const std::deque<Dog>& GetDogs() const { return dogs_; }
private:
    Point2D GetRandomRoadPosition() const;

    const Map* map_;
    std::deque<Dog> dogs_;
    size_t dog_id_counter_ = 0;
};

class Game {
public:
    using Maps = std::vector<Map>;
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

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    
    std::deque<GameSession> sessions_;
    MapIdToIndex map_id_to_session_index_;
};

}  // namespace model