#pragma once

#include <cstddef>

// Optional include for callers that want g_pMemAlloc under NO_MALLOC_OVERRIDE.
// Prefer compiling with -DSTATIC_TIER0 so Msg/Plat_* link as ordinary symbols.

#if defined(NO_MALLOC_OVERRIDE)
class IMemAlloc
{
public:
	virtual ~IMemAlloc() = default;
	virtual void *Alloc(size_t nSize) = 0;
	virtual void *Realloc(void *pMem, size_t nSize) = 0;
	virtual void Free(void *pMem) = 0;
	virtual void *Expand_NoLongerSupported(void *pMem, size_t nSize) = 0;
	virtual void *Alloc(size_t nSize, const char *pFileName, int nLine) = 0;
	virtual void *Realloc(void *pMem, size_t nSize, const char *pFileName, int nLine) = 0;
	virtual void Free(void *pMem, const char *pFileName, int nLine) = 0;
	virtual void *Expand_NoLongerSupported(void *pMem, size_t nSize, const char *pFileName, int nLine) = 0;
	virtual size_t GetSize(void *pMem) = 0;
	virtual void PushAllocDbgInfo(const char *pFileName, int nLine) = 0;
	virtual void PopAllocDbgInfo() = 0;
	virtual long CrtSetBreakAlloc(long lNewBreakAlloc) = 0;
	virtual int CrtSetReportMode(int nReportType, int nReportMode) = 0;
	virtual int CrtIsValidHeapPointer(const void *pMem) = 0;
	virtual int CrtIsValidPointer(const void *pMem, unsigned int size, int access) = 0;
	virtual int CrtCheckMemory() = 0;
	virtual int CrtSetDbgFlag(int nNewFlag) = 0;
	virtual void CrtMemCheckpoint(void *pState) = 0;
	virtual void DumpStats() = 0;
	virtual void *CrtSetReportFile(int nRptType, void *hFile) = 0;
	virtual void *CrtSetReportHook(void *pfnNewHook) = 0;
	virtual int CrtDbgReport(int nRptType, const char *szFile, int nLine, const char *szModule, const char *pMsg) = 0;
	virtual int heapchk() = 0;
	virtual bool IsDebugHeap() = 0;
	virtual void GetActualDbgInfo(const char *&pFileName, int &nLine) = 0;
	virtual void RegisterAllocation(const char *pFileName, int nLine, int nLogicalSize, int nActualSize, unsigned nTime) = 0;
	virtual void RegisterDeallocation(const char *pFileName, int nLine, int nLogicalSize, int nActualSize, unsigned nTime) = 0;
	virtual int GetVersion() = 0;
	virtual void CompactHeap() = 0;
	virtual size_t (*SetAllocFailHandler(size_t (*pfn)(size_t)))(size_t) = 0;
};

extern IMemAlloc *g_pMemAlloc;
#endif
