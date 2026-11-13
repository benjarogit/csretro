cmake_minimum_required(VERSION 3.21)
project(ClientCallbackFixtures LANGUAGES CXX)
enable_testing()

# Compile the actual bridge, registry and tests without the unrelated SDK,
# vcpkg or any licensed files. Product builds still use the client presets.
get_filename_component(client "${CMAKE_CURRENT_LIST_DIR}/../../client" ABSOLUTE)
set(NEXTCLIENT_BUILD_TESTS ON)
if (CSRETRO_GTEST_SOURCE)
    add_subdirectory("${CSRETRO_GTEST_SOURCE}" "${CMAKE_BINARY_DIR}/gtest")
else ()
    find_package(GTest CONFIG REQUIRED)
endif ()
add_subdirectory("${client}/nextclient/packages/data_encoding" data_encoding)
add_subdirectory("${client}/nextclient/goldsrc_abi" goldsrc_abi)
add_subdirectory("${client}/nextclient/nitro_api/registry" registry)
add_subdirectory("${client}/nextclient/xash_client" xash_client)
