# CSRETRO – Gesamtplan

**Stand:** 9. November 2026
**Rolle:** maßgebliche Quelle für Ziele, Entscheidungen und Gates
**Fortschritt:** steht ausschließlich in [`HANDOFF.md`](../../HANDOFF.md), nicht hier
**Repository:** `https://github.com/benjarogit/csretro` (privat)
**Zielplattform:** Linux, Produktprozess x86_64 (Abschnitt 3.7)

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
| Multilib | `-m32` funktioniert | Server-Modul |
| 32-bit GL/X11/SDL2 | vorhanden | Server-Modul |
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

**Texturpfad im Spike nachgewiesen.** Die vier Textur-Slots des ausgelieferten
Stubs sind No-Ops: `draw_set_texture_file` (`0x12f0`),
`draw_set_texture_rgba` (`0x1300`), `draw_set_texture` (`0x1310`) und
`draw_textured_rect` (`0x1340`) sind jeweils ein einzelnes `ret`,
`create_new_texture_id` (`0x1360`) vergibt nur fortlaufende Zahlen und
`draw_get_texture_size` (`0x1320`) meldet konstant `16x16`. VGUI fragt pro
Paint acht Texturen an:

```text
gfx/vgui/round_corner_nw        gfx/vgui/round_corner_small_nw
gfx/vgui/round_corner_ne        gfx/vgui/round_corner_small_ne
gfx/vgui/round_corner_sw        gfx/vgui/round_corner_small_sw
gfx/vgui/round_corner_se        gfx/vgui/round_corner_small_se
```

Aufrufer ist `vgui2::Bitmap::Paint` (`Bitmap.cpp:134`): `DrawSetTexture`
(Vtable-Slot `0x68`) → `DrawSetColor` (Slot `0x20`) → `DrawTexturedRect`
(Slot `0x78`, additive Variante Slot `0x190`). Ein Preload-Shim im Spike-Baum
(`texture.c`, nicht im Repository) ersetzt die vier Slots und belegt, dass der
Weg TGA → GL-Textur → Textur-Quad → Bildpuffer vollständig trägt:

| Messung | Ergebnis |
|---|---|
| Laden pro Paint | 8 TGAs, IDs 11–14 `round_corner_{nw,ne,sw,se}` 64x64, IDs 16–19 `round_corner_small_*` 10x10 |
| `DrawTexturedRect` | achtmal `0,0 10,10`, Tint aus `DrawSetColor` (`255,255,255,128`, danach `188,112,0,128`) |
| Paint 40 vs. 41 | 27 Pixel in `x 0..9, y 0..9`, z. B. `(0,9)` `255,255,255` → `255,200,32` |
| Diagnose-Blitte `round_corner_nw` 256x256 | Alpha-Maske (Viertelkreis) korrekt gesampelt, transparente Fläche zeigt die Szene |
| `glGetError()` | 0 |

Beweise: `docs/evidence/g07-xash3d-vgui2-texture-corner.png` (+ `-zoom`) und
`docs/evidence/g07-xash3d-vgui2-texture-sampled.png` (+ `-zoom`).

Drei Engine-Anforderungen folgen daraus für den späteren Adapter:

1. **GL-Zustand retten.** Wer das Quad mit `glBindTexture(…, 0)` und
   `glDisable(GL_TEXTURE_2D)` beendet, zerstört den Engine-Zustand: Xash3Ds
   eigenes 2D-Zeichnen rendert danach untexturierte weiße Quads über den
   ganzen Frame (weißer Bildschirm). Der Shim sichert und restauriert daher
   `GL_TEXTURE_2D`-Enable, Bindung, Blend, Depth, Cull und `glColor`.
2. **Eigene Projektion und eigener Viewport.** Der Engine-Zustand im HUD-Paint
   hat `Viewport 1280x1024`, die Stub-Koordinaten sind aber `1024x768`. Das
   Quad braucht `glOrtho(0,1024,768,0)` und `glViewport(0,0,1024,768)` mit
   Push/Pop der Matrizen.
3. **Verzögertes Zeichnen.** `DrawTexturedRect` sammelt nur ein; geflusht wird
   nach `stub_surface_paint`, sonst übermalt der Rest des Panels die Quads.

