#include "vgui_boot.h"
#include "window_geometry.h"
#include "menu_runtime_info.h"
#include "../gameui/CreateGameDialog.h"
#include "../gameui/ServerBrowserDialog.h"
#include "../gameui/GameConsoleDialog.h"
#include "../gameui/TeamSelectPanel.h"
#include "../gameui/ClassSelectPanel.h"
#include "../gameui/BuySelectPanel.h"
#include "../gameui/Controls/MenuEngine.h"
#include "../gameui/RadioSelectPanel.h"
#include "../gameui/SpectatorHudPanel.h"
#include "../gameui/ScoreboardHudPanel.h"
#include "../gameui/OptionsDialog.h"
#include "../gameui/OptionsClassicMetrics.h"
#include "../gameui/OptionsMouseGate.h"
#include "../gameui/OptionsAudioGate.h"
#include "../gameui/OptionsKeyboardGate.h"
#include "../gameui/OptionsVideoGate.h"
#include "../gameui/OptionsVideoModeSafetyGate.h"
#include "../gameui/OptionsLayoutGate.h"
#include "../gameui/Controls/MenuEngine.h"

#include <cstdlib>
#include <cstring>
#include <cmath>

#include "FileSystem.h"
#include "KeyValues.h"
#include "keydefs.h"
#include "tier0/dbg.h"
#include "tier1/interface.h"
#include "vgui/IInputInternal.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "vgui/ISchemeNext.h"
#include "vgui/ISurfaceNext.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui/KeyCode.h"
#include "vgui/MouseCode.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_internal.h"
#include "vgui_key_translation.h"
#include "vstdlib/IKeyValuesSystem.h"

#include "surface_xash.h"
#include "main_menu.h"
#include "../src/menu_priv.h"

void Csretro_SystemSetCommandLine(const char *cmd);

// tier2 accessors (do not also link tier2.cpp — KeyValues uses our keyvalues()).
IFileSystem *g_pFullFileSystem = nullptr;
IBaseUI *g_pBaseUI = nullptr;
IEngineVGui *g_pEngineVGui = nullptr;
IGameUIFuncs *g_pGameUIFuncs = nullptr;
vgui2::ISurfaceNext *g_pVGuiSurface = nullptr;
vgui2::IInputInternal *g_pVGuiInput = nullptr;
vgui2::IVGui *g_pVGui = nullptr;
vgui2::IPanel *g_pVGuiPanel = nullptr;
vgui2::ILocalize *g_pVGuiLocalize = nullptr;
vgui2::ISchemeManagerNext *g_pVGuiSchemeManager = nullptr;
vgui2::ISystem *g_pVGuiSystem = nullptr;

namespace
{
bool g_inited = false;
vgui2::Panel *g_root = nullptr;
COptionsDialog *g_options = nullptr;
CCreateGameDialog *g_createGame = nullptr;
int g_optionsGateFrame = -1;
}

bool PocDialog_Show(vgui2::Panel *parent);
void PocDialog_Hide();
bool PocDialog_IsActive();

// tier2 globals (normally ConnectTier2Libraries) — defined in tier2.cpp if linked,
// otherwise we need definitions. Provide weak-safe assignment targets by defining
// here when CSRETRO_VGUI_OWN_TIER2_GLOBALS is set; otherwise use linked tier2.cpp.
#ifndef CSRETRO_VGUI_SKIP_TIER2_DEFS
// If tier2.cpp is linked, these are already defined — do not redefine.
// Boot only assigns. Definitions stay in tier2/tier2.cpp.
#endif

namespace vgui2
{
IPanel *g_pIPanel = nullptr;
IFileSystem *g_pFullFileSystem = nullptr;
ILocalize *g_pVGuiLocalize = nullptr;
void *g_MainWindow = nullptr;

HScheme VGui_GetDefaultScheme()
{
	return 0;
}

bool VGui_InternalLoadInterfaces(CreateInterfaceFn *factoryList, int numFactories)
{
	if (!g_pIPanel)
		g_pIPanel = static_cast<IPanel *>(Sys_GetFactoryThis()(VGUI_PANEL_INTERFACE_VERSION_GS, nullptr));

	if (!g_pSurface)
		g_pSurface = static_cast<ISurface *>(InitializeInterface(VGUI_SURFACE_INTERFACE_VERSION_GS, factoryList, numFactories));
	if (!g_pSurfaceNext)
		g_pSurfaceNext = static_cast<ISurfaceNext *>(InitializeInterface(VGUI_SURFACE_NEXT_INTERFACE_VERSION, factoryList, numFactories));
	if (!g_pFullFileSystem)
		g_pFullFileSystem = static_cast<IFileSystem *>(InitializeInterface(FILESYSTEM_INTERFACE_VERSION, factoryList, numFactories));
	if (!g_pVGuiLocalize)
		g_pVGuiLocalize = static_cast<ILocalize *>(InitializeInterface(VGUI_LOCALIZE_INTERFACE_VERSION, factoryList, numFactories));

	return g_pIPanel && g_pSurface && g_pFullFileSystem && g_pVGuiLocalize;
}
} // namespace vgui2

