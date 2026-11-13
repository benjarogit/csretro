# Plugin-Autoren

Der Client ist noch nicht spielbar. Die Server-Seite ist ein AMXX-Modul
(vendorte NextClientServerApi, Rebrand folgt den Gates).

## Was heute da ist

- Modul und Testplugin im Testserver-Lauf (siehe [Testserver](../server/testserver.md)).
- Build der Server-API: [Server bauen](../build/server.md).

## Was ein Plugin noch nicht kann

- Eine stabile, vollständig rebrandete `csr_`-Native-Oberfläche und generierte
  Native-Tabellen entstehen mit späteren Gates (Plan G08/G11). Bis dahin ist die
  vendorte API die technische Referenz im Baum, nicht eine fertige öffentliche
  Plugin-Doku.

Kein zweiter Doku-Baum. Wenn Native-Tabellen generiert werden, landen sie unter
`docs/de` und `docs/en` und auf der Page.

## Gegen die bisherige API entwickeln

Der vendorte [Pawn-Include](https://github.com/benjarogit/csretro/blob/main/server/api/addons/amxmodx/scripting/include/next_client_api.inc) ist der genaue API-Vertrag. Er verwendet bisher die Upstream-Namen mit `ncl_`. Erfinde vor dem Protokoll-/Rebrand-Gate keine `csr_`-Ersatznamen. Metamod-R lädt den Server-Stack; dieser AMXX-Include ist kein allgemeines Metamod-Plugin-SDK.

Warte auf `ncl_client_api_ready`, prüfe Bereitschaft und unterstützte Fähigkeiten und nutze erst dann die passende Sandbox-, Sprite-, Viewmodel-, Sound- oder Materialoperation. Behandle Verbindungsabbruch und nicht verfügbare Clients. Prüfe Kompilierung und Verhalten gegen dieselbe Server-/Modulrevision im [Testserver](../server/testserver.md). Ein erfolgreich geladenes Servermodul belegt noch kein funktionierendes x64-Client-Rendering und keine Anmeldung.

Generierte Native-Referenzen müssen bei API-Änderungen aus dem Include abgeleitet werden. Die [Funktionsübersicht](../players/features.md) beschreibt den sichtbaren Umfang; Herkunft steht zentral in [CREDITS](https://github.com/benjarogit/csretro/blob/main/CREDITS.md).
