#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>

#include "../src/tv.h"

namespace Catch {

template <>
struct StringMaker<std::nullopt_t> {
    static std::string convert(std::nullopt_t) {
        return "nullopt";
    }
};

template <typename T>
struct StringMaker<std::optional<T>> {
    static std::string convert(const std::optional<T>& value) {
        if (value) {
            return StringMaker<T>::convert(*value);
        }

        return StringMaker<std::nullopt_t>::convert(std::nullopt);
    }
};

}  // namespace Catch

SCENARIO("TV", "[TV]") {
    GIVEN("A TV") {
        TV tv;

        SECTION("Initially it is off and doesn't show any channel") {
            CHECK_FALSE(tv.IsTurnedOn());
            CHECK(tv.GetChannel() == std::nullopt);
        }

        WHEN("it is turned off") {
            REQUIRE_FALSE(tv.IsTurnedOn());

            THEN("it can't select any channel") {
                CHECK_THROWS_AS(tv.SelectChannel(10), std::logic_error);
                CHECK(tv.GetChannel() == std::nullopt);

                tv.TurnOn();

                CHECK(tv.GetChannel() == 1);
            }

            THEN("it can't select previous channel") {
                CHECK_THROWS_AS(tv.SelectLastViewedChannel(), std::logic_error);
            }
        }

        WHEN("it is turned on first time") {
            tv.TurnOn();

            THEN("it is turned on and shows channel 1") {
                CHECK(tv.IsTurnedOn());
                CHECK(tv.GetChannel() == 1);
            }

            AND_THEN("it can be turned off") {
                tv.TurnOff();

                CHECK_FALSE(tv.IsTurnedOn());
                CHECK(tv.GetChannel() == std::nullopt);
            }

            AND_THEN("it can select channel from 1 to 99") {
                tv.SelectChannel(TV::MIN_CHANNEL);
                CHECK(tv.GetChannel() == TV::MIN_CHANNEL);

                tv.SelectChannel(42);
                CHECK(tv.GetChannel() == 42);

                tv.SelectChannel(TV::MAX_CHANNEL);
                CHECK(tv.GetChannel() == TV::MAX_CHANNEL);
            }

            AND_THEN("it can't select channel out of range") {
                CHECK_THROWS_AS(tv.SelectChannel(TV::MIN_CHANNEL - 1), std::out_of_range);
                CHECK_THROWS_AS(tv.SelectChannel(TV::MAX_CHANNEL + 1), std::out_of_range);
                CHECK(tv.GetChannel() == 1);
            }

            AND_THEN("it remembers selected channel after turning off and on") {
                tv.SelectChannel(8);
                tv.TurnOff();

                CHECK(tv.GetChannel() == std::nullopt);

                tv.TurnOn();

                CHECK(tv.GetChannel() == 8);
            }

            AND_THEN("it can switch to previous selected channel") {
                tv.SelectChannel(8);
                tv.SelectChannel(15);

                CHECK(tv.GetChannel() == 15);

                tv.SelectLastViewedChannel();

                CHECK(tv.GetChannel() == 8);

                tv.SelectLastViewedChannel();

                CHECK(tv.GetChannel() == 15);
            }

            AND_THEN("selecting same channel does not change previous channel") {
                tv.SelectChannel(8);
                tv.SelectChannel(15);
                tv.SelectChannel(15);

                tv.SelectLastViewedChannel();

                CHECK(tv.GetChannel() == 8);
            }
        }
    }
}
