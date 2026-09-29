#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <cmath>
#include <sstream>
#include <algorithm>

namespace Catch {
template<>
struct StringMaker<collision_detector::GatheringEvent> {
  static std::string convert(collision_detector::GatheringEvent const& value) {
      std::ostringstream tmp;
      tmp << "(" << value.gatherer_id << "," << value.item_id << "," << value.sq_distance << "," << value.time << ")";
      return tmp.str();
  }
};
}  // namespace Catch

class EventsMatcher : public Catch::Matchers::MatcherBase<std::vector<collision_detector::GatheringEvent>> {
    std::vector<collision_detector::GatheringEvent> expected_;
public:
    explicit EventsMatcher(const std::vector<collision_detector::GatheringEvent>& expected)
        : expected_(expected) {}

    bool match(const std::vector<collision_detector::GatheringEvent>& actual) const override {
        // Компаратор для проверки равенства элементов с учетом погрешности (1e-10)
        auto cmp = [](const collision_detector::GatheringEvent& a, const collision_detector::GatheringEvent& b) {
            const double eps = 1e-10;
            return a.item_id == b.item_id && 
                   a.gatherer_id == b.gatherer_id && 
                   std::abs(a.sq_distance - b.sq_distance) <= eps && 
                   std::abs(a.time - b.time) <= eps;
        };
        // Используем 4-х итераторную версию std::equal (с C++14) для защиты от разных размеров контейнеров
        return std::equal(actual.begin(), actual.end(), expected_.begin(), expected_.end(), cmp);
    }

    std::string describe() const override {
        return "Events match expected within 1e-10 tolerance";
    }
};

class TestProvider : public collision_detector::ItemGathererProvider {
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;

public:
    void AddItem(collision_detector::Item item) { items_.push_back(item); }
    void AddGatherer(collision_detector::Gatherer gatherer) { gatherers_.push_back(gatherer); }

    size_t ItemsCount() const override { return items_.size(); }
    collision_detector::Item GetItem(size_t idx) const override { return items_[idx]; }
    size_t GatherersCount() const override { return gatherers_.size(); }
    collision_detector::Gatherer GetGatherer(size_t idx) const override { return gatherers_[idx]; }
};

SCENARIO("FindGatherEvents functionality", "[FindGatherEvents]") {
    TestProvider provider;

    WHEN("Gatherer does not move") {
        provider.AddGatherer({{0.0, 0.0}, {0.0, 0.0}, 1.0});
        provider.AddItem({{0.0, 0.0}, 1.0});
        
        auto events = collision_detector::FindGatherEvents(provider);
        THEN("No events are detected") {
            REQUIRE(events.empty());
        }
    }
    
    WHEN("Simple Gathering") {
        provider.AddGatherer({{0.0, 0.0}, {10.0, 0.0}, 1.0});
        provider.AddItem({{5.0, 0.0}, 1.0}); 
        
        auto events = collision_detector::FindGatherEvents(provider);
        THEN("One item is collected") {
            std::vector<collision_detector::GatheringEvent> expected = {
                {0u, 0u, 0.0, 0.5}
            };
            REQUIRE_THAT(events, EventsMatcher(expected));
        }
    }
    
    WHEN("Multiple Items on Same Gatherer (sorting)") {
        provider.AddGatherer({{0.0, 0.0}, {10.0, 0.0}, 1.0});
        provider.AddItem({{7.0, 0.0}, 1.0}); 
        provider.AddItem({{3.0, 0.0}, 1.0}); 
        provider.AddItem({{5.0, 0.0}, 1.0}); 
        
        auto events = collision_detector::FindGatherEvents(provider);
        THEN("Events are sorted chronologically") {
            std::vector<collision_detector::GatheringEvent> expected = {
                {1u, 0u, 0.0, 0.3},
                {2u, 0u, 0.0, 0.5},
                {0u, 0u, 0.0, 0.7}
            };
            REQUIRE_THAT(events, EventsMatcher(expected));
        }
    }
    
    WHEN("Multiple Gatherers (sorting across gatherers)") {
        provider.AddGatherer({{0.0, 0.0}, {10.0, 0.0}, 1.0});
        provider.AddGatherer({{0.0, 5.0}, {10.0, 5.0}, 1.0});
        
        provider.AddItem({{5.0, 0.0}, 1.0}); // G0, time 0.5
        provider.AddItem({{3.0, 5.0}, 1.0}); // G1, time 0.3
        provider.AddItem({{7.0, 5.0}, 1.0}); // G1, time 0.7
        provider.AddItem({{4.0, 0.0}, 1.0}); // G0, time 0.4
        
        auto events = collision_detector::FindGatherEvents(provider);
        THEN("Events from multiple gatherers are correctly sorted chronologically") {
            std::vector<collision_detector::GatheringEvent> expected = {
                {1u, 1u, 0.0, 0.3},
                {3u, 0u, 0.0, 0.4},
                {0u, 0u, 0.0, 0.5},
                {2u, 1u, 0.0, 0.7}
            };
            REQUIRE_THAT(events, EventsMatcher(expected));
        }
    }
    
    WHEN("No events (Out of bounds or too far)") {
        provider.AddGatherer({{0.0, 0.0}, {10.0, 0.0}, 1.0});
        provider.AddItem({{5.0, 2.0001}, 1.0});  // слишком далеко (радиус 1 + 1 = 2)
        provider.AddItem({{-0.0001, 0.0}, 1.0}); // проекция сзади
        provider.AddItem({{10.0001, 0.0}, 1.0}); // проекция спереди
        
        auto events = collision_detector::FindGatherEvents(provider);
        THEN("Empty result as items are out of reach") {
            REQUIRE(events.empty());
        }
    }
    
    WHEN("Edge cases (exactly on border)") {
        provider.AddGatherer({{0.0, 0.0}, {10.0, 0.0}, 1.0});
        provider.AddItem({{5.0, 2.0}, 1.0}); 
        provider.AddItem({{0.0, 0.0}, 1.0}); 
        provider.AddItem({{10.0, 0.0}, 1.0}); 
        
        auto events = collision_detector::FindGatherEvents(provider);
        THEN("Items exactly on borders are collected") {
            // Разнесено по разному времени для избежания коллизий сортировки
            std::vector<collision_detector::GatheringEvent> expected = {
                {1u, 0u, 0.0, 0.0},
                {0u, 0u, 4.0, 0.5},
                {2u, 0u, 0.0, 1.0}
            };
            REQUIRE_THAT(events, EventsMatcher(expected));
        }
    }

    WHEN("Same Item collected by multiple gatherers") {
        provider.AddGatherer({{0.0, 0.0}, {10.0, 0.0}, 1.0}); // G0
        provider.AddGatherer({{5.0, 6.0}, {5.0, -4.0}, 1.0}); // G1 (движется вертикально вниз)
        provider.AddItem({{5.0, 0.0}, 1.0}); 
        
        auto events = collision_detector::FindGatherEvents(provider);
        THEN("The same item is registered for both gatherers") {
            std::vector<collision_detector::GatheringEvent> expected = {
                {0u, 0u, 0.0, 0.5},
                {0u, 1u, 0.0, 0.6}
            };
            REQUIRE_THAT(events, EventsMatcher(expected));
        }
    }
}
