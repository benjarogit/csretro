# Einzige erlaubte Include-Brücken zwischen den Bereichen.
# Kein add_subdirectory(refs/...).

add_library(csretro_engine_headers INTERFACE)
target_include_directories(csretro_engine_headers INTERFACE
    "${CSRETRO_ROOT}/engine/common"
    "${CSRETRO_ROOT}/engine/public"
    "${CSRETRO_ROOT}/engine/pm_shared"
    "${CSRETRO_ROOT}/engine/engine"
)

add_library(csretro_client_sdk_headers INTERFACE)
target_include_directories(csretro_client_sdk_headers INTERFACE
    "${CSRETRO_ROOT}/client/dep/NclNitroApi/dep/ncl-hl1-source-sdk/public"
    "${CSRETRO_ROOT}/client/dep/NclNitroApi/include"
)
