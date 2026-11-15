# Client bauen

Baue die vorhandenen NextClient-Komponenten in der für die Aufgabe benötigten Architektur. Der erste Port läuft auf Xash3D mit passenden lokal bereitgestellten Original-Spielbibliotheken; ausschließliches x64-Gameplay folgt danach. Die Reihenfolge steht in [Plan, Abschnitt 3.8](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md). Ein erfolgreicher Komponentenbau belegt keinen spielbaren Client. Diese Befehle starten kein Spiel und ändern die Steam-Installation nicht.

Voraussetzungen: [Build-Voraussetzungen](prerequisites.md).

## Profil wählen

- `linux` (Standard): x64-Komponenten, vcpkg `x64-linux`, Host-Compilerflags.
- `linux-port`: i386-Komponenten für den ersten Port, vcpkg `x86-linux`, `-m32` für C, C++ und Assembler. Benötigt passende 32-Bit-OpenGL- und FreeType-Bibliotheken; unter Arch liegen diese in `/usr/lib32`.
- `linux-tests` und `linux-port-tests`: passende Profile mit Testtargets. Testkonfigurationen erzeugen keine auslieferbare Engine-Bibliothek; zum Bereitstellen das Profil ohne Tests verwenden.

Alle Profile verwenden Ninja Multi-Config und getrennte Verzeichnisse unter `client/build/<preset>`. Shared-Artefakte liegen dort unter `out/bin/<config>/`. Binärdateien verschiedener Architekturen oder Konfigurationen nicht mischen.

## Komponenten bauen

```bash
scripts/doctor.sh
scripts/bootstrap.sh
scripts/build-client.sh --target xash_surface
scripts/build-client.sh --preset linux-port \
  --target client_mini --target nitro_api --target engine_mini --target gameui
```

Wiederholte `--target`-Optionen bauen diese Targets nacheinander. Ohne sie baut das Skript den vorhandenen Auslieferungssatz einschließlich Launcher und `BUILD_ALL`. Ein Installationsverzeichnis fordert die bestehende Installationsroutine an; sie stellt nicht automatisch eine funktionierende Xash3D-Laufzeit zusammen.

Weitere Optionen: `--config Debug`, `--jobs 8`, `--reconfigure`, `--clean`, `--install-dir <dir>`, `--manifest <file>`. Standardkonfiguration ist Release. Nach Presetänderungen oder neuen per CMake-Glob erfassten Quellen erneut konfigurieren.

Das Manifest enthält Version, Quellcommit, Preset, Konfiguration, Buildzeit, Größe, SHA-256, ELF-Klasse und relativen Artefaktpfad. CMake leitet die erwartete ELF-Klasse aus der Compiler-ABI ab. Leere Shared-Ausgabe oder eine abweichende Klasse lassen den Build scheitern. Das Manifest erfasst das Ausgabeverzeichnis; es bestätigt weder Callbacks noch Movement, Leistung oder Spielbarkeit.

## Komponenten ohne Spielstart prüfen

Nach dem Konfigurieren aus einem Client-Quellpfad ohne Leerzeichen:

```bash
cmake --preset linux-port-tests
cmake --build --preset linux-port-tests-release --target xash_surface-tests
LIBGL_ALWAYS_SOFTWARE=1 build/linux-port-tests/out/bin/Release/xash_surface-tests
cmake --build --preset linux-port-release --target verify-nitro-api-factory goldsrc_abi_golden
```

Die Surface-Tests verwenden einen eigenen Offscreen-Kontext. Der Factory-Probe lädt unseren Provider, das Golden-Target kompiliert ABI-Assertions. Keiner davon startet ein Spiel. Die Buildskript-Regressionen laufen mit `python3 -m unittest discover -s tests/build_scripts -v` aus dem Repository-Hauptverzeichnis und auch in CI. Spielnachweise folgen der [Laufzeit-Abnahme](../maintainers/runtime-readiness.md).

