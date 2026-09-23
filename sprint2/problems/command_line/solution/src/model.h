#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct Position {
    double x = 0.0;
    double y = 0.0;
};

struct Speed {
    double dx = 0.0;
    double dy = 0.0;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

std::string DirectionToString(Direction direction);

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept;
    Road(VerticalTag, Point start, Coord end_y) noexcept;

    bool IsHorizontal() const noexcept;
    bool IsVertical() const noexcept;

    Point GetStart() const noexcept;
    Point GetEnd() const noexcept;

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept;

    const Rectangle& GetBounds() const noexcept;

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept;

    const Id& GetId() const noexcept;
    Point GetPosition() const noexcept;
    Offset GetOffset() const noexcept;

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

    Map(Id id, std::string name) noexcept;

    const Id& GetId() const noexcept;
    const std::string& GetName() const noexcept;

    const Buildings& GetBuildings() const noexcept;
    const Roads& GetRoads() const noexcept;
    const Offices& GetOffices() const noexcept;

    double GetDogSpeed() const noexcept;
    void SetDogSpeed(double dog_speed) noexcept;

    void AddRoad(const Road& road);
    void AddBuilding(const Building& building);
    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    double dog_speed_ = 1.0;
};

class Dog {
public:
    using Id = std::uint32_t;

    Dog(Id id, std::string name, Position position);

    Id GetId() const noexcept;
    const std::string& GetName() const noexcept;

    Position GetPosition() const noexcept;
    Speed GetSpeed() const noexcept;
    Direction GetDirection() const noexcept;

    void SetPosition(Position position) noexcept;
    void SetSpeed(Speed speed) noexcept;
    void SetDirection(Direction direction) noexcept;
    void Stop() noexcept;

private:
    Id id_;
    std::string name_;
    Position position_;
    Speed speed_{0.0, 0.0};
    Direction direction_ = Direction::NORTH;
};

class GameSession {
public:
    GameSession(const Map& map, bool randomize_spawn_points);

    Dog& AddDog(std::string name);
    Dog* FindDog(Dog::Id id);

    const Map& GetMap() const noexcept;
    const std::unordered_map<Dog::Id, Dog>& GetDogs() const noexcept;

    void Update(double delta_seconds);

private:
    static constexpr double ROAD_HALF_WIDTH = 0.4;

    Position GenerateStartPosition() const;
    Position GenerateRandomPosition() const;

    void MoveDog(Dog& dog, double delta_seconds) const;
    Position LimitPositionToRoads(Position current, Position target) const;

    const Map& map_;
    bool randomize_spawn_points_ = false;
    Dog::Id next_dog_id_ = 0;
    std::unordered_map<Dog::Id, Dog> dogs_;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept;
    const Map* FindMap(const Map::Id& id) const noexcept;

    void SetRandomizeSpawnPoints(bool value) noexcept;

    GameSession& FindOrCreateSession(const Map& map);

    void Update(double delta_seconds);

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    std::vector<GameSession> sessions_;
    bool randomize_spawn_points_ = false;
};

}  // namespace model
