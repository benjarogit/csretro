# Client runtime

CSRETRO runs only on the **Default Public branch** of Counter-Strike 1.6. The
client identifies the installed build before it hooks any engine or client
function.

## Build detection

The build is determined from the content of the two retail binaries, not from a
file size or a version string:

| File | Role |
|---|---|
| `hw.so` | Engine (Linux) |
| `cstrike/cl_dlls/client.so` | Game client |

At startup the client computes the SHA-256 of both files and compares the pair
with the build registry in `client/nextclient/nitro_api/src/build_registry.cpp`.
Only an exact match yields a build identity. The registry currently contains:

| ID | Platform | Steam build |
|---|---|---|
| `public-linux-1.0.1.4` | Linux | 12934623 |

## Unknown build

If the pair matches no entry, the client logs both digests and disables the
hooks. There is no fallback to another build and no size-based guess. An unknown
build means CSRETRO does not work until the digests are checked and an entry is
added.

## Address resolution

The public providers live in `EngineAddressProviderPublicLinux.cpp` and
`ClientAddressProviderPublicLinux.cpp`. Every entry is resolved in three stages:

1. **dynsym** – `dlsym` through the dynamic symbol table entry.
2. **symtab** – the full ELF symbol table of the file on disk.
3. **pattern** – a byte pattern as the last resort.

On the public build the engine functions exist only in `symtab`; `dynsym`
contains mostly C++ libraries. Stage 2 therefore carries the resolution. The
provider tables cover 327 engine entries and 81 client entries; every name was
checked against the retail binaries.

`EngineMsgBase` is the `cl_parsefuncs` table; its layout
`{opcode, pszname, pfnParse}` matches `goldsrc::EngineMsg`.

## Adding a build

1. Take both files from an unmodified installation.
2. Run `sha256sum hw.so cstrike/cl_dlls/client.so`.
3. Add an entry with ID, Steam build number, platform and both digests to
   `build_registry.cpp`.
4. Run `nitro_api-tests`; the registry tests cover unknown, incomplete and
   platform-mismatched fingerprints.

A new build also needs its own address provider whose symbol names are checked
against the new binaries. The resolver itself stays unchanged.