## Original-Client-Brücke

Mit `scripts/build-client.sh --preset linux-port --target xash_client` bauen. Die Ausgabe `csretro_client.so` stellt den ursprünglichen Callback-Tabelleneinstieg bereit. Sie ist für den Spielclientpfad der Engine vorgesehen, neben der lokal bereitgestellten und in `client.valve.so` umbenannten Originalbibliothek. Laufzeitbereitstellung und Installer sind noch nicht verbunden. Keine Dateien in Steam ersetzen oder diesen unvollständigen Komponentensatz als fertigen Port starten.

Beim Tabellenabruf prüft die Brücke i386-ELF und den Spielhash aus der gemeinsamen Default-Public-Registry vor dem Laden. Movement, Vorhersage und CreateMove bleiben Originalcallbacks; der freigegebene Tastaturwrapper reicht nicht konsumierte Ereignisse mit unveränderten Argumenten und Rückgaben weiter. Wrapper für Initialisierung/Shutdown, HUD-Frame/Redraw sowie Mausereignis/Zurücksetzen/Deaktivieren rufen das Original auf und verbinden UI-Lebensdauer und UI-Arbeit. Die Brücke prüft die passende `vgui2.valve.so` daneben und lädt `xash_surface.so` mit ihrem mitgelieferten Ordner `fonts`. Benannte Originalmethoden liefern Paneltraversierung, Layout, Paint, Popup-Flags und Mauseingabe. Die echte Engine liefert Bildschirm-/Mausposition, Dateilesen und Marlett-Suche. SDL2 wird aus dem bereits geladenen Client genutzt; relativer Mausweg wird nur bei sichtbarer UI verbraucht. Vor jedem originalen VGUI-Schedulerframe wird der Cursor synchronisiert. Der originale Viewport erhält vier aufrufbare Factory-Einträge, weil sein nativer Initialisierer alle vier liest und die eigene Factory voranstellt; eine generische Zweierliste reicht nicht. Die Brücke bindet originale VGUI-Interfaces, initialisiert den Viewport und startet ihn in getrennten geprüften Phasen. Frame, Paint und Input werden erst nach abgeschlossenem Start freigegeben. Originale GameUI-Initialisierung und -Start belegen jetzt diese geprüften Positionen; Abbau aus Zwischenzuständen stellt Globals und exklusive Bindung nach Freigabe nativer Leases wieder her. Der native Videodaten-Provider benötigt jetzt echte Xash-Renderer-/Mount-Cvars und ein vorhandenes SDL2-GL-Fenster. Surface-Größe und Mausgrenzen nutzen Engine-Dimensionen sowie tatsächliche SDL-Pixelformate/Display-Modi statt Testhost-Vorgaben. Fehlende Daten lassen die Initialisierung scheitern; der Provider startet weder Fenster noch Spiel. Tatsächliche NextClient-Initialisierung, native Tastatur-/Fokusabnahme und Laufzeitbereitstellung bleiben offen. Siehe [Laufzeit-Abnahme](../maintainers/runtime-readiness.md).

Der native Provider `VENGINE_GAMEUIFUNCS_VERSION001` implementiert jetzt den vollständigen ursprünglichen Linux-Vertrag mit neun Methoden, Destruktor-Slots und fünfargumentiger Rendererabfrage. Die Factory bietet ihn nur bei bereiten echten Video- und Tasten-Backends an. Tastennamen und Bindings stammen aus geprüften Funktionen der bereits geladenen Xash-Engine; Aktionszustände kommen aus `pFindKey` des Originalclients, Befehlstasten werden über SDL2 und Original-VGUI übersetzt. Unbekannte Engine-Fingerprints und fehlende Dienste lassen die Initialisierung scheitern. Der registrierte i386-Referenzfingerprint ist eine Kompatibilitätsbeobachtung, kein Nachweis eines sauberen Builds oder der Spielbarkeit. Der gemeinsame Resolver prüft frische ELF-/Build-ID-Daten, Symbolzugehörigkeit und ausführbare Mappings; er behauptet keinen Schutz gegen bösartige Änderungen einer lokalen Binärdatei an Ort und Stelle. BaseUI, System-Eigentümerschaft und GameUI-Lebenszyklus sind unten angebunden; native Laufzeitabnahme bleibt offen. Tests nutzen synthetische native Module und starten das Spiel nicht.

