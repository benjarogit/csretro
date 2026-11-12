cmake_minimum_required(VERSION 3.21)
project(xash_launcher)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(xash_launcher STATIC src/xash_launcher.cpp)
add_library(xash::launcher ALIAS xash_launcher)
target_include_directories(xash_launcher PUBLIC include)
target_link_libraries(xash_launcher PUBLIC xash::host ${CMAKE_DL_LIBS})

if (NEXTCLIENT_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)

    add_executable(xash_launcher-tests tests/LauncherPlanTests.cpp)
    target_compile_definitions(xash_launcher-tests PRIVATE
            "XASH_HOST_FILE=\"$<TARGET_FILE:xash_host>\""
            "XASH_SURFACE_FILE=\"$<TARGET_FILE:xash_surface>\""
            "XASH_CLIENT_FILE=\"$<TARGET_FILE:client_mini>\""
            "XASH_VGUI_FILE=\"$<TARGET_FILE:vgui2>\""
            "XASH_GAMEUI_FILE=\"$<TARGET_FILE:gameui>\""
            "XASH_STEAM_API_FILE=\"$<TARGET_FILE:steam_api>\"")
    target_link_libraries(xash_launcher-tests PRIVATE xash_launcher GTest::gtest_main)
    add_dependencies(xash_launcher-tests xash_host xash_surface client_mini vgui2 gameui steam_api)
    gtest_discover_tests(xash_launcher-tests)
endif ()
