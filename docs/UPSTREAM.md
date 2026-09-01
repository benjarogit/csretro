# Upstream-Pins (Vendor, kein Submodule)

**Unser Origin:** https://github.com/benjarogit/csretro (`main`, privat).
Import 2026-09-01, shallow clone, `.git` entfernt. Kein `git submodule`.

Altes Remote-`main` (Xash+cs16-client bis `8ece11c`, Tag `v0.2.0`) ist kein Upstream mehr.

## Basis

| Baum | Repo | Commit | Datum |
|------|------|--------|-------|
| `client/` | https://github.com/CS-NextClient/NextClient | `f5addc2ba0276d30d26c40eeb8d51fb9fc416102` | 2026-08-02 |
| `client/dep/NclNitroApi/` | https://github.com/CS-NextClient/NclNitroApi | `f73fc1a7592ba413aa382b5b3857e4a40b1b545e` | (von NextClient gepinnt, nicht HEAD) |
| `client/dep/NclNitroApi/dep/ncl-hl1-source-sdk/` | https://github.com/CS-NextClient/ncl-hl1-source-sdk | `46c310389d669685ec953b8c23f651bd60465e19` | 2026-06-24 |
| `server/` | https://github.com/CS-NextClient/NextClientServerApi | `1c7e5c61191f1b949a28cade96dc1d821eedb335` | 2026-07-01 |

## Ziel-Engine

| Baum | Repo | Commit | Datum |
|------|------|--------|-------|
| `engine/` | https://github.com/FWGS/xash3d-fwgs | `1442d14a69093780389104dcb7369aa3685945cf` | 2026-08-27 |

Engine-3rdparty (mitimportiert, kein Submodule mehr): MultiEmulator, bzip2, xash-extras, gl-wes-v2, gl4es, libbacktrace, libogg, library_suffix, maintui, mainui (+ miniutl), mbedtls, nanogl, opus, opusfile, vgui_support (+ vgui-dev), vorbis.

## Referenzen

Ref A: Body-Quelle A1 (Allowlist). Pin unverändert, bis Phase 3 vendort. Dann neuer Pin unter `client/body/` + Attribution.

| Baum | Repo | Commit | Datum |
|------|------|--------|-------|
| `refs/a-cs16-client/` | https://github.com/Velaron/cs16-client | `bb60674c120ae9bf8fa7854018bea8a77e71c17f` | 2026-08-24 |
| `refs/b-cs16-goldsrc/` | https://github.com/FuryBaM/cs16-goldsrc-client | `b662acca3ce74c2c9851cc842592c58661d95799` | 2026-08-27 |

## Bewusst nicht vendort

- `microsoft/vcpkg` (NextClient- und NitroApi-Submodule) — Package-Manager, Windows-Toolchain
- Spielinhalte `valve/` / `cstrike/` — Valve-Copyright, lokal unter `gamedata/` ablegen

## Auffrischen

Nicht automatisch. Neuer Import = neuer Pin in dieser Datei + Review, welche Dateien sich geändert haben. Kein `git submodule update`.
