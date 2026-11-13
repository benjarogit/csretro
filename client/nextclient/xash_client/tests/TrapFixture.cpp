#include <unistd.h>

// A hash rejection must precede dlopen. Loading this synthetic unknown library
// terminates the test, even if a loader tries to close it immediately afterwards.
__attribute__((constructor)) static void TrapOnLoad() { _exit(91); }
