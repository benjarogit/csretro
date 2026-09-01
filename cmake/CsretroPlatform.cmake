# CS Retro ist 64-Bit-only. Kein stiller 32-Bit-Build auf Desktop-Zielen.
# Matrix: docs/PLATTFORMEN.md

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR
        "CS Retro baut nur 64-Bit (void*=${CMAKE_SIZEOF_VOID_P}). "
        "i386/i686 und andere 32-Bit-Desktop-Targets sind nicht unterstützt.")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _csretro_proc)
if(_csretro_proc MATCHES "^(i[3-6]86|x86|win32)$")
    message(FATAL_ERROR
        "CS Retro lehnt 32-Bit-Prozessor '${CMAKE_SYSTEM_PROCESSOR}' ab. "
        "Desktop: Linux x86_64, Windows x86_64, macOS arm64 oder x86_64.")
endif()

if(DEFINED CSRETRO_ENGINE_64BIT AND NOT CSRETRO_ENGINE_64BIT)
    message(FATAL_ERROR
        "CSRETRO_ENGINE_64BIT=OFF ist ungültig. Die Engine wird immer mit -8 (64-Bit) gebaut.")
endif()
set(CSRETRO_ENGINE_64BIT ON CACHE BOOL "Xash immer 64-Bit (-8)" FORCE)

message(STATUS "CS Retro Plattform: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR} (64-Bit)")
