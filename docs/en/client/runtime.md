# Client runtime

CSRETRO runs only on the **Default Public branch** of Counter-Strike 1.6.
The product process engine is Xash3D. The decision is in
[plan section 3.7](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md).

## Product boundary

The product target is a native x86_64 Xash3D process. The current host exercises the menu, filesystem and surface; it does not start the game engine. `client_mini` extends an existing game client and is not a replacement for the missing native CS game implementation. Loading a factory or storing engine callbacks does not prove gameplay.

Use the [runtime acceptance procedure](../maintainers/runtime-readiness.md) for export inspection, integration and manual gameplay evidence. Current results and the next task live only in [HANDOFF](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md). The retail resolver below is an offline compatibility tool, not a substitute for the Xash integration.

## Address resolution

The product provider is `EngineAddressProviderXash`. It has no retail symbol
table. The public providers in `EngineAddressProviderPublicLinux.cpp` and
`ClientAddressProviderPublicLinux.cpp` remain the catalog of retail names for
the offline check. The product process does not use them. Every retail entry
is resolved in three stages:

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

The two generated names are bound to this exact binary. The retail catalog is
pinned by SHA-256. The tool reports every mismatch. The product process does
not load this catalog.

`EngineMsgBase` is the `cl_parsefuncs` table; its layout
`{opcode, pszname, pfnParse}` matches `goldsrc::EngineMsg`.

## Adding a build

1. Take both files from an unmodified installation.
2. Run `sha256sum hw.so cstrike/cl_dlls/client.so`.
3. Add an entry with ID, Steam build number, platform and both digests to
   `build_registry.cpp`.
4. Run `nitro_api-tests`; the registry tests cover unknown, incomplete and
   platform-mismatched fingerprints.

Another retail catalog needs its own address provider whose symbol names are
checked against the new binaries. The resolver itself stays unchanged. The
product path remains `EngineAddressProviderXash`.

## First start

The manual check after a client build is described in
[First client start](first-start.md).
