# Eine 64-Bit-Menü-Lib: GetMenuAPI + CreateInterface (GameMenuExports001).
option(CSRETRO_BUILD_MENU "CS-Retro-Menü-Lib gegen Xash" ON)

if(CSRETRO_BUILD_MENU)
    add_subdirectory("${CSRETRO_ROOT}/client/menu" "${CMAKE_BINARY_DIR}/menu")
endif()
