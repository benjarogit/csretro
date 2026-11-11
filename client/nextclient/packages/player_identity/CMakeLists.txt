cmake_minimum_required(VERSION 3.21)
project(player_identity)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(player_identity STATIC src/Resolve.cpp)
add_library(player_identity::player_identity ALIAS player_identity)
target_include_directories(player_identity PUBLIC include)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
