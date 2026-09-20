# Eine 64-Bit-Client-Lib: Xash-Export + A1-Body.
# Kein refs/, kein NextClient-Feature, kein Steam, kein zweites Client-Target.
project(csretro_client LANGUAGES C CXX)

include(CheckSymbolExists)
check_symbol_exists(strlcpy string.h HAVE_STRLCPY)
check_symbol_exists(strlcat string.h HAVE_STRLCAT)

set(BODY "${CMAKE_CURRENT_SOURCE_DIR}")
set(EXPORT_DIR "${CSRETRO_ROOT}/client/export")
set(RENDER_DIR "${CSRETRO_ROOT}/client/render")

file(GLOB CS_CLIENT_SRC "${BODY}/cl_dll/*.cpp")
file(GLOB CS_EV_SRC "${BODY}/cl_dll/events/*.cpp")
file(GLOB CS_WPN_SRC "${BODY}/cl_dll/cs_wpn/*.cpp")
file(GLOB CS_WPNSH_SRC "${BODY}/dlls/wpn_shared/*.cpp")
file(GLOB CS_HUD_SRC "${BODY}/cl_dll/hud/*.cpp")
file(GLOB CS_STUDIO_SRC "${BODY}/cl_dll/studio/*.cpp")
file(GLOB CS_INPUT_SRC "${BODY}/cl_dll/input/*.cpp")
file(GLOB CS_PM_SRC "${BODY}/pm_shared/*.cpp")
file(GLOB CS_GAMSH_SRC "${BODY}/game_shared/*.cpp")
file(GLOB MINIUTL_SRC "${BODY}/miniutl/*.cpp")
file(GLOB CS_PARTICLEMAN_SRC "${BODY}/cl_dll/particleman/*.cpp")

set(CS_SOURCES
    ${CS_CLIENT_SRC}
    ${CS_EV_SRC}
    ${CS_WPN_SRC}
    ${CS_WPNSH_SRC}
    ${CS_HUD_SRC}
    ${CS_STUDIO_SRC}
    ${CS_PM_SRC}
    ${CS_INPUT_SRC}
    ${CS_GAMSH_SRC}
    ${MINIUTL_SRC}
    ${CS_PARTICLEMAN_SRC}
    "${BODY}/common/interface.cpp"
    "${BODY}/public/utflib.cpp"
    "${EXPORT_DIR}/csretro_cdll_export.cpp"
    "${RENDER_DIR}/render_core.cpp"
    "${RENDER_DIR}/render_backend.cpp"
    "${RENDER_DIR}/render_world.cpp"
    "${RENDER_DIR}/render_bsp_mesh.cpp"
    "${RENDER_DIR}/render_scene.cpp"
    "${RENDER_DIR}/render_sprite.cpp"
    "${RENDER_DIR}/render_studio.cpp"
    "${RENDER_DIR}/render_brush.cpp"
    "${RENDER_DIR}/render_decal.cpp"
    "${RENDER_DIR}/render_dlight.cpp"
    "${RENDER_DIR}/render_vis.cpp"
    "${RENDER_DIR}/render_trans.cpp"
)

if(NOT HAVE_STRLCPY)
    list(APPEND CS_SOURCES "${BODY}/public/strlcpy.c")
endif()
if(NOT HAVE_STRLCAT)
    list(APPEND CS_SOURCES "${BODY}/public/strlcat.c")
endif()

add_library(csretro_client SHARED ${CS_SOURCES})

target_include_directories(csretro_client PRIVATE
    "${BODY}/cl_dll/include"
    "${BODY}/cl_dll/include/hud"
    "${BODY}/cl_dll/include/studio"
    "${BODY}/cl_dll/include/math"
    "${BODY}/cl_dll"
    "${BODY}/common"
    "${BODY}/engine"
    "${BODY}/pm_shared"
    "${BODY}/dlls"
    "${BODY}/game_shared"
    "${BODY}/public"
    "${BODY}/public/cl_dll"
    "${BODY}/miniutl"
    "${BODY}/cl_dll/particleman"
    "${EXPORT_DIR}"
    "${RENDER_DIR}"
)

target_compile_definitions(csretro_client PRIVATE
    CLIENT_WEAPONS
    CLIENT_DLL
    STDINT_H=<cstdint>
)

if(HAVE_STRLCPY)
    target_compile_definitions(csretro_client PRIVATE HAVE_STRLCPY)
endif()
if(HAVE_STRLCAT)
    target_compile_definitions(csretro_client PRIVATE HAVE_STRLCAT)
endif()

if(NOT MSVC)
    # GoldSrc-Quellen prüfen oft LINUX, nicht __APPLE__. POSIX-Hosts teilen diese Defines.
    target_compile_definitions(csretro_client PRIVATE
        LINUX
        _LINUX
        stricmp=strcasecmp
        _strnicmp=strncasecmp
        strnicmp=strncasecmp
    )
    target_compile_options(csretro_client PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-fms-extensions>
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-unused-but-set-variable
        -Wno-missing-field-initializers
        -Wno-format-nonliteral
        -Wno-format-security
        -Wno-sign-compare
        -Wno-implicit-fallthrough
        -Wno-unknown-pragmas
        -fno-strict-aliasing
    )
    if(NOT APPLE)
        target_link_options(csretro_client PRIVATE -Wl,--no-undefined)
    endif()
endif()

if(UNIX)
    target_link_libraries(csretro_client PRIVATE m)
endif()

# Xash LibraryNaming: 64-Bit-Suffix Pflicht. 32-Bit-Namen (client.so ohne Suffix) gibt es nicht.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(_csretro_client_postfix "_arm64")
else()
    set(_csretro_client_postfix "_amd64")
endif()

set_target_properties(csretro_client PROPERTIES
    OUTPUT_NAME "client${_csretro_client_postfix}"
    PREFIX ""
    CXX_VISIBILITY_PRESET default
    C_VISIBILITY_PRESET default
)

if(UNIX AND NOT APPLE)
    set_target_properties(csretro_client PROPERTIES SUFFIX ".so")
endif()
