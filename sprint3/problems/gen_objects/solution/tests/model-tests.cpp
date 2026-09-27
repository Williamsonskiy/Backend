#include <catch2/catch_test_macros.hpp>

#include "../src/model.h"
#include "../src/loot_generator.h"

using namespace std::literals;

SCENARIO("Lost objects generation") {
    GIVEN("a game session with one road and one dog") {
        model::Map map{model::Map::Id{"map1"}, "Map 1"};
        map.AddRoad(model::Road{model::Road::HORIZONTAL, {0, 0}, 10});
        map.SetLootTypesCount(3);

        model::GameSession session{map, false};
        session.AddDog("dog");

        loot_gen::LootGenerator generator{1s, 1.0};

        WHEN("time passes") {
            session.GenerateLoot(1s, generator);

            THEN("lost object appears") {
                REQUIRE(session.GetLostObjects().size() == 1);

                const auto& object = session.GetLostObjects().begin()->second;

                CHECK(object.type < 3);
                CHECK(object.position.y == 0.0);
                CHECK(object.position.x >= 0.0);
                CHECK(object.position.x <= 10.0);
            }
        }
    }

    GIVEN("a game session without dogs") {
        model::Map map{model::Map::Id{"map1"}, "Map 1"};
        map.AddRoad(model::Road{model::Road::HORIZONTAL, {0, 0}, 10});
        map.SetLootTypesCount(3);

        model::GameSession session{map, false};

        loot_gen::LootGenerator generator{1s, 1.0};

        WHEN("time passes") {
            session.GenerateLoot(1s, generator);

            THEN("lost objects do not appear") {
                CHECK(session.GetLostObjects().empty());
            }
        }
    }
}
