// IKeyValuesSystem for VGUI KeyValues (no key_values_export recursive wrapper).
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cwchar>

#include "tier1/interface.h"
#include "vstdlib/IKeyValuesSystem.h"

class CKeyValuesSystemImpl : public IKeyValuesSystem
{
public:
	void RegisterSizeofKeyValues(int size) override { m_sizeofKV = size; }
	void *AllocKeyValuesMemory(int size) override { return malloc(size > 0 ? static_cast<size_t>(size) : 1); }
	void FreeKeyValuesMemory(void *pMem) override { free(pMem); }

	HKeySymbol GetSymbolForString(const char *name) override
	{
		if (!name)
			name = "";
		for (size_t i = 0; i < m_symbols.size(); ++i)
		{
			if (m_symbols[i] == name)
				return static_cast<HKeySymbol>(i);
		}
		m_symbols.emplace_back(name);
		return static_cast<HKeySymbol>(m_symbols.size() - 1);
	}

	const char *GetStringForSymbol(HKeySymbol symbol) override
	{
		if (symbol < 0 || static_cast<size_t>(symbol) >= m_symbols.size())
			return "";
		return m_symbols[static_cast<size_t>(symbol)].c_str();
	}

	void AddKeyValuesToMemoryLeakList(void *, HKeySymbol) override {}
	void RemoveKeyValuesFromMemoryLeakList(void *) override {}

private:
	int m_sizeofKV = 0;
	std::vector<std::string> m_symbols;
};

static CKeyValuesSystemImpl g_KV;

// GoldSource CreateInterface "KeyValues003".
class IKeyValues003 : public IBaseInterface
{
public:
	virtual void RegisterSizeofKeyValues(int size) = 0;
	virtual void *AllocKeyValuesMemory(int size) = 0;
	virtual void FreeKeyValuesMemory(void *pMem) = 0;
	virtual HKeySymbol GetSymbolForString(const char *name) = 0;
	virtual const char *GetStringForSymbol(HKeySymbol symbol) = 0;
	virtual void GetLocalizedFromANSI(const char *ansi, wchar_t *outBuf, int unicodeBufferSizeInBytes) = 0;
	virtual void GetANSIFromLocalized(const wchar_t *wchar, char *outBuf, int ansiBufferSizeInBytes) = 0;
	virtual void AddKeyValuesToMemoryLeakList(void *pMem, HKeySymbol name) = 0;
	virtual void RemoveKeyValuesFromMemoryLeakList(void *pMem) = 0;
};

class CKeyValues003 : public IKeyValues003
{
public:
	void RegisterSizeofKeyValues(int size) override { g_KV.RegisterSizeofKeyValues(size); }
	void *AllocKeyValuesMemory(int size) override { return g_KV.AllocKeyValuesMemory(size); }
	void FreeKeyValuesMemory(void *pMem) override { g_KV.FreeKeyValuesMemory(pMem); }
	HKeySymbol GetSymbolForString(const char *name) override { return g_KV.GetSymbolForString(name); }
	const char *GetStringForSymbol(HKeySymbol symbol) override { return g_KV.GetStringForSymbol(symbol); }

	void GetLocalizedFromANSI(const char *ansi, wchar_t *outBuf, int unicodeBufferSizeInBytes) override
	{
		if (!outBuf || unicodeBufferSizeInBytes < static_cast<int>(sizeof(wchar_t)))
			return;
		outBuf[0] = L'\0';
		if (!ansi)
			return;
		const size_t n = static_cast<size_t>(unicodeBufferSizeInBytes / static_cast<int>(sizeof(wchar_t))) - 1;
		mbstowcs(outBuf, ansi, n);
		outBuf[n] = L'\0';
	}

	void GetANSIFromLocalized(const wchar_t *w, char *outBuf, int ansiBufferSizeInBytes) override
	{
		if (!outBuf || ansiBufferSizeInBytes < 1)
			return;
		outBuf[0] = '\0';
		if (!w)
			return;
		wcstombs(outBuf, w, static_cast<size_t>(ansiBufferSizeInBytes - 1));
		outBuf[ansiBufferSizeInBytes - 1] = '\0';
	}

	void AddKeyValuesToMemoryLeakList(void *p, HKeySymbol n) override { g_KV.AddKeyValuesToMemoryLeakList(p, n); }
	void RemoveKeyValuesFromMemoryLeakList(void *p) override { g_KV.RemoveKeyValuesFromMemoryLeakList(p); }
};

static CKeyValues003 g_KV003;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CKeyValues003, IKeyValues003, "KeyValues003", g_KV003);

IKeyValuesSystem *KeyValuesSystem()
{
	return &g_KV;
}

IKeyValuesSystem *keyvalues()
{
	return &g_KV;
}