Offen bleibt die Übersetzung: alle acht `DrawTexturedRect`-Aufrufe kommen mit
`0,0 10,10` an, obwohl die vier Ecken an verschiedenen Stellen liegen. VGUI
zeichnet Bitmaps in **elternrelativen** Koordinaten und erwartet die
Verschiebung aus `PushMakeCurrent` (Stub `0x1de0`) beziehungsweise
`PopMakeCurrent` (`0x11b0`); beide sind im Stub No-Ops. Die korrekte
`GetAbsPos`-Quelle ist IPanel-Slot **11** (`VPanelWrapper::GetAbsPos`,
`_ZTV13VPanelWrapper` bei `vgui2.so 0x2a0b0`); Slot 13 ist `SetInset` und
zerstört das Layout. Ohne Offset landen alle Bitmaps auf `0,0`. Der Offset ist
im Spike absichtlich noch nicht aktiv, damit Fills und Textur-Quads dieselbe
lokale Basis haben; er gehört in den Adapter.

Offener Rest daraus: der Push/Pop-Offset. Fonts sind separat in 3.6.4 gemessen
(`DrawPrintText` nutzt bereits `CL_DrawString`, aber ohne echte Metrik).

Nebenbefunde:

- `engine_mini` reicht Engine-Aufrufe über `nitro_api` weiter. Der Produktprozess
  öffnet, hasht und hookt `hw.so` nicht (Abschnitt 3.7). Die Aufrufe haben noch
  keinen Xash3D-Einsprung; das ist der nächste Adapter.
- HTML/MOTD verlangt einen ChromeHTML-Controller/Serializer. Ohne CEF wird MOTD
  in CSRETRO deaktiviert oder durch eine einfache Text-Ansicht ersetzt.
- Ein x86_64-Client setzt quelloffenen 64-Bit-Client-Gamecode voraus. Valves
  `client.so` ist ausschließlich 32 Bit.

#### 3.6.4 Nachtrag 11. November 2026: Fonts kollabieren auf eine Engine-Schrift

Ein Preload-Shim (`fontprobe.c`, Spike-Baum, nicht im Repository) protokolliert
die fünf Font-Slots des Stubs in einem 14-Sekunden-Lauf
(`+map de_dust2 +showscores`, 1024x768):

| Slot | Aufrufe | Ergebnis |
|---|---|---|
| `CreateFont` (`0x14b0`) | 36 | immer `1` |
| `AddGlyphSetToFont` (`0x14c0`) | 54 | immer `1`, immer `font=1` |
| `AddCustomFontFile` (`0x14d0`) | 0 | – (kein `CustomFontFiles`-Key in `ClientScheme.res`) |
| `GetFontTall` (`0x14e0`) | 42 089 | immer `16` |
| `GetFontAscent` (`0x1680`) | 0 | – (VGUI2 fragt nur `GetFontTall`) |

Die 54 `AddGlyphSetToFont`-Aufrufe verlangen 27 verschiedene Kombinationen aus
Name, Höhe und Gewicht (Format `Name Höhe/Gewicht`; die Höhen 42–55 sind
bereits mit dem Scheme-Faktor skaliert):

```text
Verdana       12/0     20/0     20/600
Verdana Bold  18/500   38/500
Tahoma        12/500   25/500
Tahoma        20/800   22/800   24/800  26/800
Tahoma        42/800   46/800   51/800  55/800
Arial         13/600   16/600   27/600  34/600
Arial         20/700   22/700   24/700  26/700
Arial         42/700   46/700   51/700  55/700
Marlett       11/0     23/0
```

Alle 36 Handles sind dieselbe Zahl `1`, alle 54 Glyph-Sets landen auf diesem
einen Handle, und alle 42 089 `GetFontTall`-Aufrufe liefern `16`. Der Stub
berechnet diese 16 einmalig in `draw_set_text_font` (`0x11c0`): Er misst den
Literal-String `"Wg."` (`.rodata 0x3000`) über die Engine-Funktion
`DrawConsoleStringLen` (`funcs+0x74`) und speichert das Ergebnis als globale
Höhe (`stub+0x5ff4+0x3c`). `get_text_size` (`0x2510`) und
`get_character_width` (`0x2480`) messen denselben String ebenfalls mit
`DrawConsoleStringLen`, gezeichnet wird über `pfnDrawString`
(`funcs+0x1c8`).

