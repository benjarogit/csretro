# CSRETRO – Gesamtplan

**Stand:** 9. November 2026
**Rolle:** maßgebliche Quelle für Ziele, Entscheidungen und Gates
**Fortschritt:** steht ausschließlich in [`HANDOFF.md`](../../HANDOFF.md), nicht hier
**Repository:** `https://github.com/benjarogit/csretro` (privat)
**Zielplattform:** Linux x86_64 Host, 32-bit Ziel (CachyOS / Arch als Referenz)

---

## 1. Ziel

CSRETRO ist ein eigenständiger, vollständig vendorter Counter-Strike-1.6-Client plus
AMXX-Server-API. Er läuft auf dem **Default-Public-Branch** von CS 1.6, spricht ein
**eigenes Protokoll**, hat eine **eigene Identität**, eine **eigene Infrastruktur**
und eine **zweisprachige Dokumentation**.

Auf CSRETRO-Servern können ausschließlich CSRETRO-Clients spielen. Es gibt keine
Abwärtskompatibilität zu NextClient.

Arbeitsweise: ein Gate nach dem anderen, jedes Gate wird verifiziert, dokumentiert,
committet und gepusht, bevor das nächste beginnt.

---

## 2. Entscheidungen

| Nr. | Thema | Entscheidung |
|---|---|---|
| E01 | Upstream-Bindung | Vollständig vendoren, Historie mitnehmen, Cherry-Picks nur bei Bedarf. Keine Build-Abhängigkeit vom Upstream. |
| E02 | Branch-Ziel | **Nur Default Public** (25th Anniversary, `PatchVersion=1.1.2.7`). Kein `steam_legacy`, kein 8684-Fallback. |
| E03 | Client-Kompatibilität | Eigenes Protokoll, eigene Keys, keine NextClient-Kompatibilität. |
| E04 | Server-Zugang | Nur verifizierte CSRETRO-Clients; Fremdclients werden nach Timeout gekickt. |
| E05 | Namensschema | `csr_` für Cvars/Commands/Natives; `csretro_` für Dateien und Module. |
| E06 | Server-Stack | ReHLDS + ReGameDLL_CS + AMXX 1.10 + ReAPI. |
| E07 | Doku | Zweisprachig DE/EN. `docs/` ist Quelle, GitHub-Wiki wird gespiegelt. |
| E08 | GameUI | NextClients native VGUI2 (`gameui.so`) bleibt. CEF/HTML bleibt Windows-only und unter Linux deaktiviert. Kein Velaron-UI. |
| E09 | Referenzprojekte | FuryBaM/cs16-goldsrc-client und FWGS/cs16-client: nur Ideen und Verfahren, kein Code (GPL). Nicht vendoren. |
| E10 | Server-Management | Eigenes `csretro-server` im Backlog, HLSW-Funktionsumfang als Vorbild. |
| E11 | Telemetrie | Eigener Updater, eigenes Sentry, eigenes GameAnalytics, eigene Discord-App. |
| E12 | Release | Privat. Lizenzfrage nur als Notiz im Handoff. |
| E13 | Redundanz | Single Source of Truth (SSoT) für jede Information, CI-Gates gegen Duplikate. |

---

## 3. Verifizierte Fakten

### 3.1 Upstream-Pins

| Projekt | Commit | Stand |
|---|---|---|
| `CS-NextClient/NextClient` | `24cc98637998640086fda54c341d6842bbd292c5` | main nach 2.6.0-preview.2, Linux-Maps-Pfadfix |
| `CS-NextClient/NextClientServerApi` | `1da463e0548c3bdb69e441caf2a39caae31edffa` | main, nach v1.6.0, Material-API |
| `CS-NextClient/ncl-hl1-source-sdk` | `0e9be00fc52b66742a09c84c8f6169dc7c83ca8a` | Fork von `tmp64/hl1_source_sdk` |

Der API-Tag `v1.6.0` (Juli 2026) kennt die Material-Synchronisation noch nicht.
Deshalb wird `main` gepinnt.

### 3.2 System

| Komponente | Ist | Anforderung |
|---|---|---|
| OS | CachyOS (Arch, rolling) | Linux x86_64 |
| glibc | 2.44 | ≥ 2.43 |
| GCC | 16.2.1 | ≥ 15 |
| CMake | 4.4.5 | ≥ 3.21 |
| Ninja | 1.13.2 | vorhanden |
| Python | 3.14.9 | ≥ 3.12 |
| uv | 0.12.0 | optional |
| Docker | 29.9.3 | für Testserver |
| Multilib | `-m32` funktioniert | erforderlich |
| 32-bit GL/X11/SDL2 | vorhanden | erforderlich |
| Steam | läuft | erforderlich |

### 3.3 CS-1.6-Installation

Pfad: lokale Steam-Bibliothek des Maintainers (nicht Teil des Repositories)
Branch: **Default Public** (`PatchVersion=1.1.2.7`, `hw.so` vom 8. Okt 2024)

| Datei | Größe | SHA-256 |
|---|---|---|
| `hw.so` | 11.556.210 | `fca6628b5a4d76a945e11b9796f327004edc65420d9f9cc23f883143508edd78` |
| `cstrike/cl_dlls/client.so` | 10.649.895 | `2e799ef31931c0f4372ff31ad30f0aab529b16461ed3b0928dfbd958183b2e5a` |
| `engine_i486.so` | 3.650.123 | `31ff0e221742d46567de9b0c215471bb37238a603b7b11a70e2c2671c55da899` |

### 3.4 Warum Public machbar ist

NextClient erkennt den Build über die Dateigröße und akzeptiert nur 8684:

```cpp
if (engine_size == 1641376 || engine_size == 9330714)
    build_version_ = BuildVersion{8684};
else
    LOG(INFO) << "Unsupported engine version";

if (build_version_.build_number != 8684)
    return nullptr;
```

Die Public-Binaries sind jedoch **unstripped**:

| Datei | `.dynsym` | `.symtab` | Debug-Info |
|---|---|---|---|
| `hw.so` | 351 | 9.537 | `.debug_info`, `.debug_line` |
| `client.so` | 93 | 7.641 | `.debug_info`, `.debug_line` |

Erwartete Hooks gegen die statische Symboltabelle geprüft:

- **Engine: 316 von 316** Hook-Namen auflösbar
- **Client: 79 von 79** Hook-Namen auflösbar

