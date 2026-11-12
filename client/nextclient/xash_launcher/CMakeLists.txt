cmake_minimum_required(VERSION 3.21)
project(xash_launcher)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(xash_launcher STATIC src/xash_launcher.cpp src/map_load.cpp)
add_library(xash::launcher ALIAS xash_launcher)
target_include_directories(xash_launcher PUBLIC include)
target_link_libraries(xash_launcher PUBLIC xash::host goldsrc::abi ${CMAKE_DL_LIBS})

add_executable(xash_launcher_entry src/main.cpp)
set_target_properties(xash_launcher_entry PROPERTIES OUTPUT_NAME "xash_launcher")
target_link_libraries(xash_launcher_entry PRIVATE xash_launcher)

if (NEXTCLIENT_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)

    add_executable(xash_launcher-tests tests/LauncherPlanTests.cpp src/main.cpp)
    target_compile_definitions(xash_launcher-tests PRIVATE CSRETRO_ENTRY_NAME=csretro_human_main)
    target_include_directories(xash_launcher-tests PRIVATE
            "${NEXTCLIENT_ROOT}/dep/ncl-hl1-source-sdk/public"
            "${NEXTCLIENT_ROOT}/dep/ncl-hl1-source-sdk/public/tier0"
            "${NEXTCLIENT_ROOT}/dep/ncl-hl1-source-sdk/public/tier1")
    target_compile_definitions(xash_launcher-tests PRIVATE
            NO_MALLOC_OVERRIDE
            MATHLIB_HEADER_ONLY)
    target_compile_definitions(xash_launcher-tests PRIVATE
            "XASH_HOST_FILE=\"$<TARGET_FILE:xash_host>\""
            "XASH_SURFACE_FILE=\"$<TARGET_FILE:xash_surface>\""
            "XASH_CLIENT_FILE=\"$<TARGET_FILE:client_mini>\""
            "XASH_VGUI_FILE=\"$<TARGET_FILE:vgui2>\""
            "XASH_GAMEUI_FILE=\"$<TARGET_FILE:gameui>\""
            "XASH_STEAM_API_FILE=\"$<TARGET_FILE:steam_api>\""
            "XASH_ENGINE_MINI_FILE=\"$<TARGET_FILE:next_engine_mini>\""
            "XASH_NITRO_API_FILE=\"$<TARGET_FILE:nitro_api>\"")
    if (UNIX AND NOT APPLE)
        target_compile_definitions(xash_launcher-tests PRIVATE GNUC POSIX LINUX _LINUX "_DLL_EXT=.so")
        find_package(X11 REQUIRED)
        find_package(Threads REQUIRED)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl)
        target_link_libraries(xash_launcher-tests PRIVATE xash_launcher GTest::gtest_main X11::X11 Threads::Threads PkgConfig::EGL)
    else ()
        target_link_libraries(xash_launcher-tests PRIVATE xash_launcher GTest::gtest_main)
    endif ()
    add_dependencies(xash_launcher-tests xash_launcher_entry xash_host xash_surface client_mini vgui2 gameui steam_api next_engine_mini nitro_api)
    gtest_discover_tests(xash_launcher-tests)
endif ()