damit ist die Font-Lücke präzise: Nicht der Textpfad fehlt – VGUI2-Text
erscheint pro Frame –, sondern **jede Schriftmetrik kollabiert auf die eine
Konsolenschrift des jeweiligen Xash3D-Builds**. Ein `VGUI_Surface026`-Adapter
braucht deshalb einen eigenen TTF-Rasterizer plus Glyph-Atlas und echte
Metriken pro Handle (Höhe, Ascent, ABC-Breiten), statt die Engine-Font-API
weiterzureichen. Das ist Adapter-Arbeit, kein Stub-Patch: Der Stub hat für
Fonts keine Datenstruktur, an der ein Patch ansetzen könnte.

#### 3.6.5 Nachtrag 11. November 2026: Layout, Push/Pop und Bildschirmgröße

Der Spike zeichnet das Scoreboard jetzt an seiner echten Position. Drei
Befunde waren dafür nötig:

1. **`PushMakeCurrent` liefert den Offset über `VPanel`, nicht über
   `VPanelWrapper`.** Das Panel, das VGUI an `PushMakeCurrent` übergibt, ist
   ein `vgui2::VPanel`. In `_ZTVN5vgui26VPanelE` (`vgui2.so 0x29fb0`,
   Adresspunkt `0x29fb8`) ist `VPanel::GetAbsPos(int&, int&)` (`0x17200`)
   Slot **19**. Slot 11 ist dort nicht `GetAbsPos`; die frühere Angabe
   „IPanel-Slot 11“ gilt nur für die Engine-Schnittstelle
   `VPanelWrapper` (`_ZTV13VPanelWrapper` bei `0x2a0b0`).
2. **Die Übersetzung muss alle drei Primitive erfassen.** Fills laufen über
   `funcs+0x2c`, Text über `funcs+0x1c8`, Texturen über die Quads. Ein
   Shim, der nur die Texturen verschiebt, reißt das Layout auseinander.
   Der Spike kopiert daher die Engine-Funcs-Tabelle, ersetzt die beiden
   Slots durch Wrapper, die `x`/`y` um den aktuellen Push-Offset erhöhen,
   und setzt die Kopie in `stub+0x5ff4+0x3fc` ein. `PopMakeCurrent` stellt
   den Offset wieder her.
3. **Die Engine-Bildschirmgröße muss zur Framebuffer-Größe passen.** Der
   Stub fragt sie über `funcs+0x30` ab (`get_screen_size` ist eine virtuelle
   Methode mit `this` plus zwei `int*`). Läuft der X-Server mit 1280x1024,
   während das Spiel 1024x768 rendert, meldet VGUI 1280x1024 und legt das
   Dialogfenster für diese Fläche aus: gemessen `1098x714` bei `85,149`,
   die Ecken bei `1183,863` – also außerhalb des 1024x768-Buffers. Mit
   einem Xvfb passend zur Spielauflösung (1024x768) liefert
   `get_screen_size` `1024x768`, und das Dialogfenster sitzt bei `96,84`.

Messung im Spike (Paint 40 vs. 41, 1024x768):

| Größe | Wert |
|---|---|
| Engine-Bildschirmgröße | `1024x768` |
| Dialog-Offset (`GetAbsPos`) | `96,84` |
| Textur-Ecken | `96,84`, `917,84`, `96,673`, `917,673` |
| Textur-Diff zwischen zwei Paints | 108 Pixel im Rechteck `96..926, 84..682` |

Sichtbar sind damit der Panel-Hintergrund `832x600` an `96,84`, die vier
gerundeten Ecken, der weiße `ServerName`-Balken und der rechtsbündige
Kopftext `Score`, `Deaths`, `Latency`. Beweise:
`docs/evidence/g07-xash3d-vgui2-laid-out.png` (ganzer Frame),
`-header.png` (Kopfzeile) und `-corner.png` (Ecke).

Für den Adapter folgt daraus: Push/Pop muss den Offset für **jedes**
Primitive anwenden, und die Bildschirmgröße muss aus derselben Quelle kommen
wie die Framebuffer-Größe, oder die Differenz muss explizit skaliert werden.
Beides ist Adapter-Arbeit; der Stub selbst kennt weder Push-Offset noch
Skalierung.

#### 3.6.6 Nachtrag 11. November 2026: Font-Backend (Handles, Metriken, Glyphen)

Der Stub patcht seine fünf Font-Slots erst beim ersten Paint – zu spät: VGUI
legt die Scheme-Fonts schon in `HUD_Init` an, und ein Lauf mit spätem Patch
sah **null** `CreateFont`-Aufrufe. Der Spike patcht die Slots deshalb in einem
Polling-Thread, sobald `stub_surface.so` gemappt ist (dieselbe Technik wie
`fontprobe.c`; KVShim exportiert ein eigenes `dlsym`, `RTLD_NEXT` ist hier
nicht sicher).