Upstream hat den passenden Mechanismus bereits: `SearchType::Symbol` liest die
`.symtab` direkt aus der Datei (Kommentar im Code: „Valve's hw.so keeps unstripped").

Der Linux-Provider besteht fast ausschließlich aus `ExportFunc` (316 Engine, 81 Client),
dazu 9+2 `Symbol`, 7 `NotPresent`, 1 Offset (`EngineMsgBase 0x27D200`) und 1 `String`.
Der Public-Port ist damit primär ein **Fallback von `ExportFunc` auf `Symbol`** plus
Offset-Neuberechnung und ABI-Validierung.

### 3.5 Lizenzen

| Projekt | Lizenz | Konsequenz |
|---|---|---|
| NextClient | **keine LICENSE-Datei** | Standard-Urheberrecht. Privat unkritisch; öffentliche Binär-Releases brauchen Freigabe. |
| NextClientServerApi | MIT | frei |
| ncl-hl1-source-sdk | Valve Source SDK + `thirdpartylegalnotices.txt` | Weitergabe nur kostenlos, Lizenzdateien mitliefern |
| FuryBaM/cs16-goldsrc-client | GPL-3.0 | nur Ideen/Verfahren |
| FWGS/cs16-client | GPL-2.0-or-later + HL-Engine-Exception | nur Ideen/Verfahren |
| portapps/hlsw-portable | MIT | Muster für portables Layout |

### 3.6 Client-Engine: Xash3D-Befund (Spike, 9. November 2026)

Ein Spike hat geprüft, ob Xash3D-FWGS den Retail-Client tragen kann. Ergebnis:
**ja, bis einschließlich `HUD_Init`; der fehlende Surface-Backend ist die
einzige verbleibende Lücke.** Geprüft wurde die Paarung Valve `client.so`
(SHA-256 `2e799ef3…`, identisch mit dem G07a-Registry-Eintrag) und Valve
`vgui2.so` (SHA-256 `f0291c95…`) auf Xash3D-FWGS `e9b94dc` (32 Bit, SDL2
2.32.10, Xvfb/llvmpipe).

Belegt:

- `Initialize` kopiert `cl_enginefunc_t` (536 Byte, Version 7) und liefert `1`.
- Ein spike-only `LD_PRELOAD`-Shim (`kvshim.so`, außerhalb des Repos) stellt ein
  Stub-`VGUI_Surface026` (119 Slots, alle No-ops) bereit, fälscht `SteamApps`/
  `SteamAPI_Init` und ruft `CounterStrikeViewport::Initialize`/`Start`, weil
  Xash3D keine GoldSrc-GameUI hat.
- Damit läuft `HUD_Init` vollständig durch, `CounterStrikeViewport::Start`
  liefert mit gesetztem Viewport-Panel zurück (Feld `0x48` ≠ 0), und die Engine
  fährt bis `exec config.cfg` und sauberem Shutdown auf `+quit`.
- Es wird nichts gerendert: der Stub zeichnet nicht. *(Überholt durch den
  Nachtrag unten: Füllflächen und echter VGUI2-Text erscheinen im Frame.)*

**Kritischer ABI-Befund:** Valves `client.so` linkt statisch nur die
VGUI2-*Controls*; `CScheme`, `CSystem`, `CInput`, `CPanel`, `CLocalize` und das
`KeyValues`-System kommen aus `vgui2.so`. Valves `client.so` ist deshalb nur mit
Valves `vgui2.so` ABI-kompatibel. NextClients `vgui2.so` hat ein anderes
`IScheme`-Vtable-Layout; die Paarung mit Valves `client.so` stürzt in
`Label::ApplySettings` (`pScheme->GetFont`, Slot 6) ab. NextClient-Binaries sind
nur untereinander kompatibel (`client_mini.dll` + `vgui2.so`). CSRETRO baut
Client und VGUI2 daher aus **derselben** SDK-Revision (vendored
`ncl-hl1-source-sdk`) und mischt keine vorgebauten Fremd-Binaries.

Xash3D liefert **FreeVGUI** (`3rdparty/freevgui`, BSD-3-Clause) mit einer
nativen Xash3D-Platform-Surface (`platform/xash3d-fwgs/surface.cpp`). Das ist
VGUI1, nicht VGUI2: es zeigt, wie 2D-Zeichnen auf Xash3D geht, stellt aber kein
`VGUI_Surface026`.

Damit ist die Engine-Frage entschieden: **Option A ist der Weg.** Verbleibende
Arbeit ist ein echtes `VGUI_Surface026`-Backend über Xash3D (Zeichnen, Fonts,
Texturen, Maus), kein weiterer ABI-Blocker. Option B bleibt Rückfallebene,
Option C (Retail) bleibt der funktionierende Vergleichsstand.

| Option | Inhalt | Stand |
|---|---|---|
| A | `VGUI_Surface026`-Adapter auf Xash3D | **gewählt**; ABI belegt, Backend zeichnet Text |
| B | FreeVGUI um GoldSrc-VGUI2-Exporte erweitern | Rückfallebene, größerer Umbau |
| C | Retail-GoldSrc als Client-Engine | funktioniert, aber kein Xash3D |

#### 3.6.1 Nachtrag 10. November 2026: Text und Flächen sind sichtbar

Der Stub-Adapter zeichnet jetzt echte VGUI2-Ausgabe in einen gerenderten
Xash3D-Frame: das Scoreboard-Dialog zeichnet seine eigenen Strings
(`Spectators - 0 players`, Spielername `benny`, Spaltenköpfe `Score`, `Deaths`,
`Latency`) in seiner eigenen Farbe an seinen eigenen Koordinaten über der
beleuchteten `de_dust2`-Szene. Beleg:
`docs/evidence/g07-xash3d-vgui2-text.png`, Zoom
`docs/evidence/g07-xash3d-vgui2-text-zoom.png`.

Ursache der bisherigen Unsichtbarkeit war die **Zeichenreihenfolge**, nicht die
Geometrie:

1. `kvshim.so` ruft den Stub an zwei Stellen: `frame_wrap` (Client-Export
   `Frame`, aus `Host_ClientFrame`, also **vor** `R_RenderScene`) und
   `redraw_wrap` (`HUD_Redraw`, aus `CL_DrawHUD`, also **nach** der Welt und
   innerhalb des 2D-Blocks der Engine). Ein Paint aus `frame_wrap` wird von der
   Welt überschrieben; nur `redraw_wrap` bleibt sichtbar (gdb: `scene=0` bzw.
   `scene=1`, `flag2d=1`).
2. VGUI emittiert Text-Draws nur im **ersten** Paint des Prozesses, obwohl
   `CSectionHeader::Paint`/`Label::Paint` jeden Frame laufen. Weil `frame_wrap`
   pro Frame zuerst läuft, verbrauchte er diese eine Emission.
3. Wird der `frame_wrap`-Paint abgeschaltet (Spike-Shim patcht
   `mov eax,[ebx+0x110]` auf `xor eax,eax`), ist der HUD-Paint der erste Paint
   und der Text landet nach der Welt.

Projektion, Depth und Scissor sind korrekt (Viewport `0,0 1024x768`,
Scissor-Test aus, Depth-Test aus, Ortho `0..1024 × 0..768` top-down). Die
Verifikation erfolgt über `glReadPixels(GL_BACK)` unmittelbar vor und nach dem
HUD-Paint: 1755 geänderte Pixel, Bounding-Box Bildschirmzeilen 5..23, Spalten
56..810, `glGetError()` 0, in zwei Läufen identisch. Ein X11-Screenshot des
Front-Buffers verpasst den Frame, weil der Text nur einmal gezeichnet wird.

Der Text erscheint inzwischen in **jedem** Frame. Ursache der Einmal-Emission
war wieder der Stub, nicht VGUI: `stub_surface_paint` erhöht einen Paint-Zähler
und rief pro Frame `Panel::InvalidateLayout(true, true)` auf der Wurzel auf
(Stub-Offset `0x1b9e` → `0x1afd`, Vtable-Slot `0xf4`). Der zweite Parameter
lädt das Scheme neu: `PerformApplySchemeSettings` →
`SectionedListPanelHeader::ApplySchemeSettings` → `ClearImages()`, und
`Label::ApplySchemeSettings` leert die Bildliste. Die Spaltenbilder entstehen
erst wieder in `PerformLayout`, das in diesem Pfad nicht läuft. Ein
gdb-Watchpoint belegt den Schreibzugriff beim zweiten Paint über die Kette
`paint_tree` → `InvalidateLayout` → `PerformApplySchemeSettings` →
`CSectionHeader::ApplySchemeSettings` → `Label::ApplySchemeSettings`
(`Label.cpp:1001`).

Der Spike patched daher nur den ersten Aufruf durchzulassen: an `0x1b9a`
springt der Code in eine Code-Cave, die den Paint-Zähler mit `1` vergleicht
und den Aufruf ab Paint 2 überspringt (`spike-shims/patch_once.py` im
Spike-Baum). Ergebnis: 50 Text-Aufrufe pro Paint, konstant über zwölf
aufeinanderfolgende Paints (ungepatcht: 50 nur in Paint 1, danach 0), und der
Backbuffer-Diff um den HUD-Paint ist bei Paint 1 und Paint 60 bit-identisch
(1755 Pixel, Bildschirmzeilen 5..23, Spalten 56..810, `glGetError()` 0).

#### 3.6.2 Nachtrag 10. November 2026: gefüllte Panels

Der Adapter zeichnet auch die Flächen. Beleg:
`docs/evidence/g07-xash3d-vgui2-filled-panel.png` und Zoom
`docs/evidence/g07-xash3d-vgui2-filled-panel-zoom.png` (Paint 20,
`glReadPixels` vor/nach dem HUD-Paint: 499 200 geänderte Pixel = exakt
832 × 600, Bildschirmzeilen 0..599, Spalten 0..831, `glGetError()` 0; Pixel
(400,300) `32,30,26` → `220,142,26`).

Der frühere Befund „nur ein heller Balken über Zeilen 0..37" war ein
**Artefakt des Messaufbaus**, kein Zeichenfehler. Die Dumps entstanden nur beim
ersten Paint, und in Paint 1 liefert der Stub seine Flächen noch mit
`255,255,255` und VGUI-Alpha 0. VGUI zählt 0 als opak, der Stub invertiert zu
Engine-Alpha 0, also vollständig transparent; erst ab Paint 2 setzt das Scheme
`188,112,0` bei Alpha 128 durch. Sichtbar blieb in Paint 1 deshalb nur das eine
Rechteck mit echtem Alpha (`0,0 400x38`, Alpha 127).

Gegenproben:

- Ein direkter Aufruf von `CL_FillRGBA` (`libxash.so` Offset `0xfc270`) mit
  `10,0 811x599 rgba=188,112,0,127` unmittelbar nach dem Paint füllt exakt
  485 789 Pixel (Zeilen 0..598, Spalten 10..820); die Engine-Seite ist in
  Ordnung.
- gdb an `fill_rect+0x79` (`0x17b9`) zeigt korrektes Marshallen von
  `x,y,w,h,r,g,b,a`; Alpha wird mit `not`/`movzbl` invertiert, `R_FillRGBA`
  empfängt dieselben Werte als Floats. Der frühere Farb-/Alpha-Verdacht war
  falsch: die geloggten Werte entstehen nach dem Engine-Aufruf und sind
  irreführend.

Steady State: 12 `fill_rect`-Aufrufe pro Paint (Paint 1: 11), konstant über
alle Frames. Paint 2 und Paint 32 sind bit-identisch; Paint 32 zu 62
unterscheidet sich nur in 444 Pixeln (animierte Szene hinter dem
halbtransparenten Panel).

#### 3.6.3 Herkunft der Füllungen und der fehlende Texturpfad

Ein gdb-Backtrace an jedem der zwölf `fill_rect`-Aufrufe eines Paints
(Paint 2) ordnet jede Fläche ihrem Aufrufer zu:

| Rect | Aufrufer |
|---|---|
| `0,10 10x579`, `10,0 811x599`, `821,10 10x579` | `CCSClientScoreBoardDialog::PaintBackground` (`cstrikeclientscoreboard.cpp:402/409/416`) |
| `0,10 1x580`, `10,0 812x1`, `10,599 812x1`, `831,10 1x580` | dito, `:428/434/442/450` |
| `0,0 832x600` | `vgui2::Panel::PaintBackground` (`Panel.cpp:556`) |
| `1,21 819x1` (zweimal) | `vgui2::CSectionHeader::Paint` (`SectionedListPanel.cpp:97`) |
| `0,0 822x33` | `vgui2::CItemButton::PaintBackground` (`SectionedListPanel.cpp:501`) |
| `0,0 400x38` | `vgui2::Panel::PaintBackground` (`Panel.cpp:556`) für das Label `ServerName` |

Alle Flächen kommen also aus echtem Client-/VGUI2-Code; der Stub marshallt nur.
Der weiße Balken `0,0 400x38` ist der Default-Hintergrund
`255,255,255,128` des Labels `ServerName` (gdb: `this->GetClassName()` =
`Label`, `GetName()` = `ServerName`, `_bgColor` bleibt der VGUI2-Default).
Der Text darauf ist die Scheme-Farbe `BaseText` `255 176 0 255`
(`resource/ClientScheme.res:24`), die VGUI über `DrawSetTextColor` setzt und
die der Stub korrekt an die Engine gibt. Beides ist eine Client-Entscheidung,
kein Stub-Defekt; unklar bleibt nur, ob der Retail-Client denselben Balken
zeigt (Maintainer-Lauf, Automation endet an der Prozessgrenze).

**Texturen fehlen vollständig.** Die vier Textur-Slots des Stubs sind
No-Ops: `draw_set_texture_file` (`0x12f0`), `draw_set_texture_rgba`
(`0x1300`), `draw_set_texture` (`0x1310`) und `draw_textured_rect`
(`0x1340`) sind jeweils ein einzelnes `ret`, `create_new_texture_id`
(`0x1360`) vergibt nur fortlaufende Zahlen und `draw_get_texture_size`
(`0x1320`) meldet konstant `16x16`. VGUI fragt pro Paint acht Texturen an:

```text
gfx/vgui/round_corner_nw        gfx/vgui/round_corner_small_nw
gfx/vgui/round_corner_ne        gfx/vgui/round_corner_small_ne
gfx/vgui/round_corner_sw        gfx/vgui/round_corner_small_sw
gfx/vgui/round_corner_se        gfx/vgui/round_corner_small_se
```

Die Dateien liegen im Spike-Baum (`cstrike/gfx/vgui/*.tga`), werden aber nie
geladen. Aufrufer ist `vgui2::Bitmap::Paint` (`Bitmap.cpp:134`):
`DrawSetTexture` (Vtable-Slot `0x68`) → `DrawSetColor` (Slot `0x20`) →
`DrawTexturedRect` (Slot `0x78`, additiv Slot `0x190`). Weil der Stub alle vier
Slots verschluckt, bleibt von diesen Bitmaps nichts sichtbar.

Zweiter Befund daraus: alle acht `DrawTexturedRect`-Aufrufe kommen mit
`0,0 10,10` an, obwohl die vier Ecken an verschiedenen Stellen liegen. VGUI
zeichnet Bitmaps in **elternrelativen** Koordinaten und erwartet die
Verschiebung aus `PushMakeCurrent` (Stub `0x1de0`, loggt nur). Der Stub
übersetzt nicht, deshalb landen alle Ecken auf `0,0`. Ein Texturpfad braucht
also beides: TGA-Laden plus Upload **und** einen Koordinaten-Offset aus
`PushMakeCurrent`/`PopMakeCurrent` (`0x11b0`).

Offener Rest daraus: Texturen (TGA → GL-Textur, `DrawTexturedRect` als Quad,
echte `DrawGetTextureSize`-Werte) und der Push/Pop-Offset. Danach Fonts
(`DrawPrintText` nutzt bereits `CL_DrawString`, aber ohne Metrik aus
`draw_set_text_font`).

Nebenbefunde:

- `engine_mini`/`nitro_api` haken in `hw.so` (`vgui2_surface` u. a.). Mit Xash3D
  entfällt `hw.so`; die Hooks brauchen einen Xash3D-Engine-Adapter.
- HTML/MOTD verlangt einen ChromeHTML-Controller/Serializer. Ohne CEF wird MOTD
  in CSRETRO deaktiviert oder durch eine einfache Text-Ansicht ersetzt.
- Ein x86_64-Client setzt quelloffenen 64-Bit-Client-Gamecode voraus. Valves
  `client.so` ist ausschließlich 32 Bit.

---

## 4. Repo-Struktur

```
csretro/
├─ AGENTS.md                     # Harness: verbindliche Arbeitsregeln
├─ HANDOFF.md                    # Übergabestatus, Beweise, nächster Task
├─ CREDITS.md                    # alle übernommenen Projekte + Lizenzen
├─ CHANGELOG.md
├─ VERSION                       # CSRETRO-Version
├─ versions.lock.json            # Upstream-SHAs, Tool-Matrix, Hashes
├─ README.md / README.de.md
├─ client/                       # vendorter NextClient (Historie)
│  ├─ CMakeLists.txt
│  ├─ CMakePresets.json
│  ├─ dep/ncl-hl1-source-sdk/    # Submodul
│  ├─ vcpkg/                     # Submodul
│  ├─ nextclient/                # wird in G08 zu csretro/
│  └─ assets/
├─ server/
│  ├─ api/                       # vendorte NextClientServerApi
│  └─ pack/                      # AMXX-Config, Testplugins, Docker
├─ docs/
│  ├─ de/
│  └─ en/
├─ scripts/
│  ├─ doctor.sh
│  ├─ bootstrap.sh
│  ├─ build-client.sh
│  ├─ build-server.sh
│  ├─ install.sh
│  ├─ verify.sh
│  ├─ test-all.sh
│  ├─ clean.sh
│  └─ sync-upstream.sh
├─ tests/
│  ├─ shell/
│  ├─ python/
│  └─ e2e/
├─ tools/
│  ├─ docs_drift.py
│  ├─ proto_sync.py
│  ├─ keygen.sh
│  └─ release.py
├─ patches/                      # Arch-/CMake-4-Patches mit Begründung
└─ .github/workflows/
   ├─ ci.yml
   ├─ docs.yml
   └─ release.yml
```

---

## 5. Harness

### 5.1 AGENTS.md – Pflichtinhalt

1. Zweck und Nicht-Ziele (Linux-first, nur Public, keine NextClient-Kompatibilität)
2. Arbeitszyklus: Task → umsetzen → Test → Doku → Verify → Commit → Push → CI → Handoff
3. Definition of Done
4. Branch-Modell und Commit-Convention
5. Testpflichten je Änderungstyp
6. Doku-Pflichten (DE+EN, Drift-Check)
7. Cleanup-Regeln
8. Sicherheitsregeln (VAC, Secrets, keine Steam-Dateien)
9. SSoT-Matrix und Redundanzverbot
10. Umgebung und Pfade
11. Sprache: Code/Kommentare/Commits EN, Doku DE+EN

### 5.2 Definition of Done

Ein Gate ist fertig, wenn **alle** Punkte belegt sind:

- Build grün, alle zugehörigen Tests grün
- `scripts/verify.sh` grün
- Doku DE+EN aktualisiert, Drift-Check grün
- `HANDOFF.md` aktualisiert (Stand, Beweise, nächster Task)
- Commit nach Convention, Push erfolgt
- CI-Lauf grün, Link im Handoff
- Keine Temp-, Log-, Cache- oder toten Dateien im Baum

„Verifiziert" heißt: Log, Testausgabe, Hash-Manifest oder CI-Link. Keine Behauptung.

### 5.3 Branch und Commits

- Branch pro Gate: `gate/G04-client-build`
- PR gegen `main`, Squash-Merge, CI Pflicht
- Conventional Commits:

```
feat(client): add public build address provider
fix(protocol): reject mismatched protocol version
build(deps): bump vcpkg baseline
ci(linux): add arch container job
docs(wiki): add cvar reference
test(server): cover handshake timeout
chore(repo): remove legacy 8684 providers
```

Scopes: `client`, `server`, `api`, `protocol`, `docs`, `ci`, `scripts`, `deps`, `repo`.

### 5.4 Single Source of Truth

| Information | Quelle | Abgeleitet |
|---|---|---|
| Cvar-Defaults | Code (`cvar_defaults.h`, Registrierung) | Wiki-Tabellen (generiert) |
| Commands/Natives | Code + `csretro_api.inc` | Wiki-Tabellen (generiert) |
| Protokoll | ein gemeinsames `csr_proto.h` | Client und Server binden dasselbe Header ein |
| Version | `VERSION` | CMake, Launcher, Doku |
| Upstream-Pins | `versions.lock.json` | CI, Doku |
| Credits | `CREDITS.md` | Wiki verlinkt, kopiert nicht |
| Build-Flags | CMake-Presets | Skripte rufen nur Presets auf |
| Doku DE/EN | `docs/de`, `docs/en` | Wiki wird generiert |

**Redundanzverbot:**

- Referenzprojekte werden nicht vendort (FuryBaM, cs16-client, HLSW).
- Keine doppelten Cvar-Defaults in UI-Code.
- Ein generischer Adress-Provider statt Kopien pro Build.
- Upstream-Doku wird nicht übersetzt, sondern als Referenz belassen.
- CI-Gates: `docs_drift.py`, `proto_sync.py`, Grep-Gate, Linkcheck.

### 5.5 Cleanup

- Ignoriert und nach jedem Gate entfernt: `build/`, `out/`, `install/`, `.cache/`, Logs, `.env`, `CMakeUserPresets.json`, `vcpkg_installed/`
- `scripts/clean.sh` ist der einzige erlaubte Weg
- Keine Secrets, keine Steam-Dateien im Repo

---

## 6. HANDOFF.md

Template:

```markdown
# Handoff

## Stand
- Datum:
- Branch/Commit:
- CI:
- Version:

## Verifiziert (mit Beweis)
- ...

## Offen / bekannt fehlerhaft
- ...

## Nächster Task
- Gate:
- Abnahme:

## Offene Entscheidungen
- ...

## Umgebung
- Host, Toolchain, Pfade, Steam-Library

## Letzte Befehle und Ergebnisse
- ...
```

---

## 7. Gate-Plan

### P1 – Fundament

#### G01 – Repo & Harness

**Ziel:** Ein Repo, in dem jede Aufgabe nachweisbar abgeschlossen wird.

**Aufgaben:**
- `git init`, Remote `origin` → `benjarogit/csretro`
- `.gitignore`, `.gitattributes` (LFS-Regeln vom Upstream), `.editorconfig`
- `.clang-format`, `.clang-tidy` übernehmen
- `AGENTS.md` mit allen Regeln aus Abschnitt 5
- `HANDOFF.md`, `CHANGELOG.md`, `VERSION` (`0.1.0-dev`)
- `versions.lock.json` mit Pins und Tool-Matrix
- `README.md`, `README.de.md`, `CREDITS.md` (Gerüst)
- `docs/de/`, `docs/en/` mit Index
- `scripts/verify.sh`: Shellcheck, Secret-Scan, Doku-Check
- `.github/workflows/ci.yml`: Lint, Secret-Scan, Markdown-Check

**Abnahme:** CI grün, `verify.sh` grün, Push auf `main`, Branch Protection aktiv.

#### G02 – Vendor-Import

**Ziel:** Beide Upstreams mit Historie im Repo, reproduzierbar auscheckbar.

**Aufgaben:**
- `git subtree add --prefix=client` (NextClient, Commit `90b4bf7`)
- `git subtree add --prefix=server/api` (ServerApi, Commit `1da463e`)
- Submodule `client/vcpkg` und `client/dep/ncl-hl1-source-sdk` initialisieren
- Git LFS konfigurieren und pullen
- `scripts/sync-upstream.sh`: fetch, Diff-Report, Cherry-Pick-Hilfe
- `versions.lock.json`: SHAs, Release-Checksums, SDK-Pin
- `scripts/bootstrap.sh` (Repo-Anteil)

**Abnahme:** Frischer Clone + `bootstrap.sh` erzeugt identische Hashes; Upstream-Historie in `git log` sichtbar.

**Risiko:** LFS-/Submodul-Fehler bei flachem Clone. Wird durch `bootstrap.sh`-Test abgedeckt.

#### G03 – Toolchain CachyOS

**Ziel:** Jede Maschine kann den Build reproduzieren.

**Aufgaben:**
- `scripts/doctor.sh`: OS, glibc ≥ 2.43, GCC ≥ 15, CMake ≥ 3.21, Ninja, Python ≥ 3.12, `uv`, `git-lfs`, `-m32`-Testcompile, 32-bit GL/X11/Xext/Xcursor/Xi/Xrandr/Xss/Xfixes/Xrender/SDL2, Mesa/llvmpipe, Steam-Runtime, ≥ 20 GB frei
- `scripts/bootstrap.sh`: fehlende Arch-Pakete installieren, vcpkg bootstrappen
- `docs/de/build/prerequisites.md`, `docs/en/build/prerequisites.md`

**Abnahme:** `doctor.sh` grün auf leerer Shell; `bootstrap.sh` idempotent.

**Risiko:** Arch-Multilib weicht von Ubuntu 26.04 ab. Paketliste wird in G03 empirisch festgezogen.

---

### P2 – Basis

#### G04 – Client-Build (Linux, x86)

**Ziel:** Der vendorte Client baut vollständig auf CachyOS.

**Aufgaben:**
- `cmake --preset linux` in `client/`
- Reihenfolge: `client_mini` → `engine_mini` → `nitro_api` → `gameui` → `launcher` → `BUILD_ALL`
- `vgui2` und `gameui.so` ausdrücklich mitbauen
- Arch-/CMake-4-Patches nach `patches/` mit Begründung
- `scripts/build-client.sh` mit `--config`, `--target`, `--install-dir`
- Hash-Manifest, Buildzeit und Speicherbedarf dokumentieren

**Abnahme:** `BUILD_ALL` grün; alle Artefakte 32-bit ELF; Manifest vollständig.

**Risiko:** vcpkg-Triplet `x86-linux` auf Arch. Falls nicht reproduzierbar: Container-Build als Standard, Host-Build „best effort". Entscheidung im Handoff, nicht stillschweigend.

#### G05 – Server-API-Build

**Ziel:** Das AMXX-Modul baut und seine Tests laufen.

**Aufgaben:**
- CMake 3.16 → 3.21+, CMake-4-Kompatibilität
- `nextclientapi_amxx` als i386-`.so`
- CTest komplett
- Pawn-Testplugin mit AMXX-Compiler kompilieren
- `scripts/build-server.sh`

**Abnahme:** alle CTest-Suiten grün; `.so` und Modul-Metadaten korrekt.

#### G06 – Testserver

**Ziel:** Ein reproduzierbarer Server für E2E-Tests.

**Aufgaben:**
- Docker-Compose: ReHLDS + ReGameDLL_CS + AMXX 1.10 + ReAPI + Modul + Testplugin
- Healthcheck, Logs, definierter Start/Stop
- HLDS-Basisdateien (App 90) per SteamCMD oder gepflegtes ReHLDS-Image
- `tests/e2e/`-Grundgerüst

**Abnahme:** Server startet reproduzierbar, Modul lädt, Testplugin meldet sich, automatisierter Check.

**Risiko:** SteamCMD-Verfügbarkeit für App 90. Fallback: gepflegtes ReHLDS-Image.

---

#### G06b – Upstream-Radar

**Ziel:** Vor der Public-Bindung in G07 wissen, was sich seit den vendorten Pins
upstream bewegt hat, und bewusst entscheiden, was davon ins Projekt kommt.

**Aufgaben:**
- `scripts/sync-upstream.sh` für NextClient, NextClientServerApi,
  ncl-hl1-source-sdk und vcpkg ausführen
- Jede Änderung seit dem Pin klassifizieren: **übernehmen**, **zurückstellen**
  oder **ignorieren**, jeweils mit Begründung
- Pin-Bumps nur für tatsächlich importierte Änderungen; `versions.lock.json`
  aktualisieren
- Ergebnis in `HANDOFF.md` und `CHANGELOG.md` festhalten

**Abnahme:** Radar-Report vollständig, jede Änderung klassifiziert, Pins und Doku
konsistent, CI grün.

**Risiko:** Upstream hat force-gepusht oder Historie gelöscht. Dann den Pin als
nicht mehr auflösbar markieren, den letzten erreichbaren Stand sichern und die
weitere Bindung neu bewerten.

---

### P3 – Public & Identität

#### G07 – Public-Runtime

**Ziel:** CSRETRO läuft auf dem Default-Public-Branch.

| Schritt | Inhalt | Abbruch, wenn … |
|---|---|---|
| G07a | Build-Registry: Public-Build per SHA-256 von `hw.so`/`client.so` erkennen | Build nicht eindeutig identifizierbar |
| G07b | Resolver dreistufig: `dynsym` → `symtab` → Pattern | Hooks nicht auflösbar |
| G07c | `EngineMsgBase` und übrige Offset-/String-Einträge für Public neu bestimmen | Offset nicht reproduzierbar |
| G07d | Erster Start: `hw.so` wird gepatcht, Konsole antwortet, kein Crash | Crash vor der Konsole |
| G07e | ABI-Validierung gegen Public (DWARF vorhanden) | Layouts weichen strukturell ab |
| G07f | Feature-Smoke: Radar, HUD, Viewmodel, Netzwerk, Handshake | Kernfeature nicht reparabel |

**Abnahme:** Alle sechs Schritte belegt; `csr_version` (später) antwortet auf Public.

**Wenn G07a–G07d scheitern:** Stopp, Neubewertung der Architektur. Kein Legacy-Fallback.
Option wäre ein Hybrid aus `client.so`-Ersatz und Engine-Patches (FuryBaM-Ansatz).

#### G08 – Rebrand & Protokoll v1

**Ziel:** CSRETRO ist eine eigene Identität mit eigenem Protokoll.

**Aufgaben:**
- `ncl_` → `csr_`, `NextClient` → `CSRETRO`, Modul-/Dateinamen
  (`csretro_client.dll`, `csretro_engine.dll`, `csretro_nitro_api.dll`, `csretro_gameui.so`, `csretro.sh`)
- Interne C++-Namespaces und Klassennamen bleiben vorerst unverändert; G08
  benennt nur ausgelieferte Namen um
- Protokoll: Header `nclm` → `csrm`, eigene Opcodes, Version `CSR-PROTO 1`
- Eigene RSA-Keys + `pkeys`-Rotation (`tools/keygen.sh`)
- Pflicht-Handshake mit Timeout-Kick für Fremdclients
- 8684-Provider löschen (`EngineAddressProvider8684*`, `ClientAddressProvider8684*`)
- Sentry/GameAnalytics/Updater-URLs/Discord-ID entfernen bzw. auf Platzhalter
- CEF/Browser entfernen, falls Windows kein Ziel ist
- Grep-Gate: kein `nextclient`, `ncl_`, `8684` außer Credits/Referenzen
- `docs/` DE+EN aktualisieren

**Abnahme:** Protokoll-Unit-Tests, Grep-Gate, Fremdclient wird nachweislich abgelehnt, Client und Server sprechen ausschließlich CSR-PROTO 1.

#### G09 – Installation & E2E

**Ziel:** CSRETRO läuft auf dem echten System, installiert über einen vom
Benutzer ausführbaren Installer.

**Aufgaben:**
- Separate Spielkopie außerhalb der Steam-Library
- `scripts/install.sh` als benutzbarer Installer: kopieren, Rechte setzen,
  Hashes prüfen, Backup, `--dry-run`, klare Fehlermeldungen, kein Root-Zwang
- Import der Client-Daten aus einer Steam-Installation über eine **Allowlist**:
  nur benötigte Verzeichnisse und Dateien übernehmen (`cstrike`, `valve`,
  `platform`, Engine- und Client-Binaries, VGUI2, nötige Assets). Nicht
  übernehmen: Steam-Binaries, CEF/HTML-Cache, Logs, Crashes, Demos, lokale
  Konfigurationen, Download-Caches, Fremd-Mods. Der Import berichtet, was
  übersprungen wurde und warum.
- Importierte Retail-Binaries gegen die Build-Registry prüfen (SHA-256)
- Zielauswahl `--target client|server|hltv|all` mit getrennten Verzeichnissen
- Starter und Verknüpfungen: `csretro.sh` (Client), `csretro-server.sh`
  (Server), `csretro-hltv.sh` (HLTV), jeweils mit `.desktop`-Eintrag, Icon und
  sinnvoller Menükategorie; relative Pfade, keine absoluten Pfade des Erstellers
- Starter prüft, ob Steam läuft, und startet es bei Bedarf, bevor der Client
  startet; der Start wird abgewartet und ein Fehlschlag klar gemeldet
- `--uninstall`, das nur installierte Dateien entfernt und die Steam-Installation
  unberührt lässt
- Start über `csretro.sh`
- `csr_version`, Connect zum Testserver, Handshake
- Smoke: Radar, Killfeed, Viewmodel, Server-API
- Manuelle Checkliste dokumentieren

**Abnahme:** Lauf bestätigt, Logs + Screenshots, Handoff aktualisiert; Installer
und Deinstallation auf einem sauberen Benutzerkonto nachvollzogen.

**Sicherheit:** niemals im Steam-Ordner installieren, niemals auf VAC-Servern
testen, keine Steam-Zugangsdaten annehmen oder speichern.

---

### P4 – Qualität & Doku

#### G10 – Testmatrix & CI

**Aufgaben:**
- CI-Jobs: Client-Build, Server-Build, alle CTest, Python, Shell, E2E, Docs-Drift, Linkcheck, Secret-Scan
- `scripts/test-all.sh` als lokales Abbild
- Matrix dokumentieren (was läuft wo, was braucht Mesa/GPU)
- Artefakt-Upload

**Abnahme:** CI grün auf frischem Clone.

#### G11 – Wiki & Credits

**Aufgaben:**
- Wiki DE+EN: Credits, Features, Installation, Build, Cvars, Commands, Server-API, Protokoll, Troubleshooting, FAQ, Architektur, Privacy
- `tools/docs_drift.py`: Cvars/Commands/Natives aus Code extrahieren, Tabellen generieren, mit Doku vergleichen
- `tools/proto_sync.py`: Client-/Server-Protokollheader vergleichen
- GitHub-Wiki-Spiegel-Workflow
- `CREDITS.md` vollständig, Wiki verlinkt

**Abnahme:** Wiki vollständig, Drift-Check grün, Spiegel aktuell.

---

### P5 – Infrastruktur

#### G12 – Telemetrie

| Komponente | Bereitstellung | Umsetzung |
|---|---|---|
| Discord | App-ID + Icon im Developer Portal | `csr_discordrpc*`, Join-URL, Presence |
| Sentry | Projekt + DSN | Opt-in-Crashreport, lokale Dumps |
| GameAnalytics | Game-Key + Secret | Opt-in-Events, dokumentierte Event-Liste |
| Secrets | GitHub Secrets | `.env.example`, niemals Keys im Repo |

**Abnahme:** Telemetrie nur nach Opt-in, Privacy-Doku, Tests.

#### G13 – Updater

**Aufgaben:**
- Manifest: Version, Dateien, SHA256, Signatur
- Signatur Ed25519/minisign, Keygen + Verify
- `csretro-update`: check → download → verify → apply → rollback
- Hosting: GitHub Releases primär, Mirror optional
- Integration in `csretro.sh` als `--update` (opt-in)

**Abnahme:** Update 0.1.0 → 0.1.1 im Test, Rollback funktioniert, manipulierte Signatur wird abgelehnt.

---

### P6 – Release

#### G14 – Release 0.1.0

**Aufgaben:**
- Tag `v0.1.0`
- Artefakte, SHA256, Manifest, Release Notes
- Provenance/Checksums
- Handoff abschließen

**Abnahme:** Release vorhanden, alle Prüfsummen verifiziert.

---

## 8. Teststrategie

| Ebene | Werkzeug | Läuft |
|---|---|---|
| Client-Unit | CTest | lokal + CI |
| Offscreen-GL | CTest + llvmpipe | lokal + CI |
| Server-Unit | CTest + Fakes | lokal + CI |
| Pawn-Plugin | AMXX-Compiler | lokal + CI |
| Python-Tools | pytest via `uv` | lokal + CI |
| Shell/Install | Shellcheck + Skript-Tests | lokal + CI |
| E2E | Docker + Protokoll-Client | lokal + CI |
| Doku | Drift + Linkcheck | lokal + CI |

Regeln:

- Das Spiel wird **nie** automatisch gestartet. Automatisiert wird bis zur Prozessgrenze.
- Laufzeitbeobachtung macht der Mensch, das Ergebnis kommt in den Handoff.
- Ein Test ohne reproduzierbaren Fehlerfall ist kein Test.

---

## 9. Rebrand & Protokoll

### 9.1 Namensschema

| Alt | Neu |
|---|---|
| `ncl_` (Cvar/Native/Command) | `csr_` |
| `next_engine_mini.dll` | `csretro_engine.dll` |
| `nitro_api2.dll` | `csretro_nitro_api.dll` |
| `client_mini.dll` | `csretro_client.dll` |
| `gameui.so` | `csretro_gameui.so` |
| `nextclient.sh` | `csretro.sh` |
| `next_version` | `csr_version` |
| `next_client_api.inc` | `csretro_api.inc` |
| `addons/amxmodx/data/nextclient_api/` | `addons/amxmodx/data/csretro/` |
| `nclm`-Header | `csrm`-Header |

### 9.2 Protokoll v1

- Eigene Magic, eigene Version, eigene Opcodes
- Client und Server binden dasselbe `csr_proto.h` ein
- Pflicht-Handshake: ohne gültige Verifikation Kick nach Timeout
- Eigene RSA-Key-Paare, Rotation über `pkeys/active.txt`
- `proto_sync.py` verhindert Auseinanderlaufen

### 9.3 Entfernen

- 8684-Provider
- Sentry, GameAnalytics (bis G12)
- Upstream-Updater-URLs (bis G13)
- Fremde Discord-App-ID
- CEF/Browser, falls Windows kein Ziel ist

---

## 10. Public-Branch-Support

**Build-Registry:** `builds/public-<sha256>.json` mit

```json
{
  "name": "default-public-1.1.2.7",
  "engine": { "file": "hw.so", "sha256": "fca6628b...", "size": 11556210 },
  "client": { "file": "cstrike/cl_dlls/client.so", "sha256": "2e799ef3...", "size": 10649895 }
}
```

**Resolver-Reihenfolge:**

1. `dynsym` (schnell, exportiert)
2. `symtab` (statisch, unstripped)
3. Pattern-Scan (Fallback, wenn Valve künftig strippt)

**Offsets:** `EngineMsgBase` (`0x27D200` im 8684-Provider) wird für Public neu
bestimmt und im Build-Registry abgelegt, nicht im Code hartkodiert.

**ABI:** `goldsrc_abi` wird gegen Public validiert. DWARF-Info ist vorhanden und
erlaubt den Abgleich der Strukturgrößen.

---

## 11. Doku & Wiki

**Struktur DE/EN:**

```
docs/
├─ de/
│  ├─ index.md
│  ├─ credits.md
│  ├─ features.md
│  ├─ install/linux.md
│  ├─ build/prerequisites.md
│  ├─ build/client.md
│  ├─ build/server.md
│  ├─ reference/cvars.md        (generiert)
│  ├─ reference/commands.md     (generiert)
│  ├─ reference/natives.md      (generiert)
│  ├─ server/api.md
│  ├─ protocol.md
│  ├─ troubleshooting.md
│  ├─ faq.md
│  ├─ architecture.md
│  └─ privacy.md
└─ en/ (gleiche Struktur)
```

**Regeln:**

- `docs/` ist die einzige Quelle; GitHub-Wiki wird per CI gespiegelt.
- Generierte Tabellen werden nicht von Hand gepflegt.
- Upstream-Doku bleibt als technische Referenz im vendorten Baum, klar markiert.
- Jede Doku-Änderung ist Teil der Definition of Done.

---

## 12. Credits

`CREDITS.md` listet jedes Projekt mit Lizenz und Umfang:

- **Next21 Team** – NextClient, NextClientServerApi
- **Valve** – GoldSrc, HL1 SDK, Source SDK
- **SamVanheer** – GoldSrc-Reverse-Engineering (hl1_source_sdk)
- **tmp64** – hl1_source_sdk
- **KotyaraKryt** – Linux-Port, Discord, Commands, FastDL
- **mikkokko** – csldr
- **Nordic Warrior**, **fl0werD**, **lozatto**, **SanyaSho**, **MoeMod**, **Felipe** (MetaAudio)
- **FWGS / Velaron / a1batross / Vladislav4KZ / SNMetamorph** – cs16-client
- **FuryBaM** – cs16-goldsrc-client
- **portapps / crazy-max** – hlsw-portable
- **jeefo / YaPB**, **ReGameDLL_CS**
- alle vcpkg-Abhängigkeiten

Auch bei reiner Ideen-Übernahme wird credited.

---

## 13. Risiken

| Risiko | Wirkung | Gegenmaßnahme |
|---|---|---|
| Public-Binary wird künftig gestrippt | Resolver bricht | Dreistufiger Resolver + Build-Registry + Pattern-Fallback |
| Public-ABI weicht strukturell ab | Features defekt | G07e mit DWARF-Abgleich, früher Abbruch |
| Kein Legacy-Fallback | Kein Plan B bei G07-Scheitern | Bewusste Entscheidung; Neubewertung statt Hintertür |
| Arch ≠ Ubuntu 26.04 | vcpkg/Build bricht | G04-Spike, Patches, notfalls Container |
| HLDS/SteamCMD nicht verfügbar | Kein Testserver | Gepflegtes ReHLDS-Image |
| NextClient ohne Lizenz | Keine öffentlichen Binär-Releases | Privat bleiben, Notiz im Handoff |
| GPL-Referenzprojekte | Copyleft | Nur Ideen/Verfahren |
| VAC | Bann | Separate Kopie, kein VAC-Server, kein Steam-Ordner |
| Rebranding-Regression | Features brechen | Baseline-Tests vor/nach G08 |

---

## 14. Offene Punkte

1. **Discord-App**, **Sentry-Projekt**, **GameAnalytics-Keys**, **Update-Signaturkey** und Hosting werden für G12/G13 benötigt.
2. **Windows** ist kein Ziel. Entscheidung in G08: CEF/Browser-Code entfernen oder deaktiviert lassen.
3. **Lizenz** von NextClient: privat unkritisch; öffentliche Releases brauchen Freigabe.
4. **HLDS/SteamCMD**: Verfügbarkeit in G06 prüfen.
5. **Arch/vcpkg**: Host- oder Container-Build als Standard, Entscheidung in G04.

---

## 15. Backlog (nach 0.1.0)

- `csretro-server`: A2S-Query, RCON, Spieler/Bans, Logs, Map-Wechsel, portables Datenlayout (HLSW-Vorbild)
- Buy Presets (cs16-client-Vorbild)
- Erweitertes Crosshair `csr_crosshair_*`
- HUD-Farbe, Sway-Defaults
- Prediction-Übernahme (FuryBaM/cs16-client-Referenz)
- Spectator-/Scope-Politur
- Optional: Windows-Support

---

## 16. Erste Schritte für G01

1. `git init` im Projektordner, Remote setzen
2. `.gitignore`, `.gitattributes`, `.editorconfig` anlegen
3. `AGENTS.md` mit Abschnitt 5 schreiben
4. `HANDOFF.md`, `CHANGELOG.md`, `VERSION`, `versions.lock.json`
5. `README.md`, `README.de.md`, `CREDITS.md` (Gerüst)
6. `docs/de/index.md`, `docs/en/index.md`
7. `scripts/verify.sh`, `scripts/clean.sh`
8. `.github/workflows/ci.yml`
9. Commit `chore(repo): bootstrap harness and documentation skeleton`
10. Push, CI prüfen, Handoff aktualisieren

**Abnahme G01:** CI grün, `verify.sh` grün, Push erfolgt, Handoff aktuell.
