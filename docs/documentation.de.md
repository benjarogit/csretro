# An der Dokumentation arbeiten

Die Website wird mit [MkDocs](https://www.mkdocs.org/) und
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/) gebaut.
`mkdocs-static-i18n` erzeugt die deutschen und englischen Seiten aus Markdown.

## Lokal starten

```bash
python3 -m venv .venv-docs
. .venv-docs/bin/activate
python -m pip install -r requirements-docs.txt
python scripts/check-docs.py
mkdocs build --strict
mkdocs serve
```

Öffne die vom Server angezeigte lokale Adresse. Dafür werden keine Spieldaten benötigt.

## Dateien und Übersetzungen

- `docs/<thema>.de.md` und `docs/<thema>.en.md` bilden ein Paar.
- Die deutsche Website liegt unter `/csretro/`, die englische unter `/csretro/en/`.
- Die Navigation steht in `mkdocs.yml`; englische Titel in `nav_translations`.
- Links zwischen Seiten verwenden den sprachneutralen Namen, etwa `controls.md`.
- Der Sprachschalter soll auf der entsprechenden Seite bleiben.
- Styles und Screenshots liegen in `docs/assets/`.
- Keine extern geladenen Webfonts, Analyse-Tracker oder eingebetteten Drittanbieter-Videos.

## Screenshots

Nur echte Aufnahmen des Spiels veröffentlichen, keine Referenzbilder als eigenes Spiel ausgeben.
Beschreibe Motiv, Datum und Entwicklungsstand. Entferne private Daten vor der Aufnahme;
bei notwendigen Bearbeitungen muss die Bildunterschrift sie nennen. Prüfe Lesbarkeit und Alt-Texte.

`scripts/capture-docs.sh` erzeugt neue Aufnahmen mit vorhandenen lokalen Builds und Spieldaten
in einem separaten Laufzeitverzeichnis. Es braucht Gamescope, Xwayland, xdotool und ImageMagick.
Die Aufnahmen werden nicht automatisch committed; prüfe sie vor der Übernahme.

## Automatische Prüfung

Pull Requests bauen die Website mit `--strict` und prüfen Sprachpaare, Links sowie den
öffentlichen Dokumentationsumfang. Erst ein Push auf `main` veröffentlicht die Website
über GitHub Pages. PR-Code bekommt dafür keine Deployment-Schreibrechte.

Die Website wird allein aus `docs/` erzeugt, nicht aus dem gesamten Repository.
Originale Spielinhalte, Logs und Entwicklungsartefakte sind kein Website-Inhalt.