Damit gilt im Lauf (`+map de_dust2 +showscores`, 1024x768):

| Slot | Aufrufe | Ergebnis |
|---|---|---|
| `CreateFont` (`0x14b0`) | 36 | Handles `1`…`36` |
| `AddGlyphSetToFont` (`0x14c0`) | 54 | 36 primäre Sets + 18 Charset-Fallbacks |
| `GetFontTall` (`0x14e0`) | 1024 (Paint 41) | je Handle die angeforderte Höhe |
| `GetCharABCwide` (`0x1500`) | 1471 (nur Paint 1) | FreeType-Advance, danach gecacht |
| `GetCharacterWidth` (`0x2480`) | 4611 (Paint 41) | FreeType-Advance pro Zeichen |
| `GetTextSize` (`0x2510`) | 0 | – (VGUI2 ruft es nicht auf) |
| `GetFontAscent` (`0x1680`) | 0 | – (VGUI2 ruft es nicht auf) |

**Erster Glyph-Set gewinnt.** VGUI hängt an denselben Handle einen zweiten
Glyph-Set für einen anderen Zeichenbereich (`Tahoma 26/800`, dann
`Arial 26/700` – gleiche Höhe, andere Familie). Der erste Set definiert
Familie und Höhe, die weiteren nur die Abdeckung. Die 36 primären Sets
verlangen 15 verschiedene Höhen (`11, 12, 14, 17, 18, 19, 20, 22, 24, 26, 28,
32, 35, 38, 41`), vorher lieferten alle 42 089 `GetFontTall`-Aufrufe `16`.

**Schriftdateien fehlen unter Linux.** Das Retail-Scheme nennt Windows-Fonts;
im Spike-Baum liegt nur `valve/resource/marlett.ttf` (Retail-Asset). Der Spike
löst deshalb auf:

| Angefragt | Datei |
|---|---|
| `Marlett` | `valve/resource/marlett.ttf` (echter Retail-Font) |
| `Arial` | `LiberationSans-{Regular,Bold}.ttf` (metrikkompatibel) |
| `Verdana`, `Tahoma` | `DejaVuSans{,-Bold}.ttf` |

**Zeichnen.** `text_at.isra.0` macht aus dem UTF-32-Text einen Byte-String
(alles außerhalb `0x20..0x7f` wird `?`) und ruft `funcs+0x1c8`. Der Shim
ersetzt diesen Slot: pro Zeichen `FT_Load_Char(..., FT_LOAD_RENDER)`, daraus
eine GL-Textur (weißes RGB, Coverage im Alpha), Baseline `y + ascent`, ein
Quad pro Glyph in die verzögerte Queue. Messung: 50 Textaufrufe und 42
Glyph-Quads pro Paint, 21 Glyph-Texturen insgesamt, **0** Engine-Fallbacks –
die sichtbaren Buchstaben kommen vollständig aus dem Backend. Ein Lauf mit
`csretro_face=DejaVuSerif-Bold.ttf` zeigt die Kopfzeile in Serifen und belegt,
dass die Glyphen der konfigurierten Datei folgen.

Beweise: `docs/evidence/g07-xash3d-vgui2-fonts.png` (Frame),
`-header.png` (Kopfzeile) und `-serif.png` (Serifen-Diagnose).

Für den Adapter folgt daraus:

1. **Eigener Rasterizer plus Glyph-Atlas.** Der Stub hat keine
   Font-Datenstruktur; das Spike-Backend nutzt eine GL-Textur pro Glyph
   (21 Texturen für den Scoreboard), der Adapter braucht einen gepackten
   Atlas mit UV-Rechtecken.
2. **Handle pro `CreateFont`, erster Glyph-Set primär**, Metriken ausschließlich
   aus FreeType: Höhe, Ascent, Advance. `GetTextSize` und `GetFontAscent`
   werden von VGUI2 nicht benutzt und dürfen keine Layoutquelle sein.
3. **Fonts müssen mitgeliefert werden.** Linux hat weder Verdana noch Tahoma
   noch Arial; Retail liefert nur `marlett.ttf`. Ersatzschriften ändern
   Glyphenformen und Breiten, also auch Textlängen und damit Layouts – das ist
   eine dokumentierte Abweichung zum Retail-Client, keine Regression.
