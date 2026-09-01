// CS Retro tier0 shims for VGUI2 (malloc allocator, spew, time, atomics).
#ifndef STATIC_TIER0
#define TIER0_DLL_EXPORT
#endif

#include "tier0_compat.h"

#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <ctime>
#include <time.h>
#include <cstdio>
#include <cstdarg>

#include "tier0/dbg.h"
#include "tier0/platform.h"
#include "tier0/threadtools.h"

#include "../src/menu_priv.h"

#if defined(NO_MALLOC_OVERRIDE)
namespace
{
class CStdMemAlloc : public IMemAlloc
{
public:
	void *Alloc(size_t nSize) override { return malloc(nSize ? nSize : 1); }
	void *Realloc(void *pMem, size_t nSize) override { return realloc(pMem, nSize ? nSize : 1); }
	void Free(void *pMem) override { free(pMem); }
	void *Expand_NoLongerSupported(void *pMem, size_t) override { return pMem; }
	void *Alloc(size_t nSize, const char *, int) override { return Alloc(nSize); }
	void *Realloc(void *pMem, size_t nSize, const char *, int) override { return Realloc(pMem, nSize); }
	void Free(void *pMem, const char *, int) override { Free(pMem); }
	void *Expand_NoLongerSupported(void *pMem, size_t, const char *, int) override { return pMem; }
	size_t GetSize(void *pMem) override
	{
#if defined(__linux__)
		return pMem ? malloc_usable_size(pMem) : 0;
#else
		(void)pMem;
		return 0;
#endif
	}
	void PushAllocDbgInfo(const char *, int) override {}
	void PopAllocDbgInfo() override {}
	long CrtSetBreakAlloc(long) override { return 0; }
	int CrtSetReportMode(int, int) override { return 0; }
	int CrtIsValidHeapPointer(const void *pMem) override { return pMem != nullptr; }
	int CrtIsValidPointer(const void *pMem, unsigned int, int) override { return pMem != nullptr; }
	int CrtCheckMemory() override { return 1; }
	int CrtSetDbgFlag(int) override { return 0; }
	void CrtMemCheckpoint(void *) override {}
	void DumpStats() override {}
	void *CrtSetReportFile(int, void *) override { return nullptr; }
	void *CrtSetReportHook(void *) override { return nullptr; }
	int CrtDbgReport(int, const char *, int, const char *, const char *) override { return 0; }
	int heapchk() override { return 0; }
	bool IsDebugHeap() override { return false; }
	void GetActualDbgInfo(const char *&pFileName, int &nLine) override
	{
		pFileName = "";
		nLine = 0;
	}
	void RegisterAllocation(const char *, int, int, int, unsigned) override {}
	void RegisterDeallocation(const char *, int, int, int, unsigned) override {}
	int GetVersion() override { return 1; }
	void CompactHeap() override {}
	size_t (*SetAllocFailHandler(size_t (*pfn)(size_t)))(size_t) override
	{
		(void)pfn;
		return nullptr;
	}
};

CStdMemAlloc g_StdMemAlloc;
} // namespace

IMemAlloc *g_pMemAlloc = &g_StdMemAlloc;
#endif

static void SpewToStderr(const char *prefix, const char *fmt, va_list ap)
{
	char buf[2048];
	vsnprintf(buf, sizeof(buf), fmt, ap);
	fputs(prefix, stderr);
	fputs(buf, stderr);
	if (gEng.Con_Printf)
		gEng.Con_Printf("%s%s", prefix, buf);
}

void Msg(const tchar *pMsg, ...)
{
	va_list ap;
	va_start(ap, pMsg);
	SpewToStderr("", pMsg, ap);
	va_end(ap);
}

void Warning(const tchar *pMsg, ...)
{
	va_list ap;
	va_start(ap, pMsg);
	SpewToStderr("Warning: ", pMsg, ap);
	va_end(ap);
}

void Error(const tchar *pMsg, ...)
{
	va_list ap;
	va_start(ap, pMsg);
	SpewToStderr("Error: ", pMsg, ap);
	va_end(ap);
	abort();
}

void _ExitOnFatalAssert(const tchar *pFile, int line)
{
	fprintf(stderr, "Fatal assert at %s:%d\n", pFile ? pFile : "?", line);
	if (gEng.Con_Printf)
		gEng.Con_Printf("Fatal assert at %s:%d\n", pFile ? pFile : "?", line);
	abort();
}

double Plat_FloatTime()
{
	timespec ts{};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
}

unsigned long Plat_MSTime()
{
	return static_cast<unsigned long>(Plat_FloatTime() * 1000.0);
}

long ThreadInterlockedIncrement(long volatile *p)
{
	return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST);
}

long ThreadInterlockedDecrement(long volatile *p)
{
	return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST);
}

long ThreadInterlockedExchange(long volatile *p, long value)
{
	return __atomic_exchange_n(p, value, __ATOMIC_SEQ_CST);
}

long ThreadInterlockedExchangeAdd(long volatile *p, long value)
{
	return __atomic_fetch_add(p, value, __ATOMIC_SEQ_CST);
}

long ThreadInterlockedCompareExchange(long volatile *p, long value, long comperand)
{
	__atomic_compare_exchange_n(p, &comperand, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return comperand;
}

void *ThreadInterlockedExchangePointer(void *volatile *p, void *value)
{
	return __atomic_exchange_n(p, value, __ATOMIC_SEQ_CST);
}

void *ThreadInterlockedCompareExchangePointer(void *volatile *p, void *value, void *comperand)
{
	__atomic_compare_exchange_n(p, &comperand, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return comperand;
}

int64 ThreadInterlockedIncrement64(int64 volatile *p)
{
	return __atomic_add_fetch(p, int64(1), __ATOMIC_SEQ_CST);
}

int64 ThreadInterlockedDecrement64(int64 volatile *p)
{
	return __atomic_sub_fetch(p, int64(1), __ATOMIC_SEQ_CST);
}

int64 ThreadInterlockedExchange64(int64 volatile *p, int64 value)
{
	return __atomic_exchange_n(p, value, __ATOMIC_SEQ_CST);
}

int64 ThreadInterlockedExchangeAdd64(int64 volatile *p, int64 value)
{
	return __atomic_fetch_add(p, value, __ATOMIC_SEQ_CST);
}

int64 ThreadInterlockedCompareExchange64(int64 volatile *p, int64 value, int64 comperand)
{
	__atomic_compare_exchange_n(p, &comperand, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return comperand;
}

bool ThreadInterlockedAssignIf64(volatile int64 *pDest, int64 value, int64 comperand)
{
	return __atomic_compare_exchange_n(pDest, &comperand, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

void AssertValidStringPtr(const char *, int) {}
void _AssertValidWritePtr(void *, int) {}
void _AssertValidReadPtr(void *, int) {}

bool ShouldUseNewAssertDialog() { return false; }
bool DoNewAssertDialog(const tchar *, int, const tchar *) { return false; }

void _SpewInfo(SpewType_t, const tchar *, int) {}
SpewRetval_t _SpewMessage(const tchar *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	return SPEW_CONTINUE;
}

extern "C" float RandomFloat(float flMinVal, float flMaxVal)
{
	if (flMaxVal <= flMinVal)
		return flMinVal;
	return flMinVal + (flMaxVal - flMinVal) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
}
