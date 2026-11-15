# Bewertung von Orchestrierung und Gedächtnis

Bewertet am 14.11.2026. Grundlage ist Quellensichtung, keine Installation oder Leistungsmessung der Alternativen. Die verbindliche Entscheidung gehört in den [Projektplan](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md); diese Seite erläutert die Abwägung.

## ChatDev

Das aktuelle OpenBMB-Projekt heißt ChatDev 2.0 / DevAll; 1.0 liegt auf einem Legacy-Branch. Die Anwendung umfasst Python-Backend und Vue-Webkonsole, konfigurierbare Modellanbieter und ein Workflow-SDK. Die README beschreibt einen weiterentwickelten Nachfolger, kein aufgegebenes reines 1.0-Werkzeug. [Überblick](https://github.com/OpenBMB/ChatDev/blob/464a681ba7bda869db97a50c5fa707c9a0fb57a0/README.md).

Das geprüfte Paket begrenzt Python auf `>=3.12,<3.13`, trotz der weiter gefassten README-Voraussetzung. Es bringt FastAPI, MCP, FAISS und Modell-SDKs mit; die vollständige Installation ergänzt das Frontend. Auf dem aktuellen Python-3.14-Host braucht es eine getrennte Umgebung. [Paketanforderungen](https://github.com/OpenBMB/ChatDev/blob/464a681ba7bda869db97a50c5fa707c9a0fb57a0/pyproject.toml).

YAML-Graphen verbinden Modell-/Agenten-, Python-, Menschen- und Untergraphknoten. Bedingte Kanten, Nachrichtenverarbeitung, begrenzte Parallelität und Schleifenkontrollen steuern Kommunikation und Ablauf. SDK/CLI und HTTP-Ausführung sind Integrationspunkte. Diese Fähigkeiten eignen sich als Architekturvorbild; sie erzwingen nicht automatisch CSRETROs Git-, Evidenz- oder Spielprozessgrenzen. [Workflow-Vertrag](https://github.com/OpenBMB/ChatDev/blob/464a681ba7bda869db97a50c5fa707c9a0fb57a0/docs/user_guide/en/workflow_authoring.md).

Memory-Stores bieten JSON-basierte Simple-/Dateiindizes und fortgeschriebene Blackboards. Lokale semantische Indizierung benötigt ein Embedding-Modell und sentence-transformers; der dokumentierte Mem0-Adapter nutzt dessen Cloud-API. Dateihashprüfung und explizite Lese-/Schreibzuordnung sind brauchbare Vorbilder. [Memory-Vertrag](https://github.com/OpenBMB/ChatDev/blob/464a681ba7bda869db97a50c5fa707c9a0fb57a0/docs/user_guide/en/modules/memory.md).

Werkzeuge laufen als Python-Funktionen im Prozess mit erzeugten Schemas oder über lokale stdio-/entfernte HTTP-MCP-Server. Ergebnisse ergänzen den Nachrichtenstrom der Agenten. Diese Ausführungswege benötigen ausdrückliche Berechtigungsgrenzen; ihre Übernahme allein würde Repository-Schreibzugriffe nicht isolieren. [Werkzeug-Lebenszyklus](https://github.com/OpenBMB/ChatDev/blob/464a681ba7bda869db97a50c5fa707c9a0fb57a0/docs/user_guide/en/modules/tooling/README.md).

## Lokale Memory-Alternativen

Mem0 OSS unterstützt Bibliotheksbetrieb und Selbsthosting. Standard-LLM und Embeddings nutzen gehostete OpenAI-Modelle; lokaler Betrieb benötigt andere konfigurierte Anbieter und passende laufende Modelle. Ollama wird unterstützt. Für einen Repository-Cache entsteht zusätzlicher Modellbetrieb. [Mem0-Überblick](https://github.com/mem0ai/mem0/blob/f1791daa86d469249c9b002f62644d58a1eb96a0/README.md), [Ollama-Konfiguration](https://github.com/mem0ai/mem0/blob/f1791daa86d469249c9b002f62644d58a1eb96a0/mem0/configs/llms/ollama.py).

Cognee unterstützt inzwischen lokales Gedächtnis ohne LLM und API-Schlüssel. GLiNER-Extraktion und Embedding-Modelle werden beim ersten Gebrauch heruntergeladen; Python 3.10–3.14 wird unterstützt. Graph- und semantische Suche können diese Ressourcen bei größeren Beständen rechtfertigen, sind für den heutigen kleinen Evidenzcache aber unnötig. [Cognee-Schnellstart](https://github.com/topoteretes/cognee/blob/4053f6cc5536757f26a3665eed95c50088e6dc0b/README.md).

Alle drei geprüften Repositories deklarieren Apache-2.0. Framework-Code und Abhängigkeiten werden nicht übernommen. [ChatDev-Lizenz](https://github.com/OpenBMB/ChatDev/blob/464a681ba7bda869db97a50c5fa707c9a0fb57a0/LICENSE), [Mem0-Lizenz](https://github.com/mem0ai/mem0/blob/f1791daa86d469249c9b002f62644d58a1eb96a0/LICENSE), [Cognee-Lizenz](https://github.com/topoteretes/cognee/blob/4053f6cc5536757f26a3665eed95c50088e6dc0b/LICENSE).

## Entscheidung

Native Orchestrierung mit der Python-Standardbibliothek und lokalem SQLite FTS5 verwenden; der vorhandene Coding-Agent führt kontrollierte Fachphasen aus. FTS5 liefert gewichtete lexikalische Suche, kein semantisches Verständnis durch Embeddings. Das vermeidet einen zweiten Modelldienst, zwingende API-Kosten und eine weitere Webanwendung. [FTS5-Fähigkeiten](https://www.sqlite.org/fts5.html).

Verbindliche Entscheidungen bleiben im bestehenden Plan, Projektevidenz im HANDOFF. Memory ist ein löschbarer, quellengebundener Cache mit Gültigkeitsprüfung, kein konkurrierender Plan. GitHub- und Upstream-Arbeit nutzen die vorhandenen Werkzeuge und Workflows. Eine Phasendefinition beweist keinen unabhängigen Agentenlauf: Das Ausführungsprotokoll muss tatsächlichen Bearbeiter und Evidenz nennen. Semantisches Memory erst bei nachgewiesenen Suchproblemen oder entsprechendem Umfang erneut bewerten.
