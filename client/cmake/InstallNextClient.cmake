cmake_minimum_required(VERSION 3.21)

if (NOT DEFINED INSTALL_MANIFEST OR NOT EXISTS "${INSTALL_MANIFEST}")
    message(FATAL_ERROR "InstallNextClient: INSTALL_MANIFEST must name a prepared path manifest")
endif ()
include("${INSTALL_MANIFEST}")

if (NOT INSTALL_PLATFORM STREQUAL Windows AND NOT INSTALL_PLATFORM STREQUAL Linux)
    message(FATAL_ERROR "InstallNextClient: unsupported INSTALL_PLATFORM '${INSTALL_PLATFORM}'")
endif ()
list(LENGTH INSTALL_LIBRARY_SOURCES library_count)
if (NOT library_count EQUAL 8)
    message(FATAL_ERROR "InstallNextClient: expected eight shipping library sources")
endif ()

set(DIRECTORY "${INSTALL_ASSETS_DIRECTORY}/cstrike/materials")
include("${CMAKE_CURRENT_LIST_DIR}/CheckLfsPointers.cmake")

set(destinations cstrike.exe next_engine_mini.dll FileSystem_Proxy.dll cstrike/cl_dlls/client_mini.dll)
if (INSTALL_PLATFORM STREQUAL Windows)
    list(APPEND destinations cstrike/cl_dlls/GameUI.dll steam_api.dll nitro_api2.dll vgui2.dll)
else ()
    # hw.so loads GameUI by this exact name; preserve Valve's original VGUI library.
    list(APPEND destinations cstrike/cl_dlls/gameui.so steam_api.dll nitro_api2.dll vgui2.so)
    include("${CMAKE_CURRENT_LIST_DIR}/BackupValveLibrary.cmake")
    BackupValveLibrary("${INSTALL_DIRECTORY}/vgui2.so")

    # client_mini needs GetCPUInformation, which the retail tier0 library does not
    # export. Keep Valve's copy and install the SDK tier0, a superset of it.
    if (NOT EXISTS "${INSTALL_TIER0_SOURCE}")
        message(FATAL_ERROR "InstallNextClient: INSTALL_TIER0_SOURCE must name the SDK tier0 library")
    endif ()
    BackupValveLibrary("${INSTALL_DIRECTORY}/libtier0.so")
    file(COPY_FILE "${INSTALL_TIER0_SOURCE}" "${INSTALL_DIRECTORY}/libtier0.so")
endif ()

file(MAKE_DIRECTORY "${INSTALL_DIRECTORY}/cstrike/cl_dlls")
foreach (index RANGE 0 7)
    list(GET INSTALL_LIBRARY_SOURCES ${index} source)
    list(GET destinations ${index} destination)
    file(COPY_FILE "${source}" "${INSTALL_DIRECTORY}/${destination}")
endforeach ()

if (INSTALL_PLATFORM STREQUAL Windows)
    file(COPY_FILE "${INSTALL_CRASHPAD_SOURCE}" "${INSTALL_DIRECTORY}/crashpad_handler.exe")
else ()
    file(COPY_FILE "${INSTALL_LINUX_LAUNCHER_SOURCE}" "${INSTALL_DIRECTORY}/nextclient.sh")
endif ()
file(COPY "${INSTALL_ASSETS_DIRECTORY}/" DESTINATION "${INSTALL_DIRECTORY}")

if (INSTALL_TRACY_SOURCE)
    get_filename_component(tracy_filename "${INSTALL_TRACY_SOURCE}" NAME)
    file(COPY_FILE "${INSTALL_TRACY_SOURCE}" "${INSTALL_DIRECTORY}/${tracy_filename}")
endif ()
