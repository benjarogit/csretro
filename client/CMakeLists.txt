cmake_minimum_required(VERSION 3.21)

option(USE_PROFILER "Enable shared diagnostic Tracy profiling" OFF)
if (USE_PROFILER)
    list(APPEND VCPKG_MANIFEST_FEATURES profiler)
    list(REMOVE_DUPLICATES VCPKG_MANIFEST_FEATURES)
endif ()

# a clone made without Git LFS holds pointer files in place of the LFS objects, and BUILD_ALL would install them;
# checked ahead of project(), whose toolchain file runs the whole vcpkg install. One file per pattern of .gitattributes
string(HEX "version https://git-lfs" NEXTCLIENT_LFS_POINTER_HEAD)

foreach (NEXTCLIENT_LFS_FILE
        "assets/platform/servers/geoip_country.mmdb"
        "assets/platform/servers/icon_country_column.tga")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${NEXTCLIENT_LFS_FILE}" NEXTCLIENT_LFS_FILE_HEAD LIMIT 23 HEX)

    if (NEXTCLIENT_LFS_FILE_HEAD STREQUAL NEXTCLIENT_LFS_POINTER_HEAD)
        message(FATAL_ERROR "${NEXTCLIENT_LFS_FILE} is a Git LFS pointer file: install Git LFS and run 'git lfs pull'")
    endif ()
endforeach ()

# The HL1 SDK tree is tracked source. Our edits are those files.
project(NextClient VERSION 2.6.0)

set(NEXTCLIENT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include(cmake/NextClientProfiling.cmake)

# TODO do fetch curl and zlib via vcpkg, make a custom port if necessary
set(CMAKE_MODULE_PATH
        ${NEXTCLIENT_ROOT}/cmake
        ${CMAKE_MODULE_PATH}
)

if (NEXTCLIENT_BUILD_TESTS OR USE_PROFILER OR UNIX)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/bin)
    set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST CACHE STRING
            "When Google Test executables are queried for test discovery")
    enable_testing()
else ()
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out/bin)
endif ()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out/pdb)

# CMAKE_MSVC_RUNTIME_LIBRARY and BUILD_SHARED_LIBS only affect external dependencies.
# NextClient subprojects are always built as shared libraries with mt runtime.
if (VCPKG_TARGET_TRIPLET STREQUAL "x86-windows-static")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else ()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif ()
set(BUILD_SHARED_LIBS OFF)

# NitroApi options
set(NITRO_API_MSVC_RUNTIME_LIBRARY ${CMAKE_MSVC_RUNTIME_LIBRARY})

# cpr options
set(CPR_USE_SYSTEM_CURL ON)
set(CPR_USE_SYSTEM_GTEST OFF)
set(CPR_ENABLE_CURL_HTTP_ONLY OFF)
set(CPR_ENABLE_SSL ON)

# common compiler and linker options
if (MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/MP>)
    
    string(APPEND CMAKE_C_FLAGS_RELEASE
            " /Zi"  # enables PDB generation
            " /O2"  # enables maximize speed optimizations
            " /GL"  # enables whole program optimization
    )
    string(APPEND CMAKE_CXX_FLAGS_RELEASE
            " /Zi"  # enables PDB generation
            " /O2"  # enables maximize speed optimizations
            " /GL"  # enables whole program optimization
    )
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE
            " /LTCG"           # enables whole program optimization on link-time
            " /INCREMENTAL:NO" # /INCREMENTAL is not compatible with /LTCG
            " /DEBUG"          # enables PDB generation
            " /OPT:REF"       # eliminates functions and data that are never referenced
            " /OPT:ICF"       # enables COMDAT folding
    )
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE
            " /LTCG"           # enables whole program optimization on link-time
            " /INCREMENTAL:NO" # /INCREMENTAL is not compatible with /LTCG
            " /DEBUG"          # enables PDB generation
            " /OPT:REF"       # eliminates functions and data that are never referenced
            " /OPT:ICF"       # enables COMDAT folding
    )
endif ()

