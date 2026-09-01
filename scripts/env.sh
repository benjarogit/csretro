# shellcheck shell=bash
# Gemeinsame Umgebung für CMake/Waf auf diesem Host.

# CachyOS/CMake 4.4.3 findet CMAKE_ROOT sonst nicht.
export CMAKE_ROOT="${CMAKE_ROOT:-/usr/share/cmake}"
export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"
export CMAKE_GENERATOR="${CMAKE_GENERATOR:-Ninja}"
