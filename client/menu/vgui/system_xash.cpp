// ISystem for Xash/Linux — userconfig KeyValues file, not Windows Registry.
#include <cstring>
#include <ctime>
#include <string>
#include <sys/statvfs.h>

#include "Color.h"
#include "FileSystem.h"
#include "KeyValues.h"
#include "system_platform.h"
#include "tier0/platform.h"
#include "tier1/interface.h"
#include "vgui/ISystem.h"
#include "vgui/KeyCode.h"
#include "vgui_internal.h"
#include "vgui_key_translation.h"

using namespace vgui2;

static KeyCode MapVirtualToVgui(int keyCode)
{
	return KeyCode_VirtualKeyToVGUI(keyCode);
}
static int MapVguiToVirtual(KeyCode keyCode)
{
	return KeyCode_VGUIToVirtualKey(keyCode);
}

class CSystemXash : public ISystem
{
public:
	CSystemXash()
	{
		m_pUserConfigData = nullptr;
		m_szFileName[0] = '\0';
		m_szPathID[0] = '\0';
		m_commandLine = "";
		m_frameTime = Plat_FloatTime();
		m_clipboard.clear();
	}

	void Shutdown() override
	{
		SaveUserConfigFile();
		if (m_pUserConfigData)
		{
			m_pUserConfigData->deleteThis();
			m_pUserConfigData = nullptr;
		}
	}

	void RunFrame() override { m_frameTime = Plat_FloatTime(); }

	void ShellExecute(const char *command, const char *file) override
	{
		if (!command || !file)
			return;
		if (strcasecmp(command, "open") != 0)
			return;
		Csretro_PlatformShellOpen(file);
	}

	double GetFrameTime() override { return m_frameTime; }
	double GetCurrentTime() override { return Plat_FloatTime(); }
	long GetTimeMillis() override { return static_cast<long>(Plat_FloatTime() * 1000.0); }

	int GetClipboardTextCount() override { return static_cast<int>(m_clipboard.size()); }

	void SetClipboardText(const char *text, int textLen) override
	{
		if (!text || textLen < 0)
			return;
		m_clipboard.assign(text, text + textLen);
	}

	void SetClipboardText(const wchar_t *text, int textLen) override
	{
		if (!text || textLen < 0)
			return;
		m_clipboard.clear();
		for (int i = 0; i < textLen; ++i)
		{
			wchar_t w = text[i];
			if (w < 128)
				m_clipboard.push_back(static_cast<char>(w));
			else
				m_clipboard.push_back('?');
		}
	}

	int GetClipboardText(int offset, char *buf, int bufLen) override
	{
		if (!buf || bufLen < 1)
			return 0;
		if (offset < 0 || offset >= static_cast<int>(m_clipboard.size()))
		{
			buf[0] = '\0';
			return 0;
		}
		int n = static_cast<int>(m_clipboard.size()) - offset;
		if (n >= bufLen)
			n = bufLen - 1;
		memcpy(buf, m_clipboard.data() + offset, static_cast<size_t>(n));
		buf[n] = '\0';
		return n;
	}

	int GetClipboardText(int offset, wchar_t *buf, int bufLen) override
	{
		if (!buf || bufLen < 1)
			return 0;
		char tmp[4096];
		int n = GetClipboardText(offset, tmp, sizeof(tmp));
		int out = 0;
		for (int i = 0; i < n && out + 1 < bufLen; ++i)
			buf[out++] = static_cast<wchar_t>(static_cast<unsigned char>(tmp[i]));
		buf[out] = L'\0';
		return out;
	}

	// Registry → KeyValues tree under "Registry" in the same userconfig VDF.
	bool SetRegistryString(const char *key, const char *value) override
	{
		EnsureUserConfig();
		KeyValues *reg = m_pUserConfigData->FindKey("Registry", true);
		reg->SetString(SanitizeKey(key).c_str(), value ? value : "");
		return true;
	}

	bool GetRegistryString(const char *key, char *value, int valueLen) override
	{
		if (!value || valueLen < 1)
			return false;
		value[0] = '\0';
		EnsureUserConfig();
		KeyValues *reg = m_pUserConfigData->FindKey("Registry", false);
		if (!reg)
			return false;
		const char *v = reg->GetString(SanitizeKey(key).c_str(), nullptr);
		if (!v)
			return false;
		strncpy(value, v, static_cast<size_t>(valueLen - 1));
		value[valueLen - 1] = '\0';
		return true;
	}

	bool SetRegistryInteger(const char *key, int value) override
	{
		EnsureUserConfig();
		KeyValues *reg = m_pUserConfigData->FindKey("Registry", true);
		reg->SetInt(SanitizeKey(key).c_str(), value);
		return true;
	}

	bool GetRegistryInteger(const char *key, int &value) override
	{
		EnsureUserConfig();
		KeyValues *reg = m_pUserConfigData->FindKey("Registry", false);
		if (!reg)
			return false;
		KeyValues *k = reg->FindKey(SanitizeKey(key).c_str());
		if (!k)
			return false;
		value = k->GetInt();
		return true;
	}

	KeyValues *GetUserConfigFileData(const char *dialogName, int dialogID) override
	{
		EnsureUserConfig();
		if (!dialogName || !*dialogName)
			return nullptr;
		char buf[256];
		const char *name = dialogName;
		if (dialogID)
		{
			snprintf(buf, sizeof(buf), "%s_%d", dialogName, dialogID);
			name = buf;
		}
		return m_pUserConfigData->FindKey(name, true);
	}

