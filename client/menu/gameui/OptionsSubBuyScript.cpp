#include "OptionsSubBuyScript.h"

#include <FileSystem.h>
#include <tier1/KeyValues.h>
#include <tier1/utlbuffer.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>

#include "../src/menu_priv.h"

using namespace vgui2;

extern IFileSystem *g_pFullFileSystem;

COptionsSubBuyScript::COptionsSubBuyScript(Panel *parent) : PropertyPage(parent, "OptionsSubBuyScript")
{
	m_pScriptType = new ComboBox(this, "ScriptType", 2, false);
	m_pScriptType->AddItem("#CsretroGameUI_AutoBuy", new KeyValues("script", "id", 0));
	m_pScriptType->AddItem("#CsretroGameUI_ReBuy", new KeyValues("script", "id", 1));
	m_pScriptType->ActivateItem(0);
	m_pScriptType->AddActionSignalTarget(this);

	m_pEditor = new TextEntry(this, "ScriptEditor");
	m_pEditor->SetMultiline(true);
	m_pEditor->SetVerticalScrollbar(true);
	m_pEditor->SetCatchEnterKey(true);
	m_pEditor->SetWrap(false);
	m_pEditor->SetMaximumCharCount(16384);
	m_pEditor->AddActionSignalTarget(this);

	LoadControlSettings("resource/OptionsSubBuyScript.res");
	LoadScripts();
	ShowCurrentScript();
}

COptionsSubBuyScript::~COptionsSubBuyScript() = default;

void COptionsSubBuyScript::OnResetData()
{
	LoadScripts();
	ShowCurrentScript();
}

void COptionsSubBuyScript::OnApplyChanges()
{
	StashCurrentScript();
	for (int scriptIndex = 0; scriptIndex < 2; ++scriptIndex)
	{
		if (!SaveScript(scriptIndex))
			Menu_Con("CSRETRO_BUYSCRIPT write_failed %s", scriptIndex != 0 ? "rebuy.txt" : "autobuy.txt");
	}
}

void COptionsSubBuyScript::PerformLayout()
{
	BaseClass::PerformLayout();
	int wide = 0;
	int tall = 0;
	GetSize(wide, tall);
	m_pScriptType->SetBounds(36, 32, 220, 24);
	m_pEditor->SetBounds(36, 72, std::max(160, wide - 72), std::max(120, tall - 104));
}

const char *COptionsSubBuyScript::CurrentScriptPath() const
{
	return CurrentScriptIndex() != 0 ? "rebuy.txt" : "autobuy.txt";
}

int COptionsSubBuyScript::CurrentScriptIndex() const
{
	KeyValues *item = m_pScriptType->GetActiveItemUserData();
	return item && item->GetInt("id", 0) != 0 ? 1 : 0;
}

void COptionsSubBuyScript::LoadScripts()

{
	for (int scriptIndex = 0; scriptIndex < 2; ++scriptIndex)
	{
		m_stagedText[scriptIndex].clear();
		if (!g_pFullFileSystem)
			continue;
		const char *path = scriptIndex != 0 ? "rebuy.txt" : "autobuy.txt";
		FileHandle_t file = g_pFullFileSystem->Open(path, "rb", "GAME");
		if (file == FILESYSTEM_INVALID_HANDLE)
			continue;
		const int size = g_pFullFileSystem->Size(file);
		CUtlBuffer buffer(0, size + 1, CUtlBuffer::TEXT_BUFFER);
		g_pFullFileSystem->Read(buffer.Base(), size, file);
		g_pFullFileSystem->Close(file);
		static_cast<char *>(buffer.Base())[size] = '\0';
		m_stagedText[scriptIndex].assign(static_cast<char *>(buffer.Base()), size);
	}
}

void COptionsSubBuyScript::ShowCurrentScript()

{
	m_visibleScript = CurrentScriptIndex();
	m_pEditor->SetText(m_stagedText[m_visibleScript].c_str());
}

void COptionsSubBuyScript::StashCurrentScript()

{
	const int length = m_pEditor->GetTextLength();
	std::string text(static_cast<size_t>(length) + 1, '\0');
	m_pEditor->GetText(text.data(), static_cast<int>(text.size()));
	text.resize(static_cast<size_t>(length));
	m_stagedText[m_visibleScript] = text;
}

bool COptionsSubBuyScript::SaveScript(int scriptIndex)

{
	if (!g_pFullFileSystem)
		return false;
	const char *path = scriptIndex != 0 ? "rebuy.txt" : "autobuy.txt";
	FileHandle_t file = g_pFullFileSystem->Open(path, "wb", "GAME");
	if (file == FILESYSTEM_INVALID_HANDLE)
		return false;
	const std::string &text = m_stagedText[scriptIndex];
	g_pFullFileSystem->Write(text.data(), static_cast<int>(text.size()), file);
	g_pFullFileSystem->Close(file);
	return true;
}

void COptionsSubBuyScript::OnTextChanged(KeyValues *data)

{
	Panel *panel = static_cast<Panel *>(data->GetPtr("panel"));
	if (panel == m_pScriptType)
		{
			StashCurrentScript();
			ShowCurrentScript();
		}
	else if (panel == m_pEditor)
		PostActionSignal(new KeyValues("ApplyButtonEnable"));
}