cmake_minimum_required(VERSION 3.28)

project(CSRetro VERSION 0.1.0 LANGUAGES C CXX)

set(CSRETRO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

include(cmake/ClangDefaults.cmake)
include(cmake/CsretroPlatform.cmake)
include(cmake/CsretroInterfaces.cmake)
include(cmake/CsretroClient.cmake)

option(CSRETRO_BUILD_ENGINE "Xash3D-FWGS über Waf bauen" ON)
option(CSRETRO_BUILD_CLIENT "NextClient-Upstream-CMake (MSVC/vcpkg) — tot, nicht einschalten" OFF)
option(CSRETRO_BUILD_SERVER "NextClientServerApi — optional, nicht die GameDLL" OFF)
option(CSRETRO_BUILD_BOTS "Bots — Bereich ist leer" OFF)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# --- Engine (Waf bleibt das native Build von Xash; CMake orchestriert) ---
if(CSRETRO_BUILD_ENGINE)
    set(CSRETRO_WAF "${CSRETRO_ROOT}/engine/waf")
    set(CSRETRO_ENGINE_OUT "${CMAKE_BINARY_DIR}/engine")
    set(CSRETRO_WAF_CONFIGURE ${Python3_EXECUTABLE} "${CSRETRO_WAF}" configure
        --out "${CSRETRO_ENGINE_OUT}"
        --check-c-compiler clang
        --check-cxx-compiler clang++)
    list(APPEND CSRETRO_WAF_CONFIGURE -8)

    add_custom_target(engine-configure
        COMMAND ${CMAKE_COMMAND} -E env
            CC=${CMAKE_C_COMPILER}
            CXX=${CMAKE_CXX_COMPILER}
            ${CSRETRO_WAF_CONFIGURE}
        WORKING_DIRECTORY "${CSRETRO_ROOT}/engine"
        COMMENT "Xash3D Waf configure (Clang)"
        VERBATIM
    )

    add_custom_target(engine
        COMMAND ${CMAKE_COMMAND} -E env
            CC=${CMAKE_C_COMPILER}
            CXX=${CMAKE_CXX_COMPILER}
            ${Python3_EXECUTABLE} "${CSRETRO_WAF}" build --out "${CSRETRO_ENGINE_OUT}"
        WORKING_DIRECTORY "${CSRETRO_ROOT}/engine"
        DEPENDS engine-configure
        COMMENT "Xash3D Waf build (Clang)"
        VERBATIM
    )
endif()

# NextClient-Upstream-CMake bleibt tot (MSVC/vcpkg, Steam-Pfad). Eine Client-Lib = Phase 3.
if(CSRETRO_BUILD_CLIENT)
    message(FATAL_ERROR
        "CSRETRO_BUILD_CLIENT ist tot: client/CMakeLists.txt ist der alte "
        "Windows/vcpkg-Upstream. Produkt-Client: CSRETRO_BUILD_XASH_CLIENT "
        "(client/export + body).")
endif()

if(CSRETRO_BUILD_SERVER)
    add_subdirectory(server)
endif()

if(CSRETRO_BUILD_BOTS)
    message(FATAL_ERROR "bots/ ist leer — kein Target.")
endif()

message(STATUS "CS Retro ${PROJECT_VERSION}")
message(STATUS "  Engine Waf: ${CSRETRO_BUILD_ENGINE} (immer 64-Bit -8)")
message(STATUS "  Xash-Client: ${CSRETRO_BUILD_XASH_CLIENT}")
message(STATUS "  Client-Upstream: ${CSRETRO_BUILD_CLIENT} (muss OFF bleiben)")
message(STATUS "  Server-AMXX-Herkunft: ${CSRETRO_BUILD_SERVER}")