4. **Text in Baumreihenfolge zeichnen.** Der Spike schiebt Glyph-Quads wie die
   Texturen hinter den Paint; damit liegt Text über später gezeichneten
   Kind-Hintergründen. Der Adapter zeichnet synchron im Traverse.

#### 3.6.7 Nachtrag 11. November 2026: Eingabe, Hover und Klick

Bisher war nur der Zeichenweg belegt. Geprüft wurde jetzt, ob der Stub auch
**Eingabe** an VGUI2 weiterreicht – ohne echten Mauszeiger und ohne
Fensterfokus, im Xvfb-Lauf.

**Aufbau.** VGUI2 hält `CInputWin32` als Singleton in `.bss`
(`vgui2.so+0x34420`); KVShim liefert die Adresse als `VGUI_Input004`. Der
Spike ruft die Methoden direkt auf (i386-cdecl, Objektzeiger als erstes
Argument) und schiebt die Nachrichten danach durch `CVGui`:

| Schritt | Symbol | Adresse |
|---|---|---|
| Cursor | `CInputWin32::InternalCursorMoved(int,int)` | `vgui2+0x1dc00` |
| Druck | `CInputWin32::InternalMousePressed(MouseCode)` | `vgui2+0x1e8a0` |
| Loslassen | `CInputWin32::InternalMouseReleased(MouseCode)` | `vgui2+0x1e200` |
| Zustellung | `CVGui::DispatchMessages()` | `vgui2+0x1b120`, Singleton `vgui2+0x34340` |
| Abfrage | `CInputWin32::GetMouseOver()` | `vgui2+0x1ce60` |

Zwei Fallen:

1. **`CVGui::RunFrame` ist unbrauchbar.** Es ruft nach `surface()->RunFrame()`
   noch `UpdateMouseFocus(GetCursorPosition())`. Der Stub meldet als
   Cursorposition `0,0`, also würde jeder Frame den Fokus auf die linke obere
   Ecke zurücksetzen. Der Spike ruft deshalb nur die drei Input-Methoden und
   `DispatchMessages`.
2. **`UpdateMouseFocus` braucht ein Embedded-Panel.** Ohne `_rootPanel`
   (`g_Input+0x1b4`, im Spike `NULL`) fällt `CInputWin32::UpdateMouseFocus`
   auf `surface()->GetEmbeddedPanel()` zurück und ruft dort
   `Client()->IsWithinTraverse(x,y,false)`. Der Spike setzt das Embedded-Panel
   deshalb auf die TeamMenu-Wurzel (`stub+0x1cc0`); `IsWithinTraverse` ist
   Slot 44 (`vptr+0xb0`) des `IPanel`-Wrappers, `GetEmbeddedPanel` liefert
   sonst `NULL` und der Fokus bleibt leer.

**Messung.** Lauf mit `chooseteam` bei Paint 20, Cursor `239,201` bei Paint
29, Druck und Loslassen bei Paint 30, `+map de_dust2`, 1024x768:

| Prüfung | Aufruf | Ergebnis |
|---|---|---|
| Treffer | `IsWithinTraverse(239,201)` | `Button/terbutton` |
| Hover | `InternalCursorMoved` + `DispatchMessages` | `GetMouseOver()` = `Button/terbutton` |
| Klick | `InternalMousePressed`/`Released` + `DispatchMessages` | `SetVisible(0)` auf `CTeamMenu` beim Release |
| Teamwechsel | `g_iTeamNumber` (`client.so+0x1e54ec`) | Paint 30 `0`, Paint 31 **`1`** (Terrorist) |
| Negativkontrolle | Klick auf `700,600` (`RichText/MapInfo`) | kein `SetVisible`, `g_iTeamNumber` bleibt `0` |

Damit ist die Kette Maus → `CInputWin32` → `PostMessage` →
`CVGui::DispatchMessages` → `Button` → `CTeamMenu`-Kommando → Teamwechsel
belegt. Die Negativkontrolle schließt aus, dass irgendein Druck das Menü
schließt: derselbe Ablauf auf einem Panel ohne Kommando ändert nichts. Der
Teamwechsel erscheint erst im Frame nach dem Klick, weil er über den Server
zurückkommt.

