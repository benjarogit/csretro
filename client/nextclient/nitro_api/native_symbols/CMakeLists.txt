get_filename_component(nitro_symbols_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
add_library(nitro_native_symbols STATIC "${nitro_symbols_root}/src/elf_symbols.cpp")
target_include_directories(nitro_native_symbols PUBLIC "${nitro_symbols_root}/src")
target_compile_features(nitro_native_symbols PRIVATE cxx_std_17)
target_link_libraries(nitro_native_symbols PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(nitro_native_symbols PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden)
