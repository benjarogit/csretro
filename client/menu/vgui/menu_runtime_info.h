#pragma once

// Runtime provenance for menu_amd64.so — prove which build is loaded.
void CsretroMenu_LogProvenance(const char *where);
bool CsretroMenu_CaptureDebugEnabled();
void CsretroMenu_CaptureLog(const char *fmt, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 1, 2)))
#endif
	;
