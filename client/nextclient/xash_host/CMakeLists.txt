cmake_minimum_required(VERSION 3.21)
project(xash_host)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(xash_host SHARED src/xash_host.cpp)
add_library(xash::host ALIAS xash_host)

target_include_directories(xash_host
        PUBLIC include
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../xash_surface/include")

target_link_libraries(xash_host PRIVATE ${CMAKE_DL_LIBS})

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
            "XASH_SURFACE_FILE=\"$<TARGET_FILE:xash_surface>\"")
    target_include_directories(xash_host-tests PRIVATE
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
    target_link_libraries(xash_host-tests PRIVATE xash_host GTest::gtest_main ${CMAKE_DL_LIBS})
    add_dependencies(xash_host-tests xash_surface)
    gtest_discover_tests(xash_host-tests)
endif ()