static void WireFactories(CreateInterfaceFn factory)
{
	::g_pFullFileSystem = static_cast<IFileSystem *>(factory(FILESYSTEM_INTERFACE_VERSION, nullptr));
	g_pVGuiSurface = static_cast<vgui2::ISurfaceNext *>(factory(VGUI_SURFACE_NEXT_INTERFACE_VERSION, nullptr));
	g_pVGuiInput = static_cast<vgui2::IInputInternal *>(factory(VGUI_INPUTINTERNAL_INTERFACE_VERSION, nullptr));
	g_pVGui = static_cast<vgui2::IVGui *>(factory(VGUI_IVGUI_INTERFACE_VERSION_GS, nullptr));
	g_pVGuiPanel = static_cast<vgui2::IPanel *>(factory(VGUI_PANEL_INTERFACE_VERSION_GS, nullptr));
	g_pVGuiLocalize = static_cast<vgui2::ILocalize *>(factory(VGUI_LOCALIZE_INTERFACE_VERSION, nullptr));
	g_pVGuiSchemeManager = static_cast<vgui2::ISchemeManagerNext *>(factory(VGUI_SCHEME_NEXT_INTERFACE_VERSION, nullptr));
	g_pVGuiSystem = static_cast<vgui2::ISystem *>(factory(VGUI_SYSTEM_INTERFACE_VERSION_GS, nullptr));

	vgui2::g_pFullFileSystem = ::g_pFullFileSystem;
	vgui2::g_pSurfaceNext = g_pVGuiSurface;
	vgui2::g_pSurface = g_pVGuiSurface;
	vgui2::g_pSystem = g_pVGuiSystem;
	vgui2::g_pInput = g_pVGuiInput;
	vgui2::g_pIVgui = g_pVGui;
	vgui2::g_pIPanel = g_pVGuiPanel;
	vgui2::g_pVGuiLocalize = g_pVGuiLocalize;
	vgui2::g_pScheme = g_pVGuiSchemeManager;

	keyvalues()->RegisterSizeofKeyValues(sizeof(KeyValues));
}

static void AddDefaultSearchPaths()
{
	IFileSystem *fs = ::g_pFullFileSystem;
	if (!fs)
		return;

	const char *rodir = getenv("XASH3D_RODIR");
	const char *basedir = getenv("XASH3D_BASEDIR");
	char buf[1024];

	// Override zuerst: sonst gewinnt die ältere Kopie in RODIR/cstrike.
	const char *overrideEnv = getenv("CSRETRO_UI_OVERRIDE");
	if (overrideEnv && *overrideEnv)
		fs->AddSearchPath(overrideEnv, "GAME");
	if (basedir && *basedir)
	{
		// Staged runtime overlays must precede their read-only GameData copies.
		snprintf(buf, sizeof(buf), "%s/cstrike", basedir);
		fs->AddSearchPath(buf, "GAME");
		fs->AddSearchPath(buf, "GAMECONFIG");
		fs->AddSearchPath(basedir, "GAMECONFIG");
		fs->AddSearchPath(basedir, "DEFAULTGAME");
	}

	if (rodir && *rodir)
	{
		snprintf(buf, sizeof(buf), "%s/cstrike", rodir);
		fs->AddSearchPath(buf, "GAME");
		snprintf(buf, sizeof(buf), "%s/valve", rodir);
		fs->AddSearchPathNoWrite(buf, "GAME");
		snprintf(buf, sizeof(buf), "%s/platform", rodir);
		fs->AddSearchPathNoWrite(buf, "PLATFORM");
	}
}

