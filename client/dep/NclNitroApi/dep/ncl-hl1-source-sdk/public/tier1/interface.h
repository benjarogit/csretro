#pragma once
// CS Retro menu/V1: use GoldSrc body CreateInterface (same TU as GameMenuExports).
// Original SDK copy: interface.h.sdkorig — not Steam-ABI compatible goal.
#include "csretro_interface_body.h"
#ifndef CSRETRO_HAS_INITIALIZE_INTERFACE
#define CSRETRO_HAS_INITIALIZE_INTERFACE
#ifdef __cplusplus
extern void *InitializeInterface(char const *interfaceName, CreateInterfaceFn *factoryList, int numFactories);
#endif
#endif
