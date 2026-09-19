# Working on the documentation

The website uses [MkDocs](https://www.mkdocs.org/) and
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/).
`mkdocs-static-i18n` builds German and English pages from Markdown.

## Local preview

```bash
python3 -m venv .venv-docs
. .venv-docs/bin/activate
python -m pip install -r requirements-docs.txt
python scripts/check-docs.py
mkdocs build --strict
mkdocs serve
```

Open the local address printed by the server. No game data is needed.

## Files and translations

- `docs/<topic>.de.md` and `docs/<topic>.en.md` form a pair.
- German pages live at `/csretro/`; English pages at `/csretro/en/`.
- Navigation lives in `mkdocs.yml`; translated titles in `nav_translations`.
- Page links use the language-neutral name, such as `controls.md`.
- The language switcher should stay on the equivalent page.
- Styles and screenshots live in `docs/assets/`.
- No externally loaded webfonts, analytics trackers or embedded third-party videos.

## Screenshots

Publish genuine game captures, never reference images presented as the project itself.
Record subject, date and development status. Avoid private data during capture;
disclose any necessary editing in the caption. Check readability and alternative text.

`scripts/capture-docs.sh` creates captures using existing local builds and game data,
in a separate runtime directory. It requires Gamescope, Xwayland, xdotool and ImageMagick.
Captures are not committed automatically; inspect them before inclusion.

## Automated checks

Pull requests build the website with `--strict` and check translation pairs, links and the public
documentation boundary. Only a push to `main` publishes through GitHub Pages.
PR code does not receive deployment write permissions.

The site is built exclusively from `docs/`, not the whole repository.
Original game contents, logs and development artifacts are not website content.