**Bilder.** `docs/evidence/g07-xash3d-vgui2-team-menu.png` (Paint 29, Menü mit
`SELECT TEAM`), `-team-menu-header.png` (Ausschnitt) und `-team-click.png`
(Paint 30, Menü weg, Scoreboard/HUD sichtbar). Zwischen beiden Frames ändern
sich 617 714 Pixel im Rechteck `121,0..1023,715`; das Menü bedeckte also
genau diesen Bereich.

**Grenzen des Spikes.** Diese Punkte sind Adapter-Aufgaben, keine
VGUI2-Befunde:

1. **Kein Rollover-Bild.** `Button::OnCursorEntered` setzt `armed`, die
   Darstellung stammt aber aus Scheme-Texturen (`button/rollover`), die der
   Stub nicht lädt. Ein Lauf ohne Klick, aber mit `InternalCursorMoved`,
   ändert zwischen Paint 29 und 30 nur **99 Pixel** – Hover ist deshalb nur
   semantisch über `GetMouseOver` belegt, nicht visuell.
2. **Kein Mauszeiger.** Der Stub zeichnet keinen Cursor; Retail nutzt dafür
   eine Cursor-Textur über `VGUI_Surface026`. Die Position ist nur über
   `GetCursorPos`/`GetMouseOver` prüfbar.
3. **Trefferfläche ≠ gerenderte Fläche.** Der Spike-Offset verschiebt
   Füllungen und Text um wenige Pixel gegenüber den gesolveten
   Panel-Rechtecken; `239,201` liegt laut Hit-Test im `terbutton`, optisch
   aber knapp über dessen Kante. Ursache ist der getrennte Offset-Pfad, nicht
   VGUI2. Der Adapter braucht **eine** Transformation für Hit-Test und Paint.
4. **Der Scoreboard-Pfad malt immer.** Der Stub zeichnet sein Scoreboard
   unabhängig von der Sichtbarkeit, deshalb liegt es in den Team-Menu-Bildern
   als Hintergrund. Der Adapter malt nur sichtbare Bäume.

**Für den Adapter folgt daraus:**

1. **Eingabe einspeisen, nicht nachbauen.** Xash3D-Eingabe wird zu
   `InternalCursorMoved`/`InternalMousePressed`/`InternalMouseReleased` und
   danach `CVGui::DispatchMessages`; kein eigener Fokus-Code, kein
   `RunFrame`-Fokus-Reset.
2. **Embedded-Panel ist die VGUI2-Wurzel**, nicht das jeweilige Fenster. Der
   Spike setzt die TeamMenu-Wurzel nur, weil die VGUI2-Wurzel im Stub fehlt.
3. **Rollover und Cursor brauchen Scheme-Texturen.** Ohne
   `ClientScheme.res`-Materialien bleibt Hover unsichtbar und der Zeiger
   fehlt; beides gehört in den Adapter, nicht in den Spike.
4. **Hit-Test und Paint teilen die Transformation.** Der Spike belegt, dass
   getrennte Offset-Pfade zu Klicks neben dem sichtbaren Ziel führen.

---

### 3.7 Surface-Adapter (Slice A, 11. November 2026)

Der erste Produkt-Slice ist **A, Surface-Adapter**. Ein `nitro_api`-Provider
(Slice B) oder ein Launcher (Slice D) liefern ohne diese Surface kein Bild:
`engine_mini` erzeugt auf Linux kein `VGUI_Surface026`, und `vgui2` zeichnet
nicht selbst.

**Host-Naht.** Die Surface wird **nicht** in `vgui2.so` eingebunden.
`VGui_InternalLoadInterfaces` holt `VGUI_Surface026` aus der Factory-Liste des
Hosts und legt sie in `g_pSurface`; `SurfaceNext` reicht jeden Aufruf dorthin
weiter. Eine zweite Surface in der Control-Bibliothek würde diese eine
Identität aufspalten. Xash3D bleibt unverändert (GPL, nicht vendoren). Der
CSRETRO-Host `xash_host` lädt `xash_surface.so` mit `RTLD_LOCAL` und stellt
die Factory-Liste bereit. Der erste Eintrag beantwortet `VGUI_Surface026`.
Mausbewegung, Druck und Loslassen gehen über Funktionszeiger an
`CInputWin32`, danach genau eine Zustellung über
`csr_vgui_dispatch_messages`. `CVGui::RunFrame` wird nicht gerufen.
`LD_PRELOAD` bleibt ein Spike-Werkzeug. Die Retail-Bibliothek exportiert
`DispatchMessages` nicht. Die aus dem SDK gebaute `vgui2` exportiert den
C-Einstieg. Aufgerufen wird er nur über diesen Zeiger oder die
C++-Schnittstelle. Die erste Factory, die `VGUI_Surface026` liefert, wird
`g_pSurface`.

