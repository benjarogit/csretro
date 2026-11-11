cmake_minimum_required(VERSION 3.21)
project(xash_host)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(xash_host SHARED
        src/xash_host.cpp
        src/host_filesystem.cpp)
add_library(xash::host ALIAS xash_host)

target_include_directories(xash_host
        PUBLIC include
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../xash_surface/include"
            "${NEXTCLIENT_ROOT}/dep/ncl-hl1-source-sdk/public"
            "${NEXTCLIENT_ROOT}/dep/ncl-hl1-source-sdk/public/tier1")

target_link_libraries(xash_host PRIVATE xash::engine ${CMAKE_DL_LIBS})

set_target_properties(xash_host PROPERTIES
        PREFIX ""
        OUTPUT_NAME "xash_host"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)

if (NEXTCLIENT_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)

    add_executable(xash_host-tests tests/HostFactoryTests.cpp)
    target_compile_definitions(xash_host-tests PRIVATE
            "XASH_SURFACE_FILE=\"$<TARGET_FILE:xash_surface>\""
            "XASH_CLIENT_FILE=\"$<TARGET_FILE:client_mini>\""
            "XASH_VGUI_FILE=\"$<TARGET_FILE:vgui2>\""
            "XASH_GAMEUI_FILE=\"$<TARGET_FILE:gameui>\"")
    target_include_directories(xash_host-tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../xash_engine/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/../xash_surface/include"
            "${NEXTCLIENT_ROOT}/dep/ncl-hl1-source-sdk/public"
            "${NEXTCLIENT_ROOT}/dep/ncl-hl1-source-sdk/public/tier0"
            "${NEXTCLIENT_ROOT}/dep/ncl-hl1-source-sdk/public/tier1")
    target_compile_definitions(xash_host-tests PRIVATE
            NO_MALLOC_OVERRIDE
            MATHLIB_HEADER_ONLY
            "_DLL_EXT=.so")
    if (UNIX AND NOT APPLE)
        target_compile_definitions(xash_host-tests PRIVATE GNUC POSIX LINUX _LINUX)
    endif ()
    target_link_libraries(xash_host-tests PRIVATE xash_host xash::engine GTest::gtest_main ${CMAKE_DL_LIBS})
    add_dependencies(xash_host-tests xash_surface client_mini vgui2 gameui)
    gtest_discover_tests(xash_host-tests)
endif ()
