add_library(goldsrc_abi INTERFACE)
add_library(goldsrc::abi ALIAS goldsrc_abi)

target_include_directories(goldsrc_abi INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(goldsrc_abi INTERFACE cxx_std_23)

# studio_layout.cpp checks on-disk studio records. golden_layout.cpp records the
# i386 in-memory engine ABI and is compiled only for a 32-bit process.
add_library(goldsrc_abi_golden OBJECT tests/studio_layout.cpp)
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_sources(goldsrc_abi_golden PRIVATE tests/golden_layout.cpp)
endif ()
target_link_libraries(goldsrc_abi_golden PRIVATE goldsrc::abi)

if (MSVC)
    target_compile_options(goldsrc_abi_golden PRIVATE /W4)
endif ()
