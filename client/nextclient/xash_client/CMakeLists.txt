cmake_minimum_required(VERSION 3.21)
project(xash_client LANGUAGES CXX)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif ()

add_library(xash_client SHARED src/client_export.cpp src/client_callbacks.cpp src/vgui_binding.cpp)
target_include_directories(xash_client PRIVATE ../xash_surface/include)
target_link_libraries(xash_client PRIVATE goldsrc::abi nitro_build_registry nitro_native_symbols ${CMAKE_DL_LIBS})
target_link_options(xash_client PRIVATE "LINKER:--exclude-libs,ALL")
set_target_properties(xash_client PROPERTIES
        PREFIX ""
        OUTPUT_NAME "csretro_client"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
add_dependencies(xash_client goldsrc_abi_golden)

if (NEXTCLIENT_BUILD_TESTS)
    if (NOT TARGET GTest::gtest_main)
        find_package(GTest CONFIG REQUIRED)
    endif ()
    include(GoogleTest)
    add_library(client-callback-fixture SHARED tests/CallbackFixture.cpp)
    target_link_libraries(client-callback-fixture PRIVATE goldsrc::abi)
    add_library(client-no-export-fixture SHARED tests/NoExportFixture.cpp)
    add_library(client-trap-fixture SHARED tests/TrapFixture.cpp)
    add_library(client-vgui-fixture SHARED tests/VguiFixture.cpp)
    target_include_directories(client-vgui-fixture PRIVATE src)
    target_compile_features(client-vgui-fixture PRIVATE cxx_std_17)
    add_executable(xash_client-tests tests/ClientCallbackTests.cpp tests/VguiBindingTests.cpp
            src/client_callbacks.cpp src/vgui_binding.cpp)
    target_include_directories(xash_client-tests PRIVATE src)
    target_link_libraries(xash_client-tests PRIVATE goldsrc::abi nitro_build_registry nitro_native_symbols GTest::gtest_main ${CMAKE_DL_LIBS})
    target_compile_definitions(xash_client-tests PRIVATE
            CLIENT_CALLBACK_FIXTURE="$<TARGET_FILE:client-callback-fixture>"
            CLIENT_NO_EXPORT_FIXTURE="$<TARGET_FILE:client-no-export-fixture>"
            CLIENT_TRAP_FIXTURE="$<TARGET_FILE:client-trap-fixture>"
            CLIENT_VGUI_FIXTURE="$<TARGET_FILE:client-vgui-fixture>"
            CLIENT_BRIDGE="$<TARGET_FILE:xash_client>")
    add_dependencies(xash_client-tests client-callback-fixture client-no-export-fixture client-trap-fixture client-vgui-fixture xash_client)
    gtest_discover_tests(xash_client-tests)
endif ()
