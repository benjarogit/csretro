# Patches

Deviations from vendored upstream code, one directory per upstream. The patches
are **already applied** in the vendored tree; this directory exists so the change
is reviewable and can be re-applied after an upstream refresh.

Apply from the repository root:

```bash
git apply patches/server-api/0001-rsa-keygen-portability.patch
```

If an upstream update makes a patch unnecessary, delete the file and the
corresponding change together.

## sdk

The HL1 SDK source is the tracked tree `client/dep/ncl-hl1-source-sdk/`.
Our edits are those files. Configure does not apply a patch. The upstream
base is commit `0e9be00fc52b66742a09c84c8f6169dc7c83ca8a` in
`versions.lock.json`.

These files are the readable delta from that base. They are already present
in the tree. Apply one from the repository root only when refreshing that
base and the tree does not already contain the change:

```bash
git apply --directory=client/dep/ncl-hl1-source-sdk patches/sdk/0001-export-vgui-dispatch.patch
```

| Patch | Files | Reason | Gate |
|---|---|---|---|
| `0001-export-vgui-dispatch.patch` | `client/dep/ncl-hl1-source-sdk/vgui2/src/vgui.cpp` | Retail `vgui2` keeps `CVGui::DispatchMessages` local. The product library exports `csr_vgui_dispatch_messages` so the host can pump messages without `CVGui::RunFrame`. | G07 |
| `0002-lp64-tier1.patch` | tier0, tier1, vgui controls | The product process is x86_64. On-disk integers stay their existing widths. Pointer-sized fields inside this process follow LP64. | G07 |
| `0003-lp64-host-libs.patch` | `CMakeLists.txt` | The parent build supplies `tier0`, `vstdlib` and `steam_api`. The prebuilt i386 libraries in `lib/public` are not loaded. | G07 |
| `0004-lp64-focus-message.patch` | `MessageMap.h`, `Panel.h`, `Panel.cpp` | `OnRequestFocus` carries two panel pointers. The int KeyValues constructor truncates them on LP64, and the parent then crashes in `IsPopup`. The message stores both pointers and reads them back at full width. | G07 |

## server-api

| Patch | Files | Reason | Gate |
|---|---|---|---|
| `0001-rsa-keygen-portability.patch` | `server/api/deps/rsa_keygen/main.cpp`, `server/api/deps/rsa_keygen/CMakeLists.txt` | ISO C++ requires `int main()`; `system("pause")` is a Windows shell builtin; the helper referenced undefined OpenSSL variables and linked the 32-bit archives without `-m32`. | G05 |

`rsa_keygen` is a development helper that writes an RSA key pair. It is not part
of the AMXX module and not loaded by the server.
