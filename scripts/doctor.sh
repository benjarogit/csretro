#!/usr/bin/env bash
# CSRETRO toolchain doctor.
#
# Read-only. Checks the host against the requirements in
# docs/en/build/prerequisites.md and docs/de/build/prerequisites.md.
# It never installs anything and never starts the game.
#
# Exit 0: build prerequisites are met (warnings allowed).
# Exit 1: at least one hard requirement is missing.
set -uo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

PASS=0
FAIL=0
WARN=0
MISSING=()

pass() { PASS=$((PASS + 1)); printf '  [ OK ] %s%s\n' "$1" "${2:+ — $2}"; }
fail() {
    FAIL=$((FAIL + 1))
    printf '  [FAIL] %s%s\n' "$1" "${2:+ — $2}"
    [[ -n "${3:-}" ]] && printf '         %s\n' "$3"
    return 0
}
warn() {
    WARN=$((WARN + 1))
    printf '  [WARN] %s%s\n' "$1" "${2:+ — $2}"
    [[ -n "${3:-}" ]] && printf '         %s\n' "$3"
    return 0
}
section() { printf '\n== %s\n' "$1"; }

have() { command -v "$1" >/dev/null 2>&1; }

# version_ge A B -> true when A >= B (sort -V semantics)
version_ge() {
    [[ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | head -n 1)" == "$2" ]]
}

version_cmd() {
    local cmd="$1"
    shift
    "$cmd" "$@" 2>/dev/null | head -n 1
}

check_version() {
    local label="$1" found="$2" minimum="$3" pkg="$4"
    if [[ -z "$found" ]]; then
        fail "$label" "not found" "install: $pkg"
        MISSING+=("$pkg")
    elif version_ge "$found" "$minimum"; then
        pass "$label" "$found (>= $minimum)"
    else
        fail "$label" "$found is too old (need >= $minimum)" "upgrade: $pkg"
        MISSING+=("$pkg")
    fi
}

check_tool() {
    local tool="$1" pkg="${2:-$1}"
    if have "$tool"; then
        pass "$tool" "$(command -v "$tool")"
    else
        fail "$tool" "not found" "install: $pkg"
        MISSING+=("$pkg")
    fi
}

section "host"
host_arch="$(uname -m)"
if [[ "$host_arch" == "x86_64" ]]; then
    pass "architecture" "$host_arch"
else
    fail "architecture" "$host_arch" "CSRETRO needs an x86_64 host with 32-bit multilib"
fi

if [[ -f /etc/os-release ]]; then
    # shellcheck disable=SC1091
    os_name="$(. /etc/os-release && printf '%s' "${PRETTY_NAME:-$NAME}")"
    pass "operating system" "$os_name"
else
    warn "operating system" "unknown" "/etc/os-release is missing"
fi

if have pacman; then
    DISTRO_HINT="sudo pacman -S --needed"
elif have apt-get; then
    DISTRO_HINT="sudo apt-get install"
else
    DISTRO_HINT=""
fi

section "core toolchain"
check_version "glibc" "$(ldd --version 2>/dev/null | head -n 1 | grep -oE '[0-9]+\.[0-9]+$')" "2.43" "glibc"
check_version "gcc" "$(gcc -dumpfullversion 2>/dev/null)" "15" "gcc"
check_tool "g++" "gcc"
check_version "cmake" "$(cmake --version 2>/dev/null | awk 'NR==1{print $3}')" "3.21" "cmake"
check_version "ninja" "$(ninja --version 2>/dev/null)" "1.11" "ninja"
check_version "python3" "$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])' 2>/dev/null)" "3.12" "python"
check_tool "git" "git"
check_tool "git-lfs" "git-lfs"

section "build helpers"
for tool in curl zip unzip tar patch make pkg-config perl; do
    pkg="$tool"
    [[ "$tool" == "pkg-config" ]] && pkg="pkgconf"
    check_tool "$tool" "$pkg"
done

if have uv; then
    pass "uv" "$(uv --version 2>/dev/null)"
else
    warn "uv" "not found" "optional, used by the Python tool environments"
fi

section "client target (x86_64)"
if ! have gcc; then
    fail "client probe" "gcc is missing"
else
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT

    cat >"$tmp/probe.c" <<'EOF'
#include <GL/gl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xext.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/scrnsaver.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xinerama.h>
int main(void) { return 0; }
EOF

    if gcc "$tmp/probe.c" -o "$tmp/probe" \
        -lGL -lX11 -lXext -lXcursor -lXi -lXrandr -lXss -lXfixes -lXrender -lXinerama 2>"$tmp/link.log"; then
        pass "C compile and link" "x86_64"
    else
        fail "C compile and link" "x86_64 link failed" "$(head -n 3 "$tmp/link.log")"
    fi

    if [[ -f "$tmp/probe" ]] && readelf -h "$tmp/probe" 2>/dev/null | grep -q 'ELF64'; then
        pass "probe binary" "ELF64"
    elif [[ -f "$tmp/probe" ]]; then
        fail "probe binary" "not ELF64"
    fi

    cat >"$tmp/probe.cpp" <<'EOF'
#include <string>
int main() { std::string s = "csretro"; return s.size() == 7 ? 0 : 1; }
EOF
    if g++ "$tmp/probe.cpp" -o "$tmp/probe_cpp" 2>"$tmp/link_cpp.log"; then
        pass "C++ compile and link" "x86_64 libstdc++"
    else
        fail "C++ compile and link" "x86_64 failed" "$(head -n 3 "$tmp/link_cpp.log")"
    fi

    for lib in GL X11 Xext Xcursor Xi Xrandr Xinerama Xss Xfixes Xrender; do
        resolved="$(gcc -print-file-name="lib${lib}.so" 2>/dev/null)"
        if [[ "$resolved" == /* && -e "$resolved" ]]; then
            pass "lib${lib}.so" "$resolved"
        else
            fail "lib${lib}.so" "not found" "install the native ${lib} package"
        fi
    done
fi

section "client runtime (offscreen rendering)"
swrast=""
for dir in /usr/lib/dri /usr/lib/x86_64-linux-gnu/dri; do
    [[ -d "$dir" ]] || continue
    candidate="$(find "$dir" -maxdepth 1 -name 'swrast_dri.so' -print -quit 2>/dev/null)"
    if [[ -n "$candidate" ]]; then
        swrast="$candidate"
        break
    fi
done
if [[ -n "$swrast" ]]; then
    pass "Mesa swrast (llvmpipe)" "$swrast"
else
    fail "Mesa swrast (llvmpipe)" "no swrast driver" "install: mesa"
fi

section "server module toolchain (i386)"
server_tmp="$(mktemp -d)"
if g++ -m32 -x c++ -o "$server_tmp/probe" - <<<"int main(){return 0;}" >/dev/null 2>&1 \
    && readelf -h "$server_tmp/probe" 2>/dev/null | grep -q 'ELF32'; then
    pass "server module" "ELF32"
else
    fail "server module" "-m32 failed" "the AMXX module is still i386"
fi
rm -rf "$server_tmp"

steam_runtime=""
for dir in "$HOME/.steam/steam/ubuntu12_32/steam-runtime" "$HOME/.local/share/Steam/ubuntu12_32/steam-runtime"; do
    [[ -d "$dir" ]] && steam_runtime="$dir" && break
done
if [[ -n "$steam_runtime" ]]; then
    pass "Steam runtime" "$steam_runtime"
else
    warn "Steam runtime" "not found" "needed to run the retail game, not to build"
fi

section "repository"
if [[ -d .git ]]; then
    pass "git repository" "$ROOT"
else
    fail "git repository" "not a git checkout"
fi

if [[ -f client/vcpkg/bootstrap-vcpkg.sh ]]; then
    pass "vcpkg submodule" "client/vcpkg"
else
    fail "vcpkg submodule" "missing" "run: git submodule update --init --recursive"
fi

if [[ -x client/vcpkg/vcpkg ]]; then
    pass "vcpkg binary" "$(client/vcpkg/vcpkg version 2>/dev/null | head -n 1)"
else
    warn "vcpkg binary" "not bootstrapped" "run: scripts/bootstrap.sh"
fi

if [[ -f client/vcpkg/triplets/x64-linux.cmake ]]; then
    pass "vcpkg triplet" "x64-linux"
else
    fail "vcpkg triplet" "client/vcpkg/triplets/x64-linux.cmake is missing"
fi

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    uninitialised="$(git submodule status 2>/dev/null | awk '$1 ~ /^-/ { print $2 }')"
    if [[ -z "$uninitialised" ]]; then
        pass "submodules" "all initialised"
    else
        warn "submodules" "not initialised" "run: git submodule update --init --recursive"
    fi
fi

section "container (optional, needed for the test server)"
if have docker; then
    pass "docker" "$(version_cmd docker --version)"
    if docker compose version >/dev/null 2>&1; then
        pass "docker compose" "$(version_cmd docker compose version)"
    else
        warn "docker compose" "not available" "the test server needs Compose v2: tests/e2e/run.sh"
    fi
    if docker info >/dev/null 2>&1; then
        pass "docker daemon" "reachable"
    else
        warn "docker daemon" "not reachable" "start the daemon or add your user to the docker group"
    fi
else
    warn "docker" "not found" "only needed for tests/e2e/run.sh; the host builds work without it"
fi

section "disk"
free_kb="$(df -Pk "$ROOT" 2>/dev/null | awk 'NR==2 { print $4 }')"
if [[ -n "$free_kb" ]]; then
    free_gb=$((free_kb / 1024 / 1024))
    if ((free_gb >= 20)); then
        pass "free space" "${free_gb} GB (>= 20 GB)"
    else
        fail "free space" "${free_gb} GB (need >= 20 GB)" "vcpkg and the build trees are large"
    fi
else
    warn "free space" "could not determine"
fi

printf '\n== summary\n'
printf '  %d passed, %d warnings, %d failed\n' "$PASS" "$WARN" "$FAIL"

if ((${#MISSING[@]} > 0)); then
    # Deduplicate while preserving order.
    mapfile -t MISSING < <(printf '%s\n' "${MISSING[@]}" | awk '!seen[$0]++')
    printf '\n  missing packages: %s\n' "${MISSING[*]}"
    if [[ -n "$DISTRO_HINT" ]]; then
        printf '  install with: %s %s\n' "$DISTRO_HINT" "${MISSING[*]}"
    fi
    printf '  or run: scripts/bootstrap.sh --install-deps\n'
fi

if ((FAIL > 0)); then
    printf '\nDoctor found blocking problems.\n'
    exit 1
fi

printf '\nAll build prerequisites are met.\n'
