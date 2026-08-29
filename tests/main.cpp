#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

// Catch2WithMain is linked, so no CATCH_CONFIG_MAIN here.
int main(int argc, char* argv[])
{
    return Catch::Session().run(argc, argv);
}