# Static libraries are linked into the x86_64 shared modules. They need
# position-independent code or the link of vgui2 and client_mini fails.
if (UNIX AND NOT APPLE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif ()

# external dependencies (needed by engine_mini as well as the launcher)
add_subdirectory(dep/cpr)
add_subdirectory(dep/easyloggingpp)
add_subdirectory(dep/bzip2)
add_subdirectory(dep/glad)
add_subdirectory(dep/stb)

# Settings of the SDK fork, nitro_utils, funchook, goldsrc_abi and nitro_api. The standard is set
# here rather than at the top of the file, and unset again below the last of them, so that the
# directories added outside this span keep the standard they choose: cpr picks C++17 for itself
# whenever the parent leaves it unset, and data_encoding picks nothing at all. A cache entry from
# the command line outlives the unset and reaches all of them.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_CXX_STANDARD 23)

if (UNIX)
    add_compile_options(-Wno-narrowing)
    add_link_options(-static-libstdc++ -static-libgcc)
endif ()

# funchook options. Its cmake_minimum_required is 3.11, which leaves CMP0077 unset, so the policy
# default above is what keeps its option() calls from overwriting these.
set(FUNCHOOK_BUILD_SHARED OFF)
set(FUNCHOOK_BUILD_TESTS OFF)
set(FUNCHOOK_INSTALL OFF)
set(FUNCHOOK_CPU "x86")

# The prebuilt libtier0.so, libvstdlib.so and libsteam_api.so under the SDK are
# i386. The product process does not load them. These shared libraries satisfy
# the SDK link and the factory probe, which asks for the built file.
add_library(tier0 SHARED sdk_host/host_runtime.cpp)
target_include_directories(tier0 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/ncl-hl1-source-sdk/public"
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/ncl-hl1-source-sdk/public/tier0"
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/ncl-hl1-source-sdk/public/tier1")
target_compile_definitions(tier0 PRIVATE GNUC LINUX POSIX _LINUX NO_MALLOC_OVERRIDE)
add_library(vstdlib SHARED sdk_host/vstdlib_stub.cpp)
target_include_directories(vstdlib PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/ncl-hl1-source-sdk/public"
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/ncl-hl1-source-sdk/public/tier0"
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/ncl-hl1-source-sdk/public/tier1")
target_compile_definitions(vstdlib PRIVATE GNUC LINUX POSIX _LINUX NO_MALLOC_OVERRIDE)
add_library(steam_api SHARED sdk_host/steam_api_stub.cpp)
target_include_directories(steam_api PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk_host/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/nextclient/packages/player_identity/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/ncl-hl1-source-sdk/public"
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/ncl-hl1-source-sdk/public/tier0")
target_compile_definitions(steam_api PRIVATE GNUC LINUX POSIX _LINUX STEAM_API_EXPORTS)

add_subdirectory(dep/ncl-hl1-source-sdk)
add_subdirectory(dep/nitro_utils)
add_subdirectory(dep/funchook) # while funchook is not in vcpkg, add it as add_subdirectory (PS do NOT make it a sub-module).

# NextClient sub-projects
add_subdirectory(nextclient/goldsrc_abi)
add_subdirectory(nextclient/nitro_api)

unset(CMAKE_CXX_STANDARD)

add_subdirectory(nextclient/packages)
if (NEXTCLIENT_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
    add_executable(steam_api-tests sdk_host/tests/SteamProxyTests.cpp)
    target_include_directories(steam_api-tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/sdk_host/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/dep/ncl-hl1-source-sdk/public"
            "${CMAKE_CURRENT_SOURCE_DIR}/dep/ncl-hl1-source-sdk/public/tier0")
    target_compile_definitions(steam_api-tests PRIVATE GNUC LINUX POSIX _LINUX)
    target_link_libraries(steam_api-tests PRIVATE steam_api player_identity::player_identity GTest::gtest_main)
    gtest_discover_tests(steam_api-tests)
endif ()
add_subdirectory(nextclient/xash_engine)
add_subdirectory(nextclient/engine_mini)
add_subdirectory(nextclient/xash_surface)
add_subdirectory(nextclient/xash_host)
add_subdirectory(nextclient/gameui)
add_subdirectory(nextclient/client_mini)
add_subdirectory(nextclient/xash_launcher)
add_subdirectory(tools/matgen)
add_subdirectory(nextclient/steam_api_proxy)
add_subdirectory(nextclient/filesystem_proxy)
add_subdirectory(nextclient/launcher)

if (NEXTCLIENT_BUILD_TESTS AND WIN32)
    set(NEXTCLIENT_TEST_RUNTIME_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    get_property(NEXTCLIENT_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if (NEXTCLIENT_MULTI_CONFIG)
        string(APPEND NEXTCLIENT_TEST_RUNTIME_DIRECTORY "/$<CONFIG>")
    endif ()

    add_custom_target(nextclient-test-runtime
            COMMAND ${CMAKE_COMMAND} -E make_directory "${NEXTCLIENT_TEST_RUNTIME_DIRECTORY}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${NEXTCLIENT_ROOT}/test_support/runtime"
                    "${NEXTCLIENT_TEST_RUNTIME_DIRECTORY}"
            VERBATIM)
    foreach (test_target IN ITEMS client_mini-tests radar-gl-tests performance-gl-tests
            studio-capture-gl-tests scene-preview-gl-tests preview-model-tests material-lifecycle-tests)
        add_dependencies(${test_target} nextclient-test-runtime)
    endforeach ()
endif ()

# Windows 7 compatibility
if (MSVC)
    set(YY_THUNKS_WIN7_OBJ "${NEXTCLIENT_ROOT}/dep/yy-thunks/x86/YY_Thunks_for_Win7.obj")
    foreach (_tgt
            next_launcher
            engine_mini
            filesystem_proxy
            client_mini
            gameui
            steam_api_proxy
            nitro_api
            vgui2)
        target_link_libraries(${_tgt} PRIVATE "${YY_THUNKS_WIN7_OBJ}")
    endforeach ()
endif ()

# Install directory: cache variable with env fallback
if (DEFINED ENV{NEXTCLIENT_INSTALL_DIR} AND NOT NEXTCLIENT_INSTALL_DIR)
    set(NEXTCLIENT_INSTALL_DIR "$ENV{NEXTCLIENT_INSTALL_DIR}" CACHE PATH "NextClient install directory")
else ()
    set(NEXTCLIENT_INSTALL_DIR "" CACHE PATH "NextClient install directory")
endif ()

# custom build target
if (NOT NEXTCLIENT_BUILD_TESTS)
    add_custom_target(BUILD_ALL ALL)

    add_dependencies(BUILD_ALL
            next_launcher
            engine_mini
            filesystem_proxy
            client_mini
            gameui
            steam_api_proxy
            nitro_api
            vgui2
            xash_surface
    )

    if (NEXTCLIENT_INSTALL_DIR AND (WIN32 OR UNIX))
        if (WIN32)
            set(NEXTCLIENT_INSTALL_PLATFORM Windows)
        else ()
            set(NEXTCLIENT_INSTALL_PLATFORM Linux)
        endif ()
        set(NEXTCLIENT_INSTALL_TRACY_SOURCE "")
        if (USE_PROFILER)
            set(NEXTCLIENT_INSTALL_TRACY_SOURCE "$<TARGET_FILE:Tracy::TracyClient>")
        endif ()
        # The retail libtier0.so does not export GetCPUInformation, which the
        # SDK-built client_mini needs; only Linux replaces the retail library.
        set(NEXTCLIENT_INSTALL_TIER0_SOURCE "")
        if (NEXTCLIENT_INSTALL_PLATFORM STREQUAL "Linux")
            set(NEXTCLIENT_INSTALL_TIER0_SOURCE "$<TARGET_FILE:tier0>")
        endif ()
        configure_file(cmake/NextClientInstallManifest.cmake.in
                "${CMAKE_CURRENT_BINARY_DIR}/install-nextclient-configured.cmake" @ONLY)
        file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/install-nextclient-$<CONFIG>.cmake"
                INPUT "${CMAKE_CURRENT_BINARY_DIR}/install-nextclient-configured.cmake")
        add_custom_command(TARGET BUILD_ALL POST_BUILD
                COMMENT "Installing libraries and assets to ${NEXTCLIENT_INSTALL_DIR}..."
                COMMAND ${CMAKE_COMMAND}
                        "-DINSTALL_MANIFEST=${CMAKE_CURRENT_BINARY_DIR}/install-nextclient-$<CONFIG>.cmake"
                        -P "${NEXTCLIENT_ROOT}/cmake/InstallNextClient.cmake"
                VERBATIM)
    endif ()
endif ()

if (NEXTCLIENT_BUILD_TESTS AND UNIX AND NOT APPLE)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
    add_executable(sdk_pair-tests nextclient/client_mini/tests/SdkPairTests.cpp)
    target_compile_definitions(sdk_pair-tests PRIVATE
            "SDK_PAIR_CLIENT=\"$<TARGET_FILE:client_mini>\""
            "SDK_PAIR_VGUI=\"$<TARGET_FILE:vgui2>\""
            "SDK_PAIR_GAMEUI=\"$<TARGET_FILE:gameui>\"")
    target_link_libraries(sdk_pair-tests PRIVATE GTest::gtest_main)
    add_dependencies(sdk_pair-tests client_mini vgui2 gameui)
    gtest_discover_tests(sdk_pair-tests)
endif ()

if (NEXTCLIENT_BUILD_TESTS)
    add_test(NAME installation-preflight
            COMMAND ${CMAKE_COMMAND} "-DTEST_BINARY_DIRECTORY=${CMAKE_BINARY_DIR}"
                    -P "${NEXTCLIENT_ROOT}/cmake/tests/InstallNextClientTests.cmake")
    set_tests_properties(installation-preflight PROPERTIES LABELS installation RUN_SERIAL TRUE)
endif ()