void VGuiXash_Init()
{
	if (g_inited)
		return;

	CreateInterfaceFn thisFactory = Sys_GetFactoryThis();
	CreateInterfaceFn factories[1] = {thisFactory};

	WireFactories(thisFactory);
	AddDefaultSearchPaths();
	Csretro_SystemSetCommandLine(getenv("CSRETRO_CMDLINE"));

	if (g_pVGuiSystem)
		g_pVGuiSystem->SetUserConfigFile("csretro_vgui_settings.vdf", "GAMECONFIG");

	if (!vgui2::VGui_InitInterfacesList("csretro_menu", factories, 1))
	{
		Warning("VGui_InitInterfacesList failed\n");
		return;
	}

	if (g_pVGui)
		g_pVGui->Init(factories, 1);

	if (g_pVGuiSchemeManager)
	{
		// Steam-CS-1.6-GameUI: TrackerScheme (olive VGUI2). ClientScheme ist HUD-/Ingame-
		// Overlay mit ControlBG alpha 0 — ungeeignet als Options-Default.
		const vgui2::HScheme tracker =
			g_pVGuiSchemeManager->LoadSchemeFromFile("resource/TrackerScheme.res", "TrackerScheme");
		Menu_Con("CSRETRO_SCHEME_TrackerScheme %s", tracker ? "OK" : "FAIL");
		const vgui2::HScheme client =
			g_pVGuiSchemeManager->LoadSchemeFromFile("resource/ClientScheme.res", "ClientScheme");
		Menu_Con("CSRETRO_SCHEME_ClientScheme %s", client ? "OK" : "FAIL");
		(void)client;
	}

	if (g_pVGuiLocalize && ::g_pFullFileSystem)
	{
		// Search-Roots: cstrike/, valve/, platform/ — relative Pfade ohne doppeltes „platform/“.
		struct LocFile
		{
			const char *path;
			const char *tag;
		};
		const LocFile files[] = {
			{"resource/gameui_%language%.txt", "gameui"},
			// Keyboard-Actions (#Valve_Move_*) — wie NextClient GameUi.cpp.
			{"resource/valve_%language%.txt", "valve"},
			{"resource/vgui_%language%.txt", "vgui"},
			{"resource/cstrike_%language%.txt", "cstrike"},
			{"resource/platform_%language%.txt", "platform"},
		};
		for (const LocFile &lf : files)
		{
			const bool ok = g_pVGuiLocalize->AddFile(::g_pFullFileSystem, lf.path);
			Menu_Con("CSRETRO_LOC_%s %s", lf.tag, ok ? "OK" : "FAIL");
		}
		const char *basedir = getenv("XASH3D_BASEDIR");
		if (basedir && *basedir)
		{
			char customLocalization[1024];
			snprintf(customLocalization, sizeof(customLocalization), "%s/cstrike/resource/csretro_gameui_%%language%%.txt", basedir);
			const bool ok = g_pVGuiLocalize->AddFile(::g_pFullFileSystem, customLocalization);
			Menu_Con("CSRETRO_LOC_csretro_gameui %s", ok ? "OK" : "FAIL");
		}
		// Probe: fehlender String darf nicht still als #Token durchgehen.
		const char *probes[] = {
			"GameUI_Options",
			"GameUI_Keyboard",
			"GameUI_Mouse",
			"GameUI_Audio",
			"GameUI_ReverseMouse",
			"GameUI_MouseFilter",
			"GameUI_SoundEffectVolume",
			"GameUI_MP3Volume",
			"GameUI_SoundQuality",
			"GameUI_High",
			"GameUI_Low",
			"GameUI_KeyButton",
			"GameUI_Alternate",
			"Valve_Move_Forward",
			"Valve_Movement_Title",
			"PropertyDialog_OK",
			"PropertyDialog_Cancel",
			"PropertyDialog_Apply",
			"GameUI_GameMenu_FindServers",
			"GameUI_Close",
			"CsretroServerBrowser_NoServers",
			"CsretroServerBrowser_Scanning",
			"CsretroServerBrowser_Found",
			"CsretroServerBrowser_Locked",
			"CsretroServerBrowser_Password",
			"CsretroServerBrowser_Servers",
			"CsretroServerBrowser_IPAddress",
			"CsretroServerBrowser_Players",
			"CsretroServerBrowser_Map",
			"CsretroServerBrowser_Latency",
			"CsretroServerBrowser_Connect",
			"CsretroServerBrowser_Refresh",
		};
		for (const char *tok : probes)
		{
			wchar_t *w = g_pVGuiLocalize->Find(tok);
			if (!w || !w[0])
				Menu_Con("CSRETRO_LOC_MISSING %s", tok);
			else
				Menu_Con("CSRETRO_LOC_HIT %s", tok);
		}
	}

	g_root = new vgui2::Panel(nullptr, "CsretroVguiRoot");
	g_root->SetBounds(0, 0, gGlobals ? gGlobals->scrWidth : 640, gGlobals ? gGlobals->scrHeight : 480);
	g_root->SetPaintBackgroundEnabled(false);
	g_root->SetVisible(true);
	if (g_pVGuiSurface)
		g_pVGuiSurface->SetEmbeddedPanel(g_root->GetVPanel());
	GameConsole_Initialize(g_root);
	SpectatorHud_BindHost(g_root);
	ScoreboardHud_BindHost(g_root);

	g_inited = true;
	CsretroMenu_LogProvenance("VGuiXash_Init");
	Menu_Con("VGUI Xash runtime initialized");
	if (getenv("CSRETRO_V1POC"))
	{
		Menu_Con("CSRETRO_V1POC_READY");
		// Auto-Show sofort nach Init (nicht erst UI_SetActiveMenu) für Runtime-Skript.
		if (PocDialog_Show(g_root))
		{
			gMenuVisible = true;
			if (gEng.pfnSetKeyDest)
				gEng.pfnSetKeyDest(2); // key_menu
		}
	}
	else if (getenv("CSRETRO_OPTIONS_AUTO") || getenv("CSRETRO_OPTIONS_GATE") ||
		 getenv("CSRETRO_OPTIONS_AUDIO_GATE") || getenv("CSRETRO_OPTIONS_KEYBOARD_GATE") ||
		 getenv("CSRETRO_OPTIONS_KEYBOARD_CAPTURE_PHYS") ||
		 getenv("CSRETRO_OPTIONS_VIDEO_GATE") || getenv("CSRETRO_OPTIONS_VIDEO_MODE_SAFETY") ||
		 getenv("CSRETRO_OPTIONS_LAYOUT_GATE"))
	{
		// Smoke / Gate: echte Options-Subpages ohne PoC.
		if (VGuiXash_ShowOptionsDialog())
		{
			gMenuVisible = true;
			if (gEng.pfnSetKeyDest)
				gEng.pfnSetKeyDest(2);
		}
		else
			Menu_Con("CSRETRO_OPTIONS_INTERIM");
	}
}

void VGuiXash_Shutdown()
{
	if (!g_inited)
		return;
	PocDialog_Hide();
	ClassSelect_Shutdown();
	BuySelect_Shutdown();
	RadioSelect_Shutdown();
	SpectatorHud_Shutdown();
	ScoreboardHud_Shutdown();
	TeamSelect_Shutdown();
	GameConsole_Shutdown();
	ServerBrowser_Shutdown();
	MainMenu_Shutdown();
	if (g_root)
	{
		g_root->DeletePanel();
		g_root = nullptr;
	}
	g_options = nullptr;
	g_createGame = nullptr;
	if (g_pVGui)
		g_pVGui->Shutdown();
	g_inited = false;
}

