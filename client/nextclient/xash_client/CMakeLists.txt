cmake_minimum_required(VERSION 3.21)
project(xash_client LANGUAGES CXX)
find_package(Threads REQUIRED)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif ()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../../../VERSION" CSRETRO_PRODUCT_VERSION)
if (NOT CSRETRO_PRODUCT_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    message(FATAL_ERROR "VERSION must begin with a semantic product version")
endif ()
math(EXPR CSRETRO_SYSTEM_VERSION "${CMAKE_MATCH_1} * 10000 + ${CMAKE_MATCH_2} * 100 + ${CMAKE_MATCH_3}")

add_library(xash_client SHARED src/client_export.cpp src/client_callbacks.cpp src/vgui_binding.cpp
        src/original_panels.cpp src/engine_files.cpp src/original_ui.cpp src/pointer_motion.cpp src/gameui_video.cpp src/XashKeys.cpp src/NativeGameUiFuncs.cpp src/NativePanelTree.cpp src/NativeGameUiSystem.cpp src/NativeGameUiRuntime.cpp src/NativeEngineCommands.cpp src/NativeSdlInput.cpp src/NativeGameUiContext.cpp src/OriginalUiModules.cpp src/NativeKeyCapture.cpp)
target_compile_definitions(xash_client PRIVATE CSRETRO_SYSTEM_VERSION=${CSRETRO_SYSTEM_VERSION})
target_include_directories(xash_client PRIVATE ../xash_surface/include ../../dep/SDL2/include)
target_link_libraries(xash_client PRIVATE Threads::Threads)
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
    add_library(client-native-sdl-fixture SHARED tests/NativeSdlFixture.cpp)
    target_include_directories(client-native-sdl-fixture PRIVATE ../../dep/SDL2/include)
    target_compile_features(client-native-sdl-fixture PRIVATE cxx_std_17)
    target_link_libraries(client-native-sdl-fixture PRIVATE Threads::Threads)
    target_link_options(client-native-sdl-fixture PRIVATE "LINKER:--build-id=sha1")
    add_library(client-native-command-fixture SHARED tests/NativeEngineCommandFixture.cpp)
    target_link_libraries(client-native-command-fixture PRIVATE goldsrc::abi)
    target_link_options(client-native-command-fixture PRIVATE "LINKER:--build-id=sha1")
    add_library(client-native-panel-runtime-objects OBJECT tests/NativePanelFixture.cpp)
    target_include_directories(client-native-panel-runtime-objects PRIVATE src)
    target_compile_features(client-native-panel-runtime-objects PRIVATE cxx_std_17)
    target_compile_definitions(client-native-panel-runtime-objects PRIVATE CreateInterface=NativePanelCreateInterface)
    set_target_properties(client-native-panel-runtime-objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(client-native-sdl-runtime-objects OBJECT tests/NativeSdlFixture.cpp)
    target_include_directories(client-native-sdl-runtime-objects PRIVATE ../../dep/SDL2/include)
    target_compile_features(client-native-sdl-runtime-objects PRIVATE cxx_std_17)
    target_compile_definitions(client-native-sdl-runtime-objects PRIVATE CreateInterface=NativeSdlCreateInterface)
    set_target_properties(client-native-sdl-runtime-objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(client-native-gameui-fixture SHARED tests/NativeGameUiFixture.cpp
            $<TARGET_OBJECTS:client-native-panel-runtime-objects>
            $<TARGET_OBJECTS:client-native-sdl-runtime-objects>)
    target_include_directories(client-native-gameui-fixture PRIVATE src)
    target_link_libraries(client-native-gameui-fixture PRIVATE goldsrc::abi Threads::Threads)
    target_compile_features(client-native-gameui-fixture PRIVATE cxx_std_17)
    target_link_options(client-native-gameui-fixture PRIVATE "LINKER:--build-id=sha1")
    add_library(client-native-panel-fixture SHARED tests/NativePanelFixture.cpp)
    target_include_directories(client-native-panel-fixture PRIVATE src)
    target_compile_features(client-native-panel-fixture PRIVATE cxx_std_17)
    target_link_options(client-native-panel-fixture PRIVATE "LINKER:--build-id=sha1")
    add_library(client-legacy-system-consumer SHARED tests/LegacySystemConsumer.cpp)
    target_compile_features(client-legacy-system-consumer PRIVATE cxx_std_17)
    add_library(client-legacy-gameui-consumer SHARED tests/LegacyGameUiConsumer.cpp)
    target_compile_features(client-legacy-gameui-consumer PRIVATE cxx_std_17)
    add_library(client-no-export-fixture SHARED tests/NoExportFixture.cpp)
    add_library(client-trap-fixture SHARED tests/TrapFixture.cpp)
    add_library(client-vgui-fixture SHARED tests/VguiFixture.cpp)
    target_include_directories(client-vgui-fixture PRIVATE src)
    target_compile_features(client-vgui-fixture PRIVATE cxx_std_17)
    add_executable(xash_client-tests tests/ClientCallbackTests.cpp tests/VguiBindingTests.cpp
            tests/OriginalPanelTests.cpp tests/EngineFileTests.cpp tests/RuntimeCallbackTests.cpp tests/PointerMotionTests.cpp tests/GameUiVideoTests.cpp tests/NativeFunctionTests.cpp tests/NativeGameUiFuncsTests.cpp tests/NativePanelTreeTests.cpp tests/NativePanelBindingTests.cpp tests/NativeGameUiSystemTests.cpp tests/NativeGameUiRuntimeTests.cpp tests/NativeEngineCommandsTests.cpp tests/NativeSdlInputTests.cpp tests/NativeGameUiContextTests.cpp tests/OriginalUiModulesTests.cpp tests/NativeKeyCaptureTests.cpp
            src/client_callbacks.cpp src/vgui_binding.cpp src/original_panels.cpp src/engine_files.cpp src/pointer_motion.cpp src/gameui_video.cpp src/XashKeys.cpp src/NativeGameUiFuncs.cpp src/NativePanelTree.cpp src/NativeGameUiSystem.cpp src/NativeGameUiRuntime.cpp src/NativeEngineCommands.cpp src/NativeSdlInput.cpp src/NativeGameUiContext.cpp src/OriginalUiModules.cpp src/NativeKeyCapture.cpp)
    target_include_directories(xash_client-tests PRIVATE src ../xash_surface/include ../../dep/SDL2/include)
    target_link_libraries(xash_client-tests PRIVATE Threads::Threads)
    target_link_libraries(xash_client-tests PRIVATE goldsrc::abi nitro_build_registry nitro_native_symbols GTest::gtest_main ${CMAKE_DL_LIBS})
    target_compile_definitions(xash_client-tests PRIVATE CSRETRO_NATIVE_MODULE_TESTS
            CLIENT_VIDEO_NO_MASKS_FIXTURE="$<TARGET_FILE:client-video-no-masks-fixture>"
        CLIENT_NATIVE_SDL_FIXTURE="$<TARGET_FILE:client-native-sdl-fixture>"
        CLIENT_NATIVE_COMMAND_FIXTURE="$<TARGET_FILE:client-native-command-fixture>"
        CLIENT_NATIVE_GAMEUI_FIXTURE="$<TARGET_FILE:client-native-gameui-fixture>"
        CLIENT_LEGACY_SYSTEM_CONSUMER="$<TARGET_FILE:client-legacy-system-consumer>"
        CLIENT_NATIVE_PANEL_FIXTURE="$<TARGET_FILE:client-native-panel-fixture>"
        CLIENT_LEGACY_GAMEUI_CONSUMER="$<TARGET_FILE:client-legacy-gameui-consumer>"
        CLIENT_KEY_FIXTURE="$<TARGET_FILE:client-key-fixture>"
        CLIENT_VIDEO_FIXTURE="$<TARGET_FILE:client-video-fixture>"
        CLIENT_CALLBACK_FIXTURE="$<TARGET_FILE:client-callback-fixture>"
            CLIENT_NO_EXPORT_FIXTURE="$<TARGET_FILE:client-no-export-fixture>"
            CLIENT_TRAP_FIXTURE="$<TARGET_FILE:client-trap-fixture>"
            CLIENT_VGUI_FIXTURE="$<TARGET_FILE:client-vgui-fixture>"
            CLIENT_BRIDGE="$<TARGET_FILE:xash_client>")
    add_dependencies(xash_client-tests client-native-sdl-fixture client-native-command-fixture client-native-gameui-fixture client-legacy-system-consumer client-native-panel-fixture client-legacy-gameui-consumer client-key-fixture client-video-no-masks-fixture client-video-fixture client-callback-fixture client-no-export-fixture client-trap-fixture client-vgui-fixture xash_client)
    gtest_discover_tests(xash_client-tests)
endif ()
