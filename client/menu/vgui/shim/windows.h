#pragma once
// Empty shim — do not pull Win32 into product code.
typedef void *HWND;
inline HWND GetActiveWindow() { return nullptr; }
inline void OutputDebugString(const char *) {}
inline void OutputDebugStringA(const char *) {}
