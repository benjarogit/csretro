# syntax=docker/dockerfile:1

# The base image that only runs SteamCMD. The digest comes from
# versions.lock.json and is passed in by tests/e2e/run.sh; there is no default
# so an unpinned build cannot happen by accident.
ARG STEAMCMD_IMAGE
FROM ${STEAMCMD_IMAGE} AS hlds

USER steam
RUN set -eux; \
    mkdir -p /home/steam/hlds; \
    for attempt in 1 2 3 4; do \
        if /home/steam/steamcmd/steamcmd.sh \
            +force_install_dir /home/steam/hlds \
            +login anonymous \
            +app_update 90 \
            +quit; then \
            break; \
        fi; \
        if [ "$attempt" -eq 4 ]; then \
            echo "App 90 download failed after $attempt attempts" >&2; \
            exit 1; \
        fi; \
        sleep 5; \
    done; \
    test -f /home/steam/hlds/hlds_run

# ReHLDS 3.15 imports the pre-25th-anniversary Steamworks ABI
# (SteamGameServer_Init, SteamGameServer, SteamApps, ISteamGameServer011).
# The Default Public libsteam_api.so only ships the newer 015 interface, so the
# test server answers that ABI with a no-op stub instead of the Steam client.
# See steamcompat/steamstub.cpp.
FROM debian:bookworm-slim AS steamcompat

RUN apt-get update \
    && apt-get install -y --no-install-recommends g++-multilib \
    && rm -rf /var/lib/apt/lists/*

COPY steamcompat/steamstub.cpp /steam/steamstub.cpp
RUN g++ -m32 -O2 -fPIC -shared -fno-exceptions -fno-rtti \
        -Wl,-soname,libcsretro_steamcompat.so \
        -o /steam/libcsretro_steamcompat.so /steam/steamstub.cpp

# Unpack the server runtime on the build architecture. The archives hold 32-bit
# binaries, but unpacking does not execute them.
FROM debian:bookworm-slim AS runtime-files

RUN apt-get update \
    && apt-get install -y --no-install-recommends unzip \
    && rm -rf /var/lib/apt/lists/*

COPY archives/ /archives/
RUN set -eux; \
    mkdir -p /out/cstrike; \
    unzip -q /archives/rehlds-bin-*.zip -d /tmp/rehlds; \
    cp -a /tmp/rehlds/bin/linux32/. /out/; \
    unzip -q /archives/regamedll-bin-*.zip -d /tmp/regamedll; \
    cp -a /tmp/regamedll/bin/linux32/cstrike/. /out/cstrike/; \
    unzip -q /archives/metamod-bin-*.zip 'addons/metamod/*' -d /out; \
    tar xzf /archives/amxmodx-*-base-linux.tar.gz -C /out; \
    tar xzf /archives/amxmodx-*-cstrike-linux.tar.gz -C /out; \
    unzip -q /archives/reapi-bin-*.zip -d /out; \
    mkdir -p /out/cstrike/addons; \
    mv /out/addons/amxmodx /out/cstrike/addons/amxmodx

# The module is built against a current glibc, so the runtime has to be at least
# as new as the build host. Debian 12 (glibc 2.36) is too old for the
# GLIBC_ABI_GNU_TLS marker; Debian 13 carries glibc 2.41.
FROM i386/debian:trixie-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends libstdc++6 python3-minimal \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 1000 csretro

COPY --from=hlds --chown=csretro:csretro /home/steam/hlds/ /server/
COPY --from=runtime-files --chown=csretro:csretro /out/ /server/
COPY --from=steamcompat --chown=csretro:csretro /steam/libcsretro_steamcompat.so /server/
COPY --chown=csretro:csretro module/nextclientapi_amxx_i386.so /server/cstrike/addons/amxmodx/modules/
COPY --chown=csretro:csretro plugins/test_nextclient_api.amxx /server/cstrike/addons/amxmodx/plugins/
COPY --chown=csretro:csretro assets/ /server/cstrike/
COPY --chown=csretro:csretro amxx/modules.ini /server/cstrike/addons/amxmodx/configs/modules.ini
COPY --chown=csretro:csretro amxx/plugins.ini /server/cstrike/addons/amxmodx/configs/plugins.ini
COPY --chown=csretro:csretro metamod/config.ini /server/addons/metamod/config.ini
COPY --chown=csretro:csretro addons/plugins.ini /server/addons/plugins.ini
COPY --chown=csretro:csretro cstrike/liblist.gam /server/cstrike/liblist.gam
COPY --chown=csretro:csretro entrypoint.sh /entrypoint.sh
COPY --chown=csretro:csretro healthcheck.py /healthcheck.py

# ReHLDS archives do not carry the executable bit for the engine binaries, and
# AMX Mod X expects its log directory to exist.
RUN chmod +x /entrypoint.sh /server/hlds_linux /server/hlds_run /server/hltv \
    && mkdir -p /server/cstrike/addons/amxmodx/logs

USER csretro
WORKDIR /server
EXPOSE 27015/udp

HEALTHCHECK --interval=5s --timeout=3s --start-period=20s --retries=24 \
    CMD ["python3", "/healthcheck.py"]

ENTRYPOINT ["/entrypoint.sh"]
