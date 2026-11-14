# Orchestration and memory evaluation

Evaluated on 2026-11-14. This records source inspection, not installation or performance tests of the alternatives. The authoritative decision belongs in the [project plan](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md); this page explains the tradeoffs.

## ChatDev

The current OpenBMB project is ChatDev 2.0 / DevAll; 1.0 lives on a legacy branch. Its application includes a Python backend and Vue web console, configured model providers and a workflow SDK. Its README describes an actively developed successor, not an abandoned 1.0-only tool. [Overview](https://github.com/OpenBMB/ChatDev/blob/464a681ba7bda869db97a50c5fa707c9a0fb57a0/README.md).

The evaluated package restricts Python to `>=3.12,<3.13`, despite the broader README prerequisite. It brings FastAPI, MCP, FAISS and model SDKs; full installation also adds the frontend. This requires a separate environment on the current Python 3.14 host. [Package requirements](https://github.com/OpenBMB/ChatDev/blob/464a681ba7bda869db97a50c5fa707c9a0fb57a0/pyproject.toml).

YAML graphs connect model/agent, Python, human and subgraph nodes. Conditional edges, message processing, bounded parallelism and loop controls define communication and scheduling. SDK/CLI and HTTP execution are integration points. These capabilities are useful architectural references; they do not automatically enforce CSRETRO's Git, evidence or game-process boundaries. [Workflow contract](https://github.com/OpenBMB/ChatDev/blob/464a681ba7bda869db97a50c5fa707c9a0fb57a0/docs/user_guide/en/workflow_authoring.md).

Memory stores provide JSON-backed simple/file indexes and append-only blackboards. Local semantic indexing needs an embedding model and sentence-transformers; the documented Mem0 adapter uses its cloud API. Hash-based file refresh and explicit read/write attachments are useful design references. [Memory contract](https://github.com/OpenBMB/ChatDev/blob/464a681ba7bda869db97a50c5fa707c9a0fb57a0/docs/user_guide/en/modules/memory.md).

Tools run as in-process Python functions with generated schemas, or through local stdio/remote HTTP MCP servers. Outputs join the agent message stream. These execution mechanisms need explicit privilege boundaries; importing them alone would not sandbox repository writes. [Tool lifecycle](https://github.com/OpenBMB/ChatDev/blob/464a681ba7bda869db97a50c5fa707c9a0fb57a0/docs/user_guide/en/modules/tooling/README.md).

## Local memory alternatives

Mem0 OSS provides library/self-hosted deployment. Its default LLM and embeddings use hosted OpenAI models; local operation requires configuring alternative providers and running suitable models. Ollama is supported. This adds model operations for a repository cache. [Mem0 overview](https://github.com/mem0ai/mem0/blob/f1791daa86d469249c9b002f62644d58a1eb96a0/README.md), [Ollama configuration](https://github.com/mem0ai/mem0/blob/f1791daa86d469249c9b002f62644d58a1eb96a0/mem0/configs/llms/ollama.py).

Cognee currently supports local memory without an LLM or API key. Its GLiNER extraction and embedding models download on first use; Python 3.10–3.14 is supported. Graph and semantic retrieval may justify those resources for larger corpora, but they are unnecessary for today's small evidence cache. [Cognee local quickstart](https://github.com/topoteretes/cognee/blob/4053f6cc5536757f26a3665eed95c50088e6dc0b/README.md).

All three evaluated repositories declare Apache-2.0. No framework code or dependencies are imported. [ChatDev license](https://github.com/OpenBMB/ChatDev/blob/464a681ba7bda869db97a50c5fa707c9a0fb57a0/LICENSE), [Mem0 license](https://github.com/mem0ai/mem0/blob/f1791daa86d469249c9b002f62644d58a1eb96a0/LICENSE), [Cognee license](https://github.com/topoteretes/cognee/blob/4053f6cc5536757f26a3665eed95c50088e6dc0b/LICENSE).

## Decision

Use native Python standard-library orchestration and local SQLite FTS5, with the existing coding agent performing controlled specialist phases. FTS5 supplies ranked lexical retrieval; it is not embedding-based semantic understanding. This choice avoids a second model service, mandatory API charges and another web application. [FTS5 capabilities](https://www.sqlite.org/fts5.html).

Keep authoritative decisions in the existing plan and project evidence in HANDOFF. Memory is a disposable, source-linked cache with explicit validity checks, not a competing plan. Keep GitHub and upstream work on the existing tools and workflows. A phase definition is not proof that an independent agent ran: the execution record must identify its real worker and evidence. Reconsider semantic memory only after retrieval failures or measured scale justify a model stack.
