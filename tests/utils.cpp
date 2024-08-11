#include <catch2/catch_amalgamated.hpp>
#include <util.hpp>

TEST_CASE("string format", "[utils]")
{
    std::string str = util::format("Hello %s;%i %i %i", "world!", 1, 20, 3);
    REQUIRE(str == "Hello world!;1 20 3");
}