void VGuiXash_RunFrame()
{
	if (!g_inited || !g_pVGui)
		return;
	if (g_root && gGlobals)
	{
		int prevW = 0, prevH = 0;
		g_root->GetSize(prevW, prevH);
		g_root->SetBounds(0, 0, gGlobals->scrWidth, gGlobals->scrHeight);
		if (prevW != gGlobals->scrWidth || prevH != gGlobals->scrHeight)
		{
			Menu_Con("CSRETRO_VGUI_ROOT_RESIZE before=%dx%d after=%dx%d children=%d",
				prevW, prevH, gGlobals->scrWidth, gGlobals->scrHeight,
				g_root->GetChildCount());
			MainMenu_InvalidateLayout();
			// Full-screen in-game surfaces must follow the new root immediately.
			// Merely invalidating them left their old top-left viewport alive until
			// the next explicit Show() on some VGUI2 implementations.
			for (int i = 0; i < g_root->GetChildCount(); ++i)
			{
				vgui2::Panel *child = g_root->GetChild(i);
				const char *name = child ? child->GetName() : nullptr;
				const bool fullScreen = name &&
					(!strcmp(name, "TeamSelectOverlay") || !strcmp(name, "ClassSelectOverlay") ||
					 !strcmp(name, "BuySelectOverlay") || !strcmp(name, "RadioSelectOverlay") ||
					 !strcmp(name, "ScoreboardHud") || !strcmp(name, "SpectatorHud"));
				if (fullScreen)
					child->SetBounds(0, 0, gGlobals->scrWidth, gGlobals->scrHeight);
				if (child)
					child->InvalidateLayout(true);
			}
		}
	}
	g_pVGui->RunFrame();
	MenuEngine::PollPendingGameKeyDest();
	MainMenu_SyncDialogVisibility();
	if (getenv("CSRETRO_OPTIONS_VIDEO_GATE") && g_options)
		OptionsVideo_RunInputGate(g_options);
	BuySelect_AfterFrame();
	MainMenu_GateTick();
	MainMenu_PauseGateTick();
	CreateGame_GateTick();
	TeamSelect_GateTick();
	ClassSelect_GateTick();
	BuySelect_GateTick();
	RadioSelect_GateTick();
	SpectatorHud_GateTick();
	ScoreboardHud_GateTick();
	ServerBrowser_RunFrame();

	if (VGuiXash_IsTeamSelectActive() || VGuiXash_IsClassSelectActive() || VGuiXash_IsBuySelectActive())
	{
		SpectatorHud_Hide();
		if (gEng.pfnSetKeyDest)
			gEng.pfnSetKeyDest(2);
	}
	if (VGuiXash_IsConsoleActive())
		GameConsole_FocusEntry();

	// Workspace change: clamp saved/current bounds. Do not stomp back to 512×406.
	if (g_options && g_options->IsVisible() && g_pVGuiSurface)
	{
		int sw = 0, sh = 0;
		g_pVGuiSurface->GetScreenSize(sw, sh);
		static int s_prevSw = -1, s_prevSh = -1;
		if (sw > 0 && sh > 0 && (sw != s_prevSw || sh != s_prevSh))
		{
			s_prevSw = sw;
			s_prevSh = sh;
			g_options->ClampToCurrentWorkspace(sw, sh);
		}
	}

	if (PocDialog_IsActive() && g_pVGuiInput)
	{
		static int s_focusFrames = 0;
		if (++s_focusFrames >= 30)
		{
			s_focusFrames = 0;
			vgui2::VPANEL focus = g_pVGuiInput->GetFocus();
			const char *name = "(none)";
			if (focus && g_pVGuiPanel)
			{
				const char *n = g_pVGuiPanel->GetName(focus);
				if (n && n[0])
					name = n;
			}
			Menu_Con("CSRETRO_V1POC_FOCUS %s", name);
		}
	}

	if (g_optionsGateFrame >= 0 && g_options)
	{
		++g_optionsGateFrame;
		// Physical capture probe: arm ASAP (ESC/noise can close Options before frame 45).
		if (getenv("CSRETRO_OPTIONS_KEYBOARD_CAPTURE_PHYS"))
		{
			if (g_optionsGateFrame == 5 && !OptionsKeyboard_PhysicalCaptureProbeArmed())
				OptionsKeyboard_ArmPhysicalCaptureProbe(g_options);
			if (g_optionsGateFrame >= 5)
				OptionsKeyboard_PollPhysicalCaptureProbe(g_options);
		}
		if (g_optionsGateFrame == 45)
		{
			if (getenv("CSRETRO_OPTIONS_VIDEO_MODE_SAFETY"))
				OptionsVideoModeSafety_RunGate(g_options);
			else if (getenv("CSRETRO_OPTIONS_VIDEO_GATE"))
				OptionsVideo_RunFunctionalGate(g_options);
			else if (getenv("CSRETRO_OPTIONS_AUDIO_GATE"))
				OptionsAudio_RunFunctionalGate(g_options);
			else if (getenv("CSRETRO_OPTIONS_KEYBOARD_CAPTURE_PHYS"))
			{
				if (!OptionsKeyboard_PhysicalCaptureProbeArmed())
					OptionsKeyboard_ArmPhysicalCaptureProbe(g_options);
			}
			else if (getenv("CSRETRO_OPTIONS_KEYBOARD_GATE"))
				OptionsKeyboard_RunFunctionalGate(g_options);
			else if (getenv("CSRETRO_OPTIONS_LAYOUT_GATE"))
				OptionsLayout_RunFunctionalGate(g_options);
			else
				OptionsMouse_RunFunctionalGate(g_options);
		}
		else if (g_optionsGateFrame == 60)
		{
			if (getenv("CSRETRO_OPTIONS_KEYBOARD_CAPTURE_PHYS"))
			{
				// Stay open for xdotool F8 then letter q — never auto-quit here.
				;
			}
			else if (getenv("CSRETRO_OPTIONS_VIDEO_MODE_SAFETY") && OptionsVideoModeSafety_IsWaitingWallclock())
			{
				// Real wall-clock timeout: keep polling; do not quit yet.
				;
			}
			else
			{
				// zweites Screenshot nach erneutem Paint
				MenuEngine::ClientCmd("screenshot\n");
				Menu_Con(getenv("CSRETRO_OPTIONS_VIDEO_MODE_SAFETY") ? "CSRETRO_MODE_SAFETY_SHOT_TAKEN"
					: getenv("CSRETRO_OPTIONS_VIDEO_GATE") ? "CSRETRO_VIDEO_GATE_SHOT_TAKEN"
					: getenv("CSRETRO_OPTIONS_AUDIO_GATE") ? "CSRETRO_AUDIO_GATE_SHOT_TAKEN"
					: getenv("CSRETRO_OPTIONS_KEYBOARD_GATE") ? "CSRETRO_KEYBOARD_GATE_SHOT_TAKEN"
					: getenv("CSRETRO_OPTIONS_LAYOUT_GATE") ? "CSRETRO_LAYOUT_GATE_SHOT_TAKEN"
									     : "CSRETRO_MOUSE_GATE_SHOT_TAKEN");
				// A screenshot command is consumed on a later engine frame. Keep
				// ticking before graceful quit; otherwise a green gate can exit
				// without ever writing its visual proof.
				if (!getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
					g_optionsGateFrame = -1;
			}
		}
		else if (g_optionsGateFrame == 75 && getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
		{
			Menu_Con("CSRETRO_OPTIONS_GATE_QUIT");
			MenuEngine::ClientCmd("quit\n");
			g_optionsGateFrame = -1;
		}
		else if (g_optionsGateFrame > 60 && getenv("CSRETRO_OPTIONS_KEYBOARD_CAPTURE_PHYS"))
		{
			; // Poll already runs every frame above
		}
		else if (g_optionsGateFrame > 60 && getenv("CSRETRO_OPTIONS_VIDEO_MODE_SAFETY") &&
			 OptionsVideoModeSafety_IsWaitingWallclock())
		{
			if (OptionsVideoModeSafety_Poll(g_options))
				g_optionsGateFrame = -1;
		}
	}
}

void VGuiXash_Paint()
{
	if (!g_inited || !g_pVGuiSurface)
		return;
	vgui2::VPANEL embedded = g_pVGuiSurface->GetEmbeddedPanel();
	if (embedded)
	{
		// forceApplySchemeSettings: sonst bleibt NEEDS_SCHEME_UPDATE → kein Layout, BgColor alpha 0.
		g_pVGuiSurface->SolveTraverse(embedded, true);
		g_pVGuiSurface->PaintTraverse(embedded);
	}
	// Frame/PropertyDialog default to MakePopup — parent-Traverse überspringt Popups.
	if (g_pVGuiPanel)
	{
		const int n = g_pVGuiSurface->GetPopupCount();
		for (int i = 0; i < n; ++i)
		{
			vgui2::VPANEL p = g_pVGuiSurface->GetPopup(i);
			if (!p || !g_pVGuiPanel->IsVisible(p))
				continue;
			g_pVGuiSurface->SolveTraverse(p, true);
			g_pVGuiSurface->PaintTraverse(p);
		}
	}
}

bool VGuiXash_ShowPocDialog()
{
	if (!g_inited)
		VGuiXash_Init();
	return PocDialog_Show(g_root);
}

void VGuiXash_HidePocDialog() { PocDialog_Hide(); }
bool VGuiXash_IsPocActive() { return PocDialog_IsActive(); }

bool VGuiXash_ShowOptionsDialog()
{
	if (!g_inited)
		VGuiXash_Init();
	if (!g_root)
		return false;
	PocDialog_Hide();
	if (!g_options)
		g_options = new COptionsDialog(g_root);
	if (!g_options->HasPages())
	{
		// Keine Stub-Tabs: ohne echte Subpage kein Options-VGUI.
		return false;
	}
	int sw = 640, sh = 480;
	if (g_pVGuiSurface)
		g_pVGuiSurface->GetScreenSize(sw, sh);
	int w = CsretroOptionsClassic::kPreferredWide;
	int h = CsretroOptionsClassic::kPreferredTall;
	int px = (sw - w) / 2;
	int py = (sh - h) / 2;
	int minW = w, minH = h;
	g_options->GetAdaptiveMinimum(minW, minH);

	const bool gateRun =
		getenv("CSRETRO_OPTIONS_GATE") || getenv("CSRETRO_OPTIONS_AUDIO_GATE") ||
		getenv("CSRETRO_OPTIONS_KEYBOARD_GATE") || getenv("CSRETRO_OPTIONS_KEYBOARD_CAPTURE_PHYS") ||
		getenv("CSRETRO_OPTIONS_VIDEO_GATE") || getenv("CSRETRO_OPTIONS_VIDEO_MODE_SAFETY") ||
		getenv("CSRETRO_OPTIONS_LAYOUT_GATE");
	CsretroWindowGeometry::Bounds saved = CsretroWindowGeometry::Load("Options");
	if (!gateRun && saved.valid && saved.w >= minW && saved.h >= minH)
	{
		w = saved.w;
		h = saved.h;
		px = saved.x;
		py = saved.y;
	}
	CsretroWindowGeometry::ClampBounds(px, py, w, h, minW, minH, 0, 0, sw, sh);

	g_options->SetSize(w, h);
	g_options->SetPos(px, py);
	g_options->Activate();
	g_options->GetPos(px, py);
	if (getenv("CSRETRO_OPTIONS_LAYOUT_RESTORE_GATE"))
	{
		int rw = 0, rh = 0;
		g_options->GetSize(rw, rh);
		Menu_Con("CSRETRO_LAYOUT_RESTART_RESTORE bounds=%d,%d %dx%d screen=%dx%d",
			px, py, rw, rh, sw, sh);
	}
	// Gate nach einigen Paint-Frames (Client-CVars + Framebuffer).
	if (getenv("CSRETRO_OPTIONS_GATE") || getenv("CSRETRO_OPTIONS_AUDIO_GATE") ||
		getenv("CSRETRO_OPTIONS_KEYBOARD_GATE") || getenv("CSRETRO_OPTIONS_KEYBOARD_CAPTURE_PHYS") ||
		getenv("CSRETRO_OPTIONS_VIDEO_GATE") ||
		getenv("CSRETRO_OPTIONS_VIDEO_MODE_SAFETY") ||
		getenv("CSRETRO_OPTIONS_LAYOUT_GATE"))
	{
		Menu_Con("CSRETRO_OPTIONS_POPUPS %d visible=%d size=%dx%d pos=%d,%d screen=%dx%d",
			g_pVGuiSurface ? g_pVGuiSurface->GetPopupCount() : -1,
			g_options->IsVisible() ? 1 : 0,
			g_options->GetWide(), g_options->GetTall(), px, py, sw, sh);
		g_optionsGateFrame = 0;
	}
	return true;
}

void VGuiXash_HideOptionsDialog()
{
	if (!g_options)
		return;
	int x = 0, y = 0, w = 0, h = 0;
	g_options->GetBounds(x, y, w, h);
	CsretroWindowGeometry::Save("Options", x, y, w, h);
	g_options->SetVisible(false);
	g_options->Close();
}

bool VGuiXash_IsOptionsActive()
{
	return g_options && g_options->IsVisible();
}

bool VGuiXash_IsVideoCalibrationActive()
{
	return VGuiXash_IsOptionsActive() && g_options->GetPropertySheet()->GetActivePage() ==
		g_options->FindPage("Video");
}

COptionsDialog *VGuiXash_GateGetOptionsDialog() { return g_options; }

bool VGuiXash_ShowCreateGameDialog()
{
	if (!g_inited)
		VGuiXash_Init();
	if (!g_root)
		return false;
	if (!g_createGame)
		g_createGame = new CCreateGameDialog(g_root);
	if (!g_createGame->HasPages())
		return false;

	int sw = 640, sh = 480;
	if (g_pVGuiSurface)
		g_pVGuiSurface->GetScreenSize(sw, sh);
	int w = 0, h = 0;
	g_createGame->GetSize(w, h);
	g_createGame->SetPos((sw - w) / 2, (sh - h) / 2);
	g_createGame->Activate();
	return true;
}

void VGuiXash_HideCreateGameDialog()
{
	if (!g_createGame)
		return;
	g_createGame->SetVisible(false);
	g_createGame->Close();
}

bool VGuiXash_IsCreateGameActive()
{
	return g_createGame && g_createGame->IsVisible();
}

CCreateGameDialog *VGuiXash_GateGetCreateGameDialog()
{
	return g_createGame;
}

bool VGuiXash_ShowServerBrowser()
{
	if (!g_inited)
		VGuiXash_Init();
	if (!g_root)
		return false;
	return ServerBrowser_Show(g_root);
}

void VGuiXash_HideServerBrowser()
{
	ServerBrowser_Hide();
}

bool VGuiXash_IsServerBrowserActive()
{
	return ServerBrowser_IsActive();
}

bool VGuiXash_ToggleConsole() { return GameConsole_Toggle(); }
void VGuiXash_HideConsole() { GameConsole_Hide(); }
bool VGuiXash_IsConsoleActive() { return GameConsole_IsActive(); }
void VGuiXash_ConsolePrint(const char *text) { GameConsole_Print(text); }
void VGuiXash_ConsoleClear() { GameConsole_Clear(); }

bool VGuiXash_ShowMainMenu()
{
	if (!g_inited)
		VGuiXash_Init();
	if (!g_root)
		return false;
	return MainMenu_Show(g_root);
}

void VGuiXash_HideMainMenu() { MainMenu_Hide(); }

bool VGuiXash_IsMainMenuActive() { return MainMenu_IsActive(); }

bool VGuiXash_IsKeyboardCapturing()
{
	return g_options && g_options->IsVisible() && g_options->IsKeyboardCapturing();
}

bool VGuiXash_ShowTeamSelect(int validSlots)
{
	if (!g_inited)
		VGuiXash_Init();
	if (!g_root)
	{
		Menu_Con("CSRETRO_TEAM_VGUI fail — kein VGUI-Root");
		return false;
	}
	ClassSelect_Hide(false);
	BuySelect_Hide();
	RadioSelect_Hide();
	SpectatorHud_Hide();
	return TeamSelect_Show(g_root, validSlots);
}

void VGuiXash_HideTeamSelect() { TeamSelect_Hide(); }

bool VGuiXash_IsTeamSelectActive() { return TeamSelect_IsActive(); }

bool VGuiXash_TeamActivateSlot(int slot) { return TeamSelect_ActivateSlot(slot); }

bool VGuiXash_ShowClassSelect(int menuType, int validSlots)
{
	if (!g_inited)
		VGuiXash_Init();
	if (!g_root)
	{
		Menu_Con("CSRETRO_CLASS_VGUI fail — kein VGUI-Root");
		return false;
	}
	TeamSelect_Hide(false);
	BuySelect_Hide();
	RadioSelect_Hide();
	SpectatorHud_Hide();
	return ClassSelect_Show(g_root, menuType, validSlots);
}

void VGuiXash_HideClassSelect() { ClassSelect_Hide(); }

bool VGuiXash_IsClassSelectActive() { return ClassSelect_IsActive(); }

bool VGuiXash_ClassActivateSlot(int slot) { return ClassSelect_ActivateSlot(slot); }

bool VGuiXash_ShowBuySelect(int menuType, int validSlots)
{
	if (!g_inited)
		VGuiXash_Init();
	if (!g_root)
	{
		Menu_Con("CSRETRO_BUY_VGUI fail — kein VGUI-Root");
		return false;
	}
	TeamSelect_Hide();
	ClassSelect_Hide();
	RadioSelect_Hide();
	SpectatorHud_Hide();
	return BuySelect_Show(g_root, menuType, validSlots);
}

void VGuiXash_HideBuySelect() { BuySelect_Hide(); }

bool VGuiXash_IsBuySelectActive() { return BuySelect_IsActive(); }

bool VGuiXash_BuyActivateSlot(int slot) { return BuySelect_ActivateSlot(slot); }

bool VGuiXash_ShowRadioSelect(int menuType, int validSlots)
{
	if (!g_inited)
		VGuiXash_Init();
	if (!g_root)
	{
		Menu_Con("CSRETRO_RADIO_VGUI fail — kein VGUI-Root");
		return false;
	}
	TeamSelect_Hide();
	ClassSelect_Hide();
	BuySelect_Hide();
	return RadioSelect_Show(g_root, menuType, validSlots);
}

void VGuiXash_HideRadioSelect() { RadioSelect_Hide(); }

bool VGuiXash_IsRadioSelectActive() { return RadioSelect_IsActive(); }

bool VGuiXash_RadioActivateSlot(int slot) { return RadioSelect_ActivateSlot(slot); }

void VGuiXash_HideSpectatorHud() { SpectatorHud_Hide(); }

bool VGuiXash_IsSpectatorActive() { return SpectatorHud_IsActive(); }

void VGuiXash_HideScoreboardHud() { ScoreboardHud_Hide(); }

bool VGuiXash_IsScoreboardActive() { return ScoreboardHud_IsActive(); }

bool VGuiXash_IsInteractiveUiActive()
{
	return VGuiXash_IsPocActive() || VGuiXash_IsOptionsActive() || VGuiXash_IsMainMenuActive() ||
		VGuiXash_IsCreateGameActive() || VGuiXash_IsServerBrowserActive() || VGuiXash_IsConsoleActive() ||
		VGuiXash_IsTeamSelectActive() || VGuiXash_IsClassSelectActive() || VGuiXash_IsBuySelectActive() ||
		VGuiXash_IsRadioSelectActive();
}

bool VGuiXash_IsUiActive()
{
	return VGuiXash_IsInteractiveUiActive() || SpectatorHud_IsActive() || ScoreboardHud_IsActive();
}

void VGuiXash_Key(int key, int down)
{
	if (!g_inited || !g_pVGuiInput)
		return;

	if (CsretroMenu_CaptureDebugEnabled())
	{
		CsretroMenu_CaptureLog("VGuiXash_Key key=%d down=%d options=%d capturing=%d",
			key, down ? 1 : 0,
			(g_options && g_options->IsVisible()) ? 1 : 0,
			(g_options && g_options->IsKeyboardCapturing()) ? 1 : 0);
	}

	// Keyboard capture: consume raw Xash keynums immediately (focus-independent).
	if (g_options && g_options->IsVisible())
	{
		const bool consumed = g_options->OnRawXashKey(key, down);
		if (CsretroMenu_CaptureDebugEnabled())
			CsretroMenu_CaptureLog("OnRawXashKey key=%d down=%d consumed=%d", key, down ? 1 : 0, consumed ? 1 : 0);
		if (consumed)
			return;
	}

	// Mouse buttons arrive as Xash key events.
	vgui2::MouseCode mouse = vgui2::MOUSE_LAST;
	if (key == K_MOUSE1)
		mouse = vgui2::MOUSE_LEFT;
	else if (key == K_MOUSE2)
		mouse = vgui2::MOUSE_RIGHT;
	else if (key == K_MOUSE3)
		mouse = vgui2::MOUSE_MIDDLE;
	else if (key == K_MOUSE4)
		mouse = vgui2::MOUSE_4;
	else if (key == K_MOUSE5)
		mouse = vgui2::MOUSE_5;

	// Xash delivers SDL_MOUSEWHEEL as K_MWHEEL* keys (IN_MWheelEvent).
	// Same physical event, two owners: capture → raw bind (already consumed above);
	// idle → VGUI MouseWheeled so SectionedListPanel/Menu/etc. scroll.
	if (key == K_MWHEELUP || key == K_MWHEELDOWN)
	{
		if (down && g_pVGuiInput)
		{
			const int delta = (key == K_MWHEELUP) ? 1 : -1;
			if (CsretroMenu_CaptureDebugEnabled())
			{
				CsretroMenu_CaptureLog("WHEEL_EVENT key=%d delta=%d capturing=%d owner=vgui_scroll",
					key, delta,
					(g_options && g_options->IsKeyboardCapturing()) ? 1 : 0);
			}
			g_pVGuiInput->InternalMouseWheeled(delta);
		}
		return;
	}

	if (mouse != vgui2::MOUSE_LAST)
	{
		if (down)
		{
			// Xash does not emit double-click events — synthesize for VGUI.
			static int s_lastCode = -1;
			static int s_lastX = 0, s_lastY = 0;
			static long s_lastTime = 0;
			int mx = 0, my = 0;
			g_pVGuiInput->GetCursorPos(mx, my);
			const long nowMs = g_pVGuiSystem ? g_pVGuiSystem->GetTimeMillis() : 0;
			const bool isDouble =
				(static_cast<int>(mouse) == s_lastCode) &&
				(nowMs - s_lastTime) > 0 && (nowMs - s_lastTime) < 400 &&
				std::abs(mx - s_lastX) < 6 && std::abs(my - s_lastY) < 6;
			if (isDouble)
			{
				if (CsretroMenu_CaptureDebugEnabled())
					CsretroMenu_CaptureLog("MouseDoublePressed code=%d at %d,%d", static_cast<int>(mouse), mx, my);
				// Canonical path only: DoublePressed → list ItemDoubleLeftClick → BeginCapture.
				g_pVGuiInput->InternalMouseDoublePressed(mouse);
				s_lastCode = -1;
				s_lastTime = 0;
			}
			else
			{
				if (CsretroMenu_CaptureDebugEnabled())
					CsretroMenu_CaptureLog("MousePressed code=%d at %d,%d", static_cast<int>(mouse), mx, my);
				g_pVGuiInput->InternalMousePressed(mouse);
				s_lastCode = static_cast<int>(mouse);
				s_lastX = mx;
				s_lastY = my;
				s_lastTime = nowMs;
			}
		}
		else
		{
			if (CsretroMenu_CaptureDebugEnabled())
				CsretroMenu_CaptureLog("MouseReleased code=%d", static_cast<int>(mouse));
			g_pVGuiInput->InternalMouseReleased(mouse);
		}
		return;
	}

	vgui2::KeyCode code = KeyCode_VirtualKeyToVGUI(key);
	if (VGuiXash_IsConsoleActive())
	{
		GameConsole_HandleRawKey(key, down);
		return;
	}
	if (MenuEngine::SendWarmupReadyKey(key, down))
		return;
	if (code == vgui2::KEY_NONE)
		return;
	// PoC proof: TAB/BACKSPACE even when a child (TextEntry) holds focus.
	if (down && PocDialog_IsActive())
	{
		if (code == vgui2::KEY_TAB)
			Menu_Con("CSRETRO_V1POC_TAB");
		else if (code == vgui2::KEY_BACKSPACE)
			Menu_Con("CSRETRO_V1POC_BACKSPACE");
	}
	if (down)
	{
		g_pVGuiInput->InternalKeyCodePressed(code);
		g_pVGuiInput->InternalKeyCodeTyped(code);
	}
	else
		g_pVGuiInput->InternalKeyCodeReleased(code);
}

void VGuiXash_MouseMove(int x, int y)
{
	if (!g_inited || !g_pVGuiInput)
		return;
	g_pVGuiInput->InternalCursorMoved(x, y);
	if (g_pVGuiSurface)
		static_cast<vgui2::CSurfaceXash *>(g_pVGuiSurface)->SetCursorPosInternal(x, y);
}

void VGuiXash_Char(int ch)
{
	if (!g_inited || !g_pVGuiInput)
		return;

	// Fallback when SDL text input is active: printables never reach Key_Event.
	// Char codes for a-z / 0-9 are the same Xash keynums — no second translation table.
	if (g_options && g_options->IsVisible() && g_options->IsKeyboardCapturing())
	{
		int keynum = ch;
		if (keynum >= 'A' && keynum <= 'Z')
			keynum = keynum - 'A' + 'a';
		if (keynum >= 32 && keynum < 127)
		{
			CsretroMenu_CaptureLog("VGuiXash_Char capture fallback ch=%d keynum=%d", ch, keynum);
			if (g_options->OnRawXashKey(keynum, true))
				return;
		}
	}

	g_pVGuiInput->InternalKeyTyped(static_cast<wchar_t>(ch));
}
