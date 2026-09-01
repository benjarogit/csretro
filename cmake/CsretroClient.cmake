# Eine 64-Bit-Client-Lib unter Xash. Kein add_subdirectory(refs/...), kein NextClient-MSVC.

add_library(csretro_client_export INTERFACE)
target_include_directories(csretro_client_export INTERFACE
    "${CSRETRO_ROOT}/client/export"
)
# Kein csretro_engine_headers am Body: Xash-Header (STATIC_CHECK_SIZEOF) zerlegen den Compile.

option(CSRETRO_BUILD_XASH_CLIENT "CS-Retro-Client (Export + A1-Body) gegen Xash" ON)

if(CSRETRO_BUILD_XASH_CLIENT)
    add_subdirectory("${CSRETRO_ROOT}/client/body" "${CMAKE_BINARY_DIR}/client")
endif()