Der native Engine-VGUI-Provider besitzt drei getrennte strukturelle Container, die Original-VGUI selbst alloziert. Sein Callback-Vertrag erhält die ursprünglichen 32-Bit-Handles auf beiden Build-Architekturen. Nach dem Start bindet er den vollständigen Original-Viewport ein; Surface-Paint und Eingabe nutzen die gemeinsame Wurzel. Beim Abbau werden geliehene Kinder gelöst und nur eigene Container freigegeben. Angeforderter Abbau wartet auf die Rückkehr des laufenden nativen UI-Aufrufs. Veraltete Steam-Login-Aufrufe melden fehlende Unterstützung über den originalen Fehlerpfad und behaupten keine Authentifizierung. Der native GameUI-Lebenszyklus verwendet diese Engine-Paneldienste.

Die Brücke stellt jetzt die ursprünglichen System-/BaseUI-Verträge bereit und verbindet originale GameUI- und Konsolenstarts mit den bestehenden Viewport-Phasen. Vor Original-Initialize prüft sie passende GameUI und Xash-Dateisystem; die Engine muss dieselbe bereits initialisierte Dateisysteminstanz benennen. Sie hält die tatsächlich vom Originalclient geladenen Module, ohne einen zweiten GameUI-Singleton oder ein weiteres Dateisystem zu erzeugen. Systemdienste verwenden echte Engine-Dateien, Konsole, Parameter, Modul-/Befehlsregistrierung und Framezeit. Optionale Demo-Module bleiben nicht verfügbar.

SDL2 ab 2.0.8 liefert einen geliehenen Event-Watch: Tastatur-/Text-/Fokusereignisse werden gepuffert und im UI-Owner-Thread zugestellt. Es gibt keine SDL-Initialisierung, Fenstererzeugung, Änderung des globalen Eventfilters oder Textinput-Modus. Gameplay-Tasten behalten außerhalb der UI-Sperre ihre originale Behandlung; Text-/Fokus-/Gameplay-Abnahme benötigt weiter einen menschlichen Laufzeittest. Die einzelne Surface meldet Tastaturbedarf aus dem echten nativen UI-Zustand. BaseUI-Paint akzeptiert nur die tatsächlichen vollständigen Fenstergrenzen. Ein Event-Watch kann Engine-Tastenbindungen nicht konsumieren. Der vom Maintainer freigegebene Engine-Tastaturwrapper übernimmt dies getrennt: UI-Tasten samt Wiederholungen und Freigaben bleiben auch nach dem Schließen der UI konsumiert. Im Spiel begonnene Tasten behalten beim Öffnen der UI ihre originale Behandlung von Wiederholungen und Freigaben. Passende Engine-Tastenfreigaben beenden den Besitz, auch bei Fokusverlust. Originale Maus-Clear-/Deaktivierungs-Callbacks setzen Keyboard-Besitz ebenso wenig zurück wie gewöhnliche UI-Wechsel. Escape/toggleconsole aktivieren einmal pro Druck. Andere Ereignisse behalten originale Argumente, Binding-Zeiger und Rückgaben. Sichtbare native tastaturfähige Popups können Capture anfordern; Cursor-Sichtbarkeit allein kann das nicht. Der authentifizierte Capture-Bereich umfasst auch Xash-Internationaltasten 256–264. Schutz pro Taste erhält verschachtelte Gameplay-Repeats/-Freigaben auch bei angefordertem UI-Abbau, ohne Doppelzustellung. Es werden keine zweiten VGUI-Text-/Tastenereignisse erzeugt. Echte Tastatur-/Fokus-/Gameplay-Abnahme bleibt offen.

