# xash_host

The host owns the `VGUI_Surface026` factory. `vgui2.so` does not link this
library. Xash3D is not patched. `LD_PRELOAD` stays a spike tool.

- `csr_host_open` loads `xash_surface.so` with `RTLD_LOCAL` and creates the
  one surface. The first factory entry is `csr_surface_create_interface`.
- Extra factories follow that entry. The surface name is not forwarded to them.
- `csr_host_bind_input` forwards cursor, press and release, then one dispatch.
  `CVGui::RunFrame` is not called. `SurfaceSetCursorPos` does not dispatch.
- Our `vgui2` exports `csr_vgui_dispatch_messages` through
  `patches/sdk/0001-export-vgui-dispatch.patch`. The SDK submodule pin does
  not move. Retail `vgui2` keeps `DispatchMessages` local, so the host takes
  a function pointer instead of a hardcoded address.
