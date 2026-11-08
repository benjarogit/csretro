#!/bin/sh
# Start the CSRETRO test server. Every value has a deterministic default so the
# container is useful without a compose file, but tests override them.
set -eu

: "${CSR_MAP:=de_dust2}"
: "${CSR_MAXPLAYERS:=16}"
: "${CSR_PORT:=27015}"
: "${CSR_RCON_PASSWORD:=csretro-e2e}"

# hlds_linux is started directly instead of through hlds_run: the stub only has
# to be preloaded into the engine process, and no restart wrapper means the
# container exits when the server exits.
#
# The stub answers the SteamGameServer011 ABI that ReHLDS was built against; see
# steamcompat/steamstub.cpp. -insecure disables VAC: this server is a local test
# target, never a public one. sv_lan 1 keeps it off the master servers, so
# -nomaster is neither needed nor used.
exec env \
    LD_LIBRARY_PATH=. \
    LD_PRELOAD=/server/libcsretro_steamcompat.so \
    ./hlds_linux \
    -game cstrike \
    -insecure \
    -port "$CSR_PORT" \
    +maxplayers "$CSR_MAXPLAYERS" \
    +map "$CSR_MAP" \
    +rcon_password "$CSR_RCON_PASSWORD" \
    +sv_lan 1 \
    +log on