	void SetUserConfigFile(const char *fileName, const char *pathName) override
	{
		EnsureUserConfig();
		strncpy(m_szFileName, fileName ? fileName : "csretro_vgui_settings.vdf", sizeof(m_szFileName) - 1);
		m_szFileName[sizeof(m_szFileName) - 1] = '\0';
		strncpy(m_szPathID, pathName ? pathName : "GAMECONFIG", sizeof(m_szPathID) - 1);
		m_szPathID[sizeof(m_szPathID) - 1] = '\0';
		if (g_pFullFileSystem)
			m_pUserConfigData->LoadFromFile(g_pFullFileSystem, m_szFileName, m_szPathID);
	}

	void SaveUserConfigFile() override
	{
		if (m_pUserConfigData && g_pFullFileSystem && m_szFileName[0])
			m_pUserConfigData->SaveToFile(g_pFullFileSystem, m_szFileName, m_szPathID);
	}

	bool SetWatchForComputerUse(bool) override { return false; }
	double GetTimeSinceLastUse() override { return 0.0; }

	int GetAvailableDrives(char *buf, int bufLen) override
	{
		if (!buf || bufLen < 2)
			return 0;
		strncpy(buf, "/", static_cast<size_t>(bufLen - 1));
		buf[bufLen - 1] = '\0';
		return 1;
	}

	bool CommandLineParamExists(const char *paramName) override
	{
		return paramName && m_commandLine.find(paramName) != std::string::npos;
	}

	const char *GetFullCommandLine() override { return m_commandLine.c_str(); }

	bool GetCurrentTimeAndDate(int *year, int *month, int *dayOfWeek, int *day, int *hour, int *minute, int *second) override
	{
		time_t now = time(nullptr);
		tm *lt = localtime(&now);
		if (!lt)
			return false;
		if (year)
			*year = lt->tm_year + 1900;
		if (month)
			*month = lt->tm_mon + 1;
		if (dayOfWeek)
			*dayOfWeek = lt->tm_wday;
		if (day)
			*day = lt->tm_mday;
		if (hour)
			*hour = lt->tm_hour;
		if (minute)
			*minute = lt->tm_min;
		if (second)
			*second = lt->tm_sec;
		return true;
	}

	double GetFreeDiskSpace(const char *path) override
	{
		struct statvfs st{};
		if (statvfs(path && *path ? path : "/", &st) != 0)
			return 0.0;
		return static_cast<double>(st.f_bavail) * static_cast<double>(st.f_frsize);
	}

	bool CreateShortcut(const char *, const char *, const char *, const char *, const char *) override { return false; }
	bool GetShortcutTarget(const char *, char *, char *, int) override { return false; }
	bool ModifyShortcutTarget(const char *, const char *, const char *, const char *) override { return false; }

	bool GetCommandLineParamValue(const char *paramName, char *value, int valueBufferSize) override
	{
		if (!paramName || !value || valueBufferSize < 1)
			return false;
		value[0] = '\0';
		size_t pos = m_commandLine.find(paramName);
		if (pos == std::string::npos)
			return false;
		pos += strlen(paramName);
		while (pos < m_commandLine.size() && (m_commandLine[pos] == ' ' || m_commandLine[pos] == '='))
			++pos;
		size_t end = pos;
		while (end < m_commandLine.size() && m_commandLine[end] != ' ')
			++end;
		size_t n = end - pos;
		if (n >= static_cast<size_t>(valueBufferSize))
			n = static_cast<size_t>(valueBufferSize - 1);
		memcpy(value, m_commandLine.data() + pos, n);
		value[n] = '\0';
		return true;
	}

	bool DeleteRegistryKey(const char *keyName) override
	{
		EnsureUserConfig();
		KeyValues *reg = m_pUserConfigData->FindKey("Registry", false);
		if (!reg)
			return false;
		KeyValues *k = reg->FindKey(SanitizeKey(keyName).c_str());
		if (!k)
			return false;
		reg->RemoveSubKey(k);
		k->deleteThis();
		return true;
	}

	const char *GetDesktopFolderPath() override { return "/tmp"; }
	const char *GetStartMenuFolderPath() override { return "/tmp"; }
	const char *GetAllUserDesktopFolderPath() override { return "/tmp"; }
	const char *GetAllUserStartMenuFolderPath() override { return "/tmp"; }

	KeyCode KeyCode_VirtualKeyToVGUI(int keyCode) override { return MapVirtualToVgui(keyCode); }
	int KeyCode_VGUIToVirtualKey(KeyCode keyCode) override { return MapVguiToVirtual(keyCode); }

	void SetCommandLine(const char *cmd) { m_commandLine = cmd ? cmd : ""; }

private:
	void EnsureUserConfig()
	{
		if (!m_pUserConfigData)
			m_pUserConfigData = new KeyValues("UserConfigData");
	}

	static std::string SanitizeKey(const char *key)
	{
		std::string s = key ? key : "";
		for (char &c : s)
		{
			if (c == '\\' || c == '/' || c == ' ')
				c = '_';
		}
		return s;
	}

	KeyValues *m_pUserConfigData;
	char m_szFileName[256];
	char m_szPathID[64];
	std::string m_commandLine;
	std::string m_clipboard;
	double m_frameTime;
};

static CSystemXash g_SystemXash;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CSystemXash, ISystem, VGUI_SYSTEM_INTERFACE_VERSION_GS, g_SystemXash);

namespace vgui2
{
ISystem *g_pSystem = &g_SystemXash;
}

void Csretro_SystemSetCommandLine(const char *cmd)
{
	g_SystemXash.SetCommandLine(cmd);
}
