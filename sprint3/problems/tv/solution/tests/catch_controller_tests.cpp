#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <string_view>

#include "../src/controller.h"

SCENARIO("Controller", "[Controller]") {
    using namespace std::literals;

    GIVEN("Controller, TV and Menu") {
        TV tv;
        std::istringstream input;
        std::ostringstream output;
        Menu menu{input, output};
        Controller controller{tv, menu};

        auto run_menu_command = [&menu, &input](std::string command) {
            input.str(std::move(command));
            input.clear();
            menu.Run();
        };

        auto expect_output = [&output](std::string_view expected) {
            CHECK(output.str() == std::string{expected});
        };

        auto expect_empty_output = [&expect_output] {
            expect_output(""sv);
        };

        auto expect_extra_arguments_error = [&expect_output](std::string_view command) {
            expect_output(
                "Error: the "s.append(command).append(" command does not require any arguments\n"sv)
            );
        };

        WHEN("TV is turned off") {
            REQUIRE_FALSE(tv.IsTurnedOn());

            AND_WHEN("Info command is entered") {
                run_menu_command("Info"s);

                THEN("it prints that TV is off") {
                    expect_output("TV is turned off\n"sv);
                }
            }

            AND_WHEN("Info command has extra arguments") {
                run_menu_command("Info extra"s);

                THEN("it prints error") {
                    expect_extra_arguments_error("Info"sv);
                    CHECK_FALSE(tv.IsTurnedOn());
                }
            }

            AND_WHEN("TurnOn command is entered") {
                run_menu_command("TurnOn"s);

                THEN("TV is turned on") {
                    CHECK(tv.IsTurnedOn());
                    expect_empty_output();
                }
            }

            AND_WHEN("TurnOn command has extra arguments") {
                run_menu_command("TurnOn extra"s);

                THEN("TV is not turned on and error is printed") {
                    CHECK_FALSE(tv.IsTurnedOn());
                    expect_extra_arguments_error("TurnOn"sv);
                }
            }

            AND_WHEN("TurnOff command is entered") {
                run_menu_command("TurnOff"s);

                THEN("TV stays off") {
                    CHECK_FALSE(tv.IsTurnedOn());
                    expect_empty_output();
                }
            }

            AND_WHEN("SelectChannel command is entered") {
                run_menu_command("SelectChannel 8"s);

                THEN("it prints that TV is off") {
                    CHECK_FALSE(tv.IsTurnedOn());
                    expect_output("TV is turned off\n"sv);
                }
            }

            AND_WHEN("SelectChannel has invalid argument") {
                run_menu_command("SelectChannel abc"s);

                THEN("it prints invalid channel") {
                    expect_output("Invalid channel\n"sv);
                }
            }

            AND_WHEN("SelectPreviousChannel command is entered") {
                run_menu_command("SelectPreviousChannel"s);

                THEN("it prints that TV is off") {
                    expect_output("TV is turned off\n"sv);
                }
            }
        }

        WHEN("TV is turned on") {
            tv.TurnOn();
            REQUIRE(tv.IsTurnedOn());

            AND_WHEN("Info command is entered") {
                tv.SelectChannel(42);
                run_menu_command("Info"s);

                THEN("it prints current channel") {
                    expect_output("TV is turned on\nChannel number is 42\n"sv);
                }
            }

            AND_WHEN("TurnOff command is entered") {
                run_menu_command("TurnOff"s);

                THEN("TV is turned off") {
                    CHECK_FALSE(tv.IsTurnedOn());
                    expect_empty_output();
                }
            }

            AND_WHEN("TurnOff command has extra arguments") {
                run_menu_command("TurnOff extra"s);

                THEN("TV is still on and error is printed") {
                    CHECK(tv.IsTurnedOn());
                    expect_extra_arguments_error("TurnOff"sv);
                }
            }

            AND_WHEN("SelectChannel command is entered with valid channel") {
                run_menu_command("SelectChannel 8"s);

                THEN("channel is selected") {
                    CHECK(tv.GetChannel() == 8);
                    expect_empty_output();
                }
            }

            AND_WHEN("SelectChannel command is entered with too small channel") {
                run_menu_command("SelectChannel 0"s);

                THEN("it prints range error") {
                    CHECK(tv.GetChannel() == 1);
                    expect_output("Channel is out of range\n"sv);
                }
            }

            AND_WHEN("SelectChannel command is entered with too large channel") {
                run_menu_command("SelectChannel 100"s);

                THEN("it prints range error") {
                    CHECK(tv.GetChannel() == 1);
                    expect_output("Channel is out of range\n"sv);
                }
            }

            AND_WHEN("SelectPreviousChannel command is entered") {
                tv.SelectChannel(8);
                tv.SelectChannel(15);

                run_menu_command("SelectPreviousChannel"s);

                THEN("previous channel is selected") {
                    CHECK(tv.GetChannel() == 8);
                    expect_empty_output();
                }
            }
        }
    }
}