Shutdown wartet auf aktive native Aufrufe, auch vor Original-Client-Shutdown, und entfernt UI-eigene Befehle, Watches und Controls vor dem Ende ihrer nativen Allocator-/Modullebensdauer. Original-Shutdown läuft höchstens einmal pro erfolgreichem Initialisierungszyklus. Unsicherer Abbau hält den vollständigen Graphen und erfordert einen Prozessneustart; er zählt niemals als erfolgreicher Cleanup. Derselbe GameUI-Kontext ist nach Shutdown nicht wiederverwendbar. Kompatibilitätsfingerprints identifizieren beobachtete Referenzdateien; sie belegen weder Buildherkunft noch einen spielbaren Port.

`xash_client-tests` läuft über das passende Testpreset. CI verwendet zusätzlich `python3 tests/xash_client/run.py --profile linux-port --gtest-source /usr/src/googletest` auf einem Multilib-Host mit GoogleTest-Quellen. Dabei werden dieselbe Brücke und künstliche Callback-Tests ohne SDK-Builds, vcpkg oder lizenzierte Spieldateien gebaut. Ohne `--gtest-source` wird ein passendes installiertes GoogleTest-Paket verwendet; `--profile linux` prüft native x64-Weitergabe und lehnt den nur für i386 vorgesehenen produktiven Spielvertrag ab. Mit `--with-surface` wird zusätzlich die echte Surface mit vendorten SDK-Headern und System-OpenGL/EGL/FreeType gebaut und getestet. Offscreen-Tests nutzen einen EGL-Pbuffer mit `LIBGL_ALWAYS_SOFTWARE=1`; CI führt diese Kombination auf x64 aus. Keiner der Runner startet Xash3D oder lädt Original-Spielbibliotheken.

## Pfade und Bereinigung

Abhängigkeitsbuilds wie OpenSSL scheitern an Quellpfaden mit Leerzeichen. Der Wrapper verwendet den Symlink `${TMPDIR:-/tmp}/csretro-src` ohne Leerzeichen; die Dateien bleiben im Repository. Für manuelle CMake-Befehle dessen `client`-Verzeichnis betreten. Einen konfigurierten Build nicht zwischen echtem Pfad und Symlink verschieben: CMake speichert absolute Pfade.

`scripts/build-client.sh --preset linux-port --clean` baut nur dieses Profil neu. `scripts/clean.sh --client-build linux-port-tests` entfernt nur dessen Testbuild, optional mit `--dry-run`. Andere Profile bleiben erhalten. Die vollständige Bereinigung generierter Daten erfolgt weiterhin über `scripts/clean.sh`.

## Fehlersuche

- Git-LFS-Pointer beim Konfigurieren: `git lfs pull` ausführen.
- Fehlende vcpkg-Baseline: gepinntes Submodul initialisieren; bei einem flachen Klon die vollständige Historie holen.
- Falsche ELF-Klasse oder unpassende Systembibliothek: Profil und passende Systemabhängigkeiten prüfen, dann nur dieses Profil bereinigen und neu konfigurieren. Für `linux-port` ist ELF32 richtig.
- i386-Assemblerfehler bei Registern/Operanden: mit dem getrackten Port-Preset neu konfigurieren, damit auch Assemblerabhängigkeiten `CMAKE_ASM_FLAGS=-m32` erhalten.
- Fehlendes 32-Bit-GL oder FreeType: passende Multilib-Pakete aus den Voraussetzungen installieren. Eine gecachte x64-Bibliothek funktioniert nicht im i386-Build.

Historische Buildmessungen und ihre Commits stehen in [HANDOFF.md](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md).