**Prozess.** Der Produktprozess ist x86_64. Valves `client.so`, `vgui2.so`
und `hw.so` sind i386 und werden nicht in diesen Prozess geladen. Client und
`vgui2` kommen aus dem vendorten SDK, die Engine ist Xash3D. Der Client-Build
ist das Preset `linux` mit dem vcpkg-Triplet `x64-linux`. Es gibt kein
zweites 32-bit-Client-Preset. Das Server-Modul bleibt i386, weil ReHLDS es
so lädt. Es gibt keine zweite Surface.

**Engine-Identität.** Der Produktprozess ist Xash3D, nicht der Public-Linux-SHA
von `hw.so`. `NitroApi` öffnet, hasht und hookt diese Datei nicht. Es gibt
einen Provider, `EngineAddressProviderXash`, und keine zweite Kopie der
Public-Linux-Tabelle. Fehlende Adressen sind keine Hooks. Ein Aufruf springt
nicht durch einen Null-Trampolin. `GenericHookJit` bleibt i386 und läuft in
diesem Prozess nicht. Die Retail-Symboltabelle bleibt die Quelle für die
Offline-Prüfung und ist nicht der Produktpfad.

**Engine-Adapter.** Ein Adapter, `xash_engine`, bedient die Weiterleitung von
`vgui2_surface`, `VGUI2_Draw_Init`, `VGUI2_ResetCurrentTexture` und
`VGUI2_Draw_SetTextColor`. Liegt ein Retail-Hook vor, bleibt dieser Aufruf.
Sonst ruft der Adapter nur gesetzte Funktionszeiger der einen Surface.
`VGUI2_Draw_Init` hat an dieser Surface keinen eigenen Einstieg und ist dann
kein Aufruf. Textfarbe geht mit VGUI-Alpha 0 an `DrawSetTextColor`. Ein
fehlender Zeiger wird nicht aufgerufen. Die übrigen Weiterleitungen von
`engine_mini` bleiben unverändert.

**Client-Paar.** `client_mini` und `vgui2` werden aus derselben vendorten
SDK-Revision als ELF64 gebaut. Beide linken die Host-Laufzeit
`libtier0.so`, `libvstdlib.so` und `libsteam_api.so`. Valves i386-Bibliotheken
werden nicht gelinkt. Beide tragen `VGUI_Scheme009`. Die Surface bleibt die
eine Host-Factory. Der Linux-Patch, der Valves `client.so` umschreibt, läuft
in diesem Prozess nicht.

**Host lädt das Paar.** `xash_host` öffnet `client_mini` und `vgui2`, nachdem
die Surface-Factory steht. Er liest `csr_vgui_dispatch_messages` und
`VGUI_Scheme009`. Fehlt eine Bibliothek oder ein Export, ist das ein
Fehlschlag und kein Aufruf. Der Dispatch-Zeiger wird nicht gerufen.
`CVGui::RunFrame` bleibt ungenutzt. Es gibt keine zweite Surface und keine
zweite Factory. Der Host startet das Spiel nicht.

**GameUI.** `gameui` kommt aus derselben SDK-Revision und ist ELF64. Es linkt
`libtier0.so`, `libvstdlib.so` und `libsteam_api.so`. Es trägt `GameUI007`
und `GameConsole003` und keine eigene `VGUI_Surface026`. Die Surface bleibt
die eine Host-Factory. Der Host lädt `gameui`, nachdem die Surface-Factory
steht. Er liest
`GameUI007` und `GameConsole003` und ruft die Spieloberfläche nicht auf.
Eine fehlende Bibliothek oder ein fehlender Export ist kein Aufruf. Die
Surface bleibt die einzige Factory. Das Spiel startet er nicht.

**Dateisystem.** Dieselbe Factory-Liste beantwortet `VFileSystem009`, sobald
der Host eine Wurzel gemountet hat. Die Surface bleibt der erste Eintrag.
Der Proxy ist eigener Code und öffnet `hw.so` nicht. Er liest nur Dateien
unter der übergebenen Wurzel. Eine fehlende Datei oder ein Pfad außerhalb
dieser Wurzel ist ein Fehltreffer und kein erfundener Pfad. Es gibt kein
zweites Dateisystem und keine zweite Factory. Retail-Daten liegen nicht im
Repository. Der Host startet das Spiel nicht.

