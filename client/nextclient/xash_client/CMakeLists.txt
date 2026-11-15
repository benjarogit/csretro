cmake_minimum_required(VERSION 3.21)
project(xash_client LANGUAGES CXX)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif ()

add_library(xash_client SHARED src/client_export.cpp src/client_callbacks.cpp src/vgui_binding.cpp
        src/original_panels.cpp src/engine_files.cpp src/original_ui.cpp src/pointer_motion.cpp src/gameui_video.cpp src/XashKeys.cpp src/NativeGameUiFuncs.cpp src/NativePanelTree.cpp)
target_include_directories(xash_client PRIVATE ../xash_surface/include)
target_link_libraries(xash_client PRIVATE goldsrc::abi nitro_build_registry nitro_native_symbols ${CMAKE_DL_LIBS})
target_link_options(xash_client PRIVATE "LINKER:--exclude-libs,ALL")
set_target_properties(xash_client PROPERTIES
        PREFIX ""
        OUTPUT_NAME "csretro_client"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
add_dependencies(xash_client goldsrc_abi_golden)
if (TARGET xash_surface)
    add_dependencies(xash_client xash_surface)
endif ()

if (NEXTCLIENT_BUILD_TESTS)
    if (NOT TARGET GTest::gtest_main)
        find_package(GTest CONFIG REQUIRED)
    endif ()
    include(GoogleTest)
    add_library(client-callback-fixture SHARED tests/CallbackFixture.cpp)
    target_link_libraries(client-callback-fixture PRIVATE goldsrc::abi)
    add_library(client-video-fixture SHARED tests/VideoFixture.cpp)
    target_link_libraries(client-video-fixture PRIVATE goldsrc::abi)
    add_library(client-video-no-masks-fixture SHARED tests/VideoFixture.cpp)
    target_link_libraries(client-video-no-masks-fixture PRIVATE goldsrc::abi)
    target_compile_definitions(client-video-no-masks-fixture PRIVATE CSRETRO_FIXTURE_NO_MASKS)
    add_library(client-key-fixture SHARED tests/KeyFixture.cpp)
    target_link_libraries(client-key-fixture PRIVATE goldsrc::abi)
    target_link_options(client-key-fixture PRIVATE "LINKER:--build-id=sha1")
    add_library(client-native-panel-fixture SHARED tests/NativePanelFixture.cpp)
    target_include_directories(client-native-panel-fixture PRIVATE src)
    target_compile_features(client-native-panel-fixture PRIVATE cxx_std_17)
    target_link_options(client-native-panel-fixture PRIVATE "LINKER:--build-id=sha1")
    add_library(client-legacy-gameui-consumer SHARED tests/LegacyGameUiConsumer.cpp)
    target_compile_features(client-legacy-gameui-consumer PRIVATE cxx_std_17)
    add_library(client-no-export-fixture SHARED tests/NoExportFixture.cpp)
    add_library(client-trap-fixture SHARED tests/TrapFixture.cpp)
    add_library(client-vgui-fixture SHARED tests/VguiFixture.cpp)
    target_include_directories(client-vgui-fixture PRIVATE src)
    target_compile_features(client-vgui-fixture PRIVATE cxx_std_17)
    add_executable(xash_client-tests tests/ClientCallbackTests.cpp tests/VguiBindingTests.cpp
            tests/OriginalPanelTests.cpp tests/EngineFileTests.cpp tests/RuntimeCallbackTests.cpp tests/PointerMotionTests.cpp tests/GameUiVideoTests.cpp tests/NativeFunctionTests.cpp tests/NativeGameUiFuncsTests.cpp tests/NativePanelTreeTests.cpp tests/NativePanelBindingTests.cpp
            src/client_callbacks.cpp src/vgui_binding.cpp src/original_panels.cpp src/engine_files.cpp src/pointer_motion.cpp src/gameui_video.cpp src/XashKeys.cpp src/NativeGameUiFuncs.cpp src/NativePanelTree.cpp)
    target_include_directories(xash_client-tests PRIVATE src ../xash_surface/include)
    target_link_libraries(xash_client-tests PRIVATE goldsrc::abi nitro_build_registry nitro_native_symbols GTest::gtest_main ${CMAKE_DL_LIBS})
    target_compile_definitions(xash_client-tests PRIVATE
            CLIENT_VIDEO_NO_MASKS_FIXTURE="$<TARGET_FILE:client-video-no-masks-fixture>"
        CLIENT_NATIVE_PANEL_FIXTURE="$<TARGET_FILE:client-native-panel-fixture>"
        CLIENT_LEGACY_GAMEUI_CONSUMER="$<TARGET_FILE:client-legacy-gameui-consumer>"
        CLIENT_KEY_FIXTURE="$<TARGET_FILE:client-key-fixture>"
        CLIENT_VIDEO_FIXTURE="$<TARGET_FILE:client-video-fixture>"
        CLIENT_CALLBACK_FIXTURE="$<TARGET_FILE:client-callback-fixture>"
            CLIENT_NO_EXPORT_FIXTURE="$<TARGET_FILE:client-no-export-fixture>"
            CLIENT_TRAP_FIXTURE="$<TARGET_FILE:client-trap-fixture>"
            CLIENT_VGUI_FIXTURE="$<TARGET_FILE:client-vgui-fixture>"
            CLIENT_BRIDGE="$<TARGET_FILE:xash_client>")
    add_dependencies(xash_client-tests client-native-panel-fixture client-legacy-gameui-consumer client-key-fixture client-video-no-masks-fixture client-video-fixture client-callback-fixture client-no-export-fixture client-trap-fixture client-vgui-fixture xash_client)
    gtest_discover_tests(xash_client-tests)
endif ()
