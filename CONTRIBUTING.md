# Mitmachen / Contributing

[Deutsch](https://benjarogit.github.io/csretro/contributing/) ·
[English](https://benjarogit.github.io/csretro/en/contributing/)

Feedback, Ideen, Tests, Übersetzungen und Code sind willkommen. Bitte suche vor einem neuen
[Issue](https://github.com/benjarogit/csretro/issues/new/choose) nach bestehenden Meldungen.
Beschreibe bei Fehlern Version/Commit, Plattform, Reproduktionsschritte, erwartetes und tatsächliches
Verhalten. Entferne persönliche Daten aus Logs.

Feedback, ideas, testing, translations and code are welcome. Search existing issues first.
For bugs, include the version/commit, platform, reproduction steps, expected and actual results.
Remove personal information from logs.

## Pull requests

- Fork the repository and use a focused branch.
- Discuss larger gameplay or architecture changes in an issue before implementing them.
- Explain what changed and how it was tested; distinguish compilation from in-game testing.
- Update both documentation languages when changing documented behavior.
- Preserve upstream credits and license notices. Identify the source and permission for imported material.
- Do not upload original game data, third-party asset packs without permission, credentials or personal logs.
- Use respectful, constructive communication. No harassment, personal attacks or publication of private information.

Build: [Deutsch](https://benjarogit.github.io/csretro/build/) /
[English](https://benjarogit.github.io/csretro/en/build/).
The documentation build does not require game assets.

## Documentation checks

```bash
python3 -m venv .venv-docs
. .venv-docs/bin/activate
python -m pip install -r requirements-docs.txt
python scripts/check-docs.py
mkdocs build --strict
mkdocs serve
```

Contributions remain subject to the applicable component licenses. No blanket relicensing is implied.
