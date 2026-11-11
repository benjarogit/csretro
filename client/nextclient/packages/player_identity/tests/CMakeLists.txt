find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(player_identity-tests ResolveTests.cpp)
target_link_libraries(player_identity-tests PRIVATE GTest::gtest_main player_identity::player_identity)
gtest_discover_tests(player_identity-tests)
