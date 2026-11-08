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

## server-api

| Patch | Files | Reason | Gate |
|---|---|---|---|
| `0001-rsa-keygen-portability.patch` | `server/api/deps/rsa_keygen/main.cpp`, `server/api/deps/rsa_keygen/CMakeLists.txt` | ISO C++ requires `int main()`; `system("pause")` is a Windows shell builtin; the helper referenced undefined OpenSSL variables and linked the 32-bit archives without `-m32`. | G05 |

`rsa_keygen` is a development helper that writes an RSA key pair. It is not part
of the AMXX module and not loaded by the server.
