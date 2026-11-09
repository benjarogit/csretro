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

## Checking the providers

`tools/check_address_providers.py` compares both providers with an installation:

```bash
python3 tools/check_address_providers.py --game-dir /path/to/Half-Life
```

The tool checks every symbol name against `dynsym` and `symtab` and counts every
string literal in the binary. A missing symbol, a non-unique string or an
unsupported search type makes it exit with code 1. That is the evidence to run
after a Steam update and before adding a new provider.

Three entries differ from the rule:

| Entry | Type | Reason |
|---|---|---|
| `isFogEnabled.26640` | symbol | compiler-generated name of a static local; present only under this name in the public build |
| `R_ForceCVars.part.3` | symbol | compiler-generated part of a function; present only under this name in the public build |
| `GL_ARB_multitexture ` | string | occurs exactly once in `hw.so` |

The two generated names are bound to this exact binary. The build is pinned by
SHA-256; another build gets its own provider, and the tool reports every
mismatch before the client starts.

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

## First start

The manual check after a client build is described in
[First client start](first-start.md).
