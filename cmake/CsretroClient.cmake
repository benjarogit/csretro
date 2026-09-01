# Phase 2: nur der Exportvertrag. Phase 3 hängt hier die eine Client-Lib an.
# Kein add_subdirectory(refs/...), kein Body, kein NextClient-MSVC-Upstream.

add_library(csretro_client_export INTERFACE)
target_include_directories(csretro_client_export INTERFACE
    "${CSRETRO_ROOT}/client/export"
)
target_link_libraries(csretro_client_export INTERFACE
    csretro_engine_headers
)
