# IDE and provider adapters

The coordinator works through the repository CLI and shared local SQLite state. It requires neither Codex nor Cursor, a running editor, a model account, or a model API. A client name identifies a cooperating reader/writer; it does not select a model or prove an agent is running. Use a lowercase identifier of at most 48 ASCII letters, digits, underscores or hyphens, starting with a letter. The default is `cli`.

## Discover and hand off

From the repository root:

```sh
scripts/chatdev.sh adapters
scripts/chatdev.sh --client external-ide export 1
scripts/chatdev.sh --client external-ide resume 1
```

Replace `1` with an existing task ID. `adapters` always reports the built-in local JSON transport. Optional executable discovery uses PATH lookup only for `codex`, `cursor-agent`, and the `cursor` desktop launcher. It does not execute these programs, check their authentication, install software, attach to another session or spend model credits. A discovered desktop launcher does not establish that a separate Agent CLI is installed.

`export` writes a versioned assignment under `.local/chatdev/assignments/` with private permissions. Its receipt contains a repository-relative filename and SHA-256. Identical task/client payloads reuse one immutable file; changed state creates a new file. Concurrent exports publish complete files without overwriting existing assignments. Protected paths, unsafe client names, escaping scope/source paths and managed-directory symlinks are rejected. This is cooperative coordination, not a filesystem sandbox against another process.

Schema version 1 contains the persisted task ID, objective, mode, state, selected roles, scope and source fingerprints, plus references to the shared database and status file. It excludes full chats, raw GitHub snapshots and role-result transcripts. The assignment grants no execution rights. A consumer must resume the task, reread the current harness/plan/handoff and acquire file claims before editing. Retained assignments are historical snapshots; a file alone never proves that its authority fingerprints are still current. Results still enter through `record`, with real actors and evidence; independent quality and fresh explicit merge approval remain necessary.

## Optional official integrations

Cursor documents its [Agent CLI](https://cursor.com/docs/cli/using), including MCP and ACP, and [headless print mode](https://cursor.com/docs/cli/headless). Those are possible future execution boundaries, not features installed by this adapter. The inspected local desktop-launcher help lists an `agent` subcommand; the standalone `cursor-agent` executable was absent during inspection. The launcher may install components when that subcommand is used, so it was not invoked. No installer, permission bypass or background Cursor session was started.

Codex documents its [CLI commands](https://developers.openai.com/codex/cli/reference/), including noninteractive execution. Local `codex --help` confirmed the CLI and its `exec` command without starting a model task. The adapter only discovers its executable. Presence is not authentication, availability of a particular model, permission to edit, or authorization for paid calls.

The file handoff is usable in any editor that can read JSON. It is not an ACP/MCP server, an editor extension, a model runner or an automatic editor-session synchronizer. Both editors can explicitly resume the same repository task; no existing session is modified behind the user's back.

## Reuse in a future Orqen project

The reusable boundaries are the versioned assignment schema, safe client identity, capabilities discovery, task/evidence records and shared-state references. An explicitly authorized execution adapter could consume those boundaries without making the coordinator depend on an IDE vendor. That adapter would need its own documented permissions, result validation, interruption behavior and cost controls.

CSRETRO's authority paths, gate semantics, original-content restrictions, upstream registry and build/test commands remain this project's profile. Extract and test a configurable project profile before installing the tooling elsewhere; do not copy CSRETRO defaults into Orqen or create a second authoritative plan. This change does not edit, initialize or install anything in an Orqen repository.
