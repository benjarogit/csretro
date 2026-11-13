# One fingerprint registry shared by the hook provider and original-client bridge.
get_filename_component(nitro_registry_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
add_library(nitro_build_registry STATIC "${nitro_registry_root}/src/build_registry.cpp")
target_include_directories(nitro_build_registry PUBLIC "${nitro_registry_root}/src" "${nitro_registry_root}/include")
target_compile_features(nitro_build_registry PRIVATE cxx_std_17)
target_link_libraries(nitro_build_registry PRIVATE data_encoding::data_encoding)
set_target_properties(nitro_build_registry PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden)
