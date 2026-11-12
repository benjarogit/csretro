//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Basic header for using vgui
//
// $NoKeywords: $
//=============================================================================//

#ifndef VGUI2_H
#define VGUI2_H

#ifdef _WIN32
#pragma once
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#ifdef WIN32
#pragma warning( disable: 4355 )	// disables 'this' : used in base member initializer list
#endif

typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;
typedef unsigned long  ulong;

#ifndef _WCHAR_T_DEFINED
// DAL - wchar_t is a built in define in gcc 3.2 with a size of 4 bytes
#if !defined( __x86_64__ ) && !defined( __WCHAR_TYPE__  )
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif
#endif

// do this in GOLDSRC only!!!
//#define Assert assert

namespace vgui2
{
// Handle to an internal vgui panel. It is a pointer in this process. Retail
// i386 vgui2 is not loaded here, so the handle is not a 32-bit cross-dll slot.
typedef unsigned long VPANEL;

// handles to vgui objects
// NULL_HANDLE values signify an invalid value
typedef unsigned long HScheme;
typedef unsigned long HTexture;
typedef unsigned long HCursor;
typedef unsigned long HPanel;
const HPanel INVALID_PANEL = 0xffffffff;
typedef unsigned long HFont;
const HFont INVALID_FONT = 0; // the value of an invalid font handle

/**
*	Represents an invalid handle. Code originally used NULL, which isn't intended to be used with non-pointer types. - Solokiller
*/
#define NULL_HANDLE 0
}

#include "vstdlib/strtools.h"

#ifdef SOURCE_SDK_VGUI_NS_ALIAS
namespace vgui = vgui2;
#endif


#endif // VGUI2_H