**Ohne Steam-Prozess.** Der portierte Client startet keinen Steam-Client.
Voraussetzung bleiben die Originaldaten des Besitzers: Maps, Modelle, Sounds
und `valve/resource` (darunter Marlett). Sie liegen nicht im Repository. Der
bisherige GoldSrc-Starter über die Steam-Runtime gilt nur, bis dieser Host
ihn ersetzt.

**Bildrate.** 60 fps ist die Untergrenze, keine Obergrenze. Counter-Strike 1.6
lief darüber. Kein 60-Hz-VSync-Lock und kein zweites fps-Cvar.

**Spieleridentität.** Ein Slot, `ResolvePlayerIdentity`. Er bekommt den
bestehenden Spielnamen aus dem `name`-Setinfo und optional eine Persona.
Ist die Persona eingeloggt und hat einen Namen, ist das der angezeigte
Name. Ein Portrait gibt es nur, wenn dieselben Angaben bereits Pixel
enthalten. Sonst bleibt der Spielname, und es gibt kein Portrait. Die
Funktion lädt nichts herunter, speichert nichts und startet Steam nicht.
Die Retail-`steam_api` der Installation ist i386 und wird in den
x86_64-Prozess nicht geladen. Es gibt kein zweites Namens-Cvar und keinen
zweiten Avatar-Cache.

Der erste Stand liegt in `client/nextclient/xash_surface/` (Target
`xash_surface`, Artefakt `xash_surface.so`):

- Die Klasse beantwortet `VGUI_Surface026` vollständig. `CreateInterface`
  liefert nur die lebende Instanz aus `csr_surface_create`.
- `PushMakeCurrent` setzt den Ursprung auf die absolute Panelposition des
  Hosts, optional plus Inset, und schneidet an das absolute Clip-Rechteck.
  Die Abbildung inklusive UV-Kürzung ist engine-frei getestet.
- Füllungen, Textur-Quads und Glyphen zeichnen sofort in Aufrufreihenfolge,
  mit eigener Ortho-Projektion. VGUI-Alpha 0 ist opak und wird erst am
  GL-Aufruf invertiert. Jeder Draw stellt den GL-Zustand wieder her,
  einschließlich `GL_TEXTURE_2D` und der Texturbindung.
- Glyphen kommen aus FreeType in einen gemeinsamen Atlas. Der erste
  Glyph-Set legt Höhe und Schnitt fest, weitere Sets nur die Abdeckung.
  `GetFontTall` bleibt die angeforderte Höhe. Laufweite und Ascent kommen
  aus der Datei. Arial wird als Liberation Sans mitgeliefert, Verdana und
  Tahoma als DejaVu Sans. Das ist die in 3.6.6 beschriebene Abweichung zum
  Retail-Client. Marlett wird nicht mitgeliefert; der Host gibt den Pfad
  über `resolve_font`. Unschärfe, Scanlines und Umriss fehlen noch.
  `SolveTraverse` läuft den sichtbaren Baum über `csr_surface_panel_fns_t`.
  `PaintTraverse` ruft die Panel-Paint-Methode mit Repaint und Allow-Force
  auf; den Kind-Durchlauf macht VGUI2. Popups schließt
  `ShouldPaintChildPanel` aus, die Surface malt sie danach. Mauseingabe
  geht über `csr_surface_input_fns_t` an den Host: Cursor, Druck oder
  Loslassen, danach eine Zustellung. `RunFrame` bleibt leer, damit
  `UpdateMouseFocus` den Fokus nicht auf eine gespeicherte `0,0`-Position
  zurücksetzt. Der Host bildet die drei Aufrufe auf `CInputWin32` und
  `CVGui::DispatchMessages` ab.
- Tests: `xash_surface-tests` über das Preset `linux-tests`. Der
  Offscreen-Lauf nutzt einen EGL-Pbuffer und `LIBGL_ALWAYS_SOFTWARE=1`.
  Das Spiel wird dabei nicht gestartet.

Der Spike mit dem Stub bleibt der Beweis für den echten VGUI2-Baum. Die
Bibliothek hängt noch nicht an Xash3D.

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
- Client-Starter folgt Abschnitt 3.7: kein Steam-Prozess, Originaldaten
  erforderlich, fehlende Kopie endet mit einer klaren Meldung
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
