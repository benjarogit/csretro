// Minimal ICommandLine for VGUI Panel/debug parms (no precompiled.h).
#include "tier0/icommandline.h"

#include <cstring>
#include <string>
#include <vector>

class CCommandLineXash : public ICommandLine
{
public:
	void CreateCmdLine(const char *commandline) override
	{
		m_line = commandline ? commandline : "";
		RebuildParms();
	}
	void CreateCmdLine(int argc, char **argv) override
	{
		m_line.clear();
		for (int i = 0; i < argc; ++i)
		{
			if (i)
				m_line.push_back(' ');
			if (argv[i])
				m_line += argv[i];
		}
		RebuildParms();
	}
	const char *GetCmdLine() const override { return m_line.c_str(); }

	const char *CheckParm(const char *psz, const char **ppszValue) const override
	{
		const int i = FindParm(psz);
		if (!i)
			return nullptr;
		if (ppszValue)
			*ppszValue = (i + 1 < static_cast<int>(m_parms.size())) ? m_parms[static_cast<size_t>(i + 1)].c_str() : nullptr;
		return m_parms[static_cast<size_t>(i)].c_str();
	}
	void RemoveParm(const char *) override {}
	void AppendParm(const char *pszParm, const char *pszValues) override
	{
		if (!pszParm)
			return;
		if (!m_line.empty())
			m_line.push_back(' ');
		m_line += pszParm;
		if (pszValues && *pszValues)
		{
			m_line.push_back(' ');
			m_line += pszValues;
		}
		RebuildParms();
	}

	const char *ParmValue(const char *psz, const char *pDefaultVal) const override
	{
		const char *v = nullptr;
		if (!CheckParm(psz, &v) || !v)
			return pDefaultVal;
		return v;
	}
	int ParmValue(const char *psz, int nDefaultVal) const override
	{
		const char *v = ParmValue(psz, static_cast<const char *>(nullptr));
		return v ? atoi(v) : nDefaultVal;
	}
	float ParmValue(const char *psz, float flDefaultVal) const override
	{
		const char *v = ParmValue(psz, static_cast<const char *>(nullptr));
		return v ? static_cast<float>(atof(v)) : flDefaultVal;
	}

	int ParmCount() const override { return static_cast<int>(m_parms.size()); }
	int FindParm(const char *psz) const override
	{
		if (!psz)
			return 0;
		for (size_t i = 1; i < m_parms.size(); ++i)
		{
			if (m_parms[i] == psz)
				return static_cast<int>(i);
		}
		return 0;
	}
	const char *GetParm(int nIndex) const override
	{
		if (nIndex < 0 || nIndex >= static_cast<int>(m_parms.size()))
			return "";
		return m_parms[static_cast<size_t>(nIndex)].c_str();
	}

private:
	void RebuildParms()
	{
		m_parms.clear();
		m_parms.emplace_back("csretro");
		const char *p = m_line.c_str();
		while (*p)
		{
			while (*p == ' ' || *p == '\t')
				++p;
			if (!*p)
				break;
			const char *start = p;
			while (*p && *p != ' ' && *p != '\t')
				++p;
			m_parms.emplace_back(start, p);
		}
	}

	std::string m_line;
	std::vector<std::string> m_parms;
};

static CCommandLineXash g_CmdLine;

ICommandLine *CommandLine()
{
	return &g_CmdLine;
}
