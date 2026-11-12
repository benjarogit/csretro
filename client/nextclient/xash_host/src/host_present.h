#pragma once

// Opens the X11 display, creates one window surface and makes it current.
// Returns 0 when that fails. The caller does not swap and does not move host time.
int csr_host_present_begin(int wide, int tall);

// eglSwapBuffers on the window surface from csr_host_present_begin.
// Returns 0 when the surface is missing or the swap returns false.
int csr_host_present_swap(void);

// Drops the window and the EGL display. Safe when present never started.
// The swap count stays readable through csr_host_present_query.
void csr_host_present_shutdown(void);

// Delivers queued X11 motion and button events to the host input functions.
void csr_host_present_dispatch(void);
