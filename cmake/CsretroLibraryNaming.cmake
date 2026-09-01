# Xash LibraryNaming für Win/Lin/Mac, 64-Bit.
# Quelle: engine/Documentation/extensions/library-naming.md
# $ext = CMAKE_SHARED_LIBRARY_SUFFIX (dll / so / dylib). Nicht erfinden.

function(csretro_xash_lib_name out_name base)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(_arch "arm64")
    else()
        set(_arch "amd64")
    endif()
    set(${out_name} "${base}_${_arch}" PARENT_SCOPE)
endfunction()
