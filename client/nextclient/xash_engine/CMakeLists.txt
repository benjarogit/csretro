cmake_minimum_required(VERSION 3.21)
project(xash_engine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(xash_engine SHARED src/xash_engine.cpp)
add_library(xash::engine ALIAS xash_engine)

target_include_directories(xash_engine PUBLIC include)

set_target_properties(xash_engine PROPERTIES
        PREFIX ""
        OUTPUT_NAME "xash_engine"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)

if (NEXTCLIENT_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)

    add_executable(xash_engine-tests tests/XashEngineTests.cpp)
    target_link_libraries(xash_engine-tests PRIVATE xash_engine GTest::gtest_main ${CMAKE_DL_LIBS})
    gtest_discover_tests(xash_engine-tests)
endif ()
