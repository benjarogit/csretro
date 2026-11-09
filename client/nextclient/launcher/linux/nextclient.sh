#!/bin/bash
# Starts NextClient through the Steam runtime. Starts Steam first when it is
# closed and waits for it.
# Arguments are passed to the game, e.g. ./nextclient.sh -windowed

GAMEROOT=$(cd "${0%/*}" && echo "$PWD")

steam_running() {
    pgrep -u "$(id -u)" -x steam >/dev/null 2>&1
}

start_steam() {
    if steam_running; then
        return 0
    fi

    if command -v steam >/dev/null 2>&1; then
        setsid steam >/dev/null 2>&1 &
    elif command -v flatpak >/dev/null 2>&1 && flatpak info com.valvesoftware.Steam >/dev/null 2>&1; then
        setsid flatpak run com.valvesoftware.Steam >/dev/null 2>&1 &
    elif command -v snap >/dev/null 2>&1 && snap list steam >/dev/null 2>&1; then
        setsid snap run steam >/dev/null 2>&1 &
    else
        echo "nextclient.sh: Steam is not running and no Steam installation was found" >&2
        return 1
    fi

    echo "nextclient.sh: waiting for Steam to start" >&2
    for _ in $(seq 1 90); do
        if steam_running; then
            return 0
        fi
        sleep 1
    done

    echo "nextclient.sh: Steam did not start within 90 seconds" >&2
    return 1
}

for steam_home in "$HOME" "$HOME/snap/steam/common"; do
    for steam_dir in "$steam_home/.steam/steam" "$steam_home/.local/share/Steam"; do
        if [ -x "$steam_dir/ubuntu12_32/steam-runtime/run.sh" ]; then
            STEAM_RUNTIME_RUN="$steam_dir/ubuntu12_32/steam-runtime/run.sh"
            STEAM_HOME="$steam_home"
            break 2
        fi
    done
done

if [ -z "$STEAM_RUNTIME_RUN" ]; then
    echo "nextclient.sh: Steam runtime not found, is Steam installed?" >&2
    exit 1
fi

start_steam || exit 1

# snap's Steam lives in its own HOME, steamclient.so looks for the running Steam there
export HOME="$STEAM_HOME"

# with SteamAppId set hw.so skips writing steam_appid.txt (10 for cstrike), and Steam
# hands out Half-Life auth tickets that servers reject as STEAM_ID_LAN
unset SteamAppId

# set by the snap VS Code terminal, hides the system locales
unset LOCPATH

cd "$GAMEROOT" || exit 1

# run.sh sets LD_LIBRARY_PATH itself, so ours is prepended inside it
exec "$STEAM_RUNTIME_RUN" bash -c \
    'export LD_LIBRARY_PATH="$PWD${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"; exec ./cstrike.exe "$@"' \
    nextclient "$@"
