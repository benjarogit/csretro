cmake_minimum_required(VERSION 3.10)
project(rsa_keygen)

add_executable(${PROJECT_NAME} main.cpp)

# Upstream referenced OPENSSL_INCLUDE_PATHS / OPENSSL_STATIC_LIBS, which nothing
# defines; the target from deps/openssl already carries both.
target_link_libraries(${PROJECT_NAME} PRIVATE openssl::openssl)

# The vendored OpenSSL archives are 32-bit, so this helper has to match. Upstream
# only applied -m32 to the AMXX module and left the tool on the host ABI.
if (UNIX)
    target_compile_options(${PROJECT_NAME} PRIVATE -m32)
    target_link_options(${PROJECT_NAME} PRIVATE -m32)
endif ()
