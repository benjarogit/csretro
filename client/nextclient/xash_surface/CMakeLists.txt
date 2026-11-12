cmake_minimum_required(VERSION 3.21)
project(xash_surface)

find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(XASH_SURFACE_SDK "${NEXTCLIENT_ROOT}/dep/ncl-hl1-source-sdk/public")

add_library(xash_surface SHARED
        src/XashSurface.cpp
        src/font_face.cpp)
add_library(xash::surface ALIAS xash_surface)

target_include_directories(xash_surface
        PUBLIC include
        PRIVATE
        src
        "${CMAKE_CURRENT_SOURCE_DIR}/../xash_engine/include"
        "${XASH_SURFACE_SDK}"
        "${XASH_SURFACE_SDK}/tier0"
        "${XASH_SURFACE_SDK}/tier1")

target_compile_definitions(xash_surface PRIVATE
        NO_MALLOC_OVERRIDE
        MATHLIB_HEADER_ONLY
        "_DLL_EXT=.so")

if (UNIX AND NOT APPLE)
    target_compile_definitions(xash_surface PRIVATE GNUC POSIX LINUX _LINUX)
    target_link_libraries(xash_surface PRIVATE OpenGL::GL Freetype::Freetype ${CMAKE_DL_LIBS})
elseif (WIN32)
    target_link_libraries(xash_surface PRIVATE opengl32 Freetype::Freetype)
endif ()

add_custom_command(TARGET xash_surface POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
        "$<TARGET_FILE_DIR:xash_surface>/fonts"
        VERBATIM)

set_target_properties(xash_surface PROPERTIES
        PREFIX ""
        OUTPUT_NAME "xash_surface"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)

if (NEXTCLIENT_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)

    add_executable(xash_surface-tests
            tests/SurfaceMapTests.cpp
            tests/FontAtlasTests.cpp
            tests/SurfaceApiTests.cpp
            tests/SurfaceWalkTests.cpp
            tests/SurfaceInputTests.cpp
            tests/SurfaceFileTests.cpp
            tests/SurfaceGlTests.cpp)
    target_include_directories(xash_surface-tests PRIVATE
            src
            "${XASH_SURFACE_SDK}"
            "${XASH_SURFACE_SDK}/tier0"
            "${XASH_SURFACE_SDK}/tier1")
    target_compile_definitions(xash_surface-tests PRIVATE
            NO_MALLOC_OVERRIDE
            MATHLIB_HEADER_ONLY
            "_DLL_EXT=.so")
    if (UNIX AND NOT APPLE)
        target_compile_definitions(xash_surface-tests PRIVATE GNUC POSIX LINUX _LINUX)
    endif ()
    target_link_libraries(xash_surface-tests PRIVATE
            xash_surface
            GTest::gtest_main
            OpenGL::GL
            ${CMAKE_DL_LIBS})
    gtest_discover_tests(xash_surface-tests
            PROPERTIES LABELS gl
            ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
endif ()
