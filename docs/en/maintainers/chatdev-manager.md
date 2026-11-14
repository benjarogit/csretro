# Project manager and continuation

The builtin `project_manager` is an executed coordination role, not another permanently running model. The role registry contains fifteen responsibilities: the previous fourteen host phases plus the automatic PM. Start/resume/status/backlog invoke `ProjectManager.report`, persist its result and return it to the orchestrator as `project_manager`. The report includes the current milestone from the authoritative handoff/plan, verified versus recorded phase results, priorities, dependencies, blockers, collaboration and a next action.

## Work with the existing state

```sh
scripts/chatdev.sh --client local-host status
scripts/chatdev.sh --client local-host continue
scripts/chatdev.sh assign 2 --priority P1 --depends-on 1
```

Use actual existing task IDs. `continue` refreshes GitHub, selects an unfinished item and returns `resume-existing` with that same ID; it does not create a new project, run a model or manipulate an IDE. The host consumes the recommendation and does the work. A newly needed repository/CI role is added to the existing task and prior quality approval is invalidated. Terminal tasks are not restarted. With no unfinished local item, the manager refers to the actual `Next task` section in HANDOFF.md before generic historical port summaries. `--offline` explicitly leaves remote state unverified.

`assign` persists P0–P3 priorities and local task dependencies. Unknown/self/cyclic dependencies are rejected atomically. Dependency completion needs current evidence and independent quality, not merely a recorded done status. A failed check or requested review routes back to existing work. Current-CI selection excludes closed-branch history and superseded runs; unknown metadata is reported rather than called green. Scope/code/authority changes downgrade progress from verified to recorded-only.

An open integration PR holds continuation for live protected Auto-Merge verification against `.github/merge-policy.json` by `tools/github_merge_gate.py`. The merge gate binds the acceptance declaration digest to an independent current-head approved review using `CSRETRO-ACCEPTANCE <sha256>`; later declaration edits invalidate that proof. The manager recommends `verify-protected-auto-merge`; it never certifies effective protection, acceptance or authorization from snapshot checks. Missing protection blocks merging. Sensitive labels from the central policy, with a conservative fallback when unavailable, recommend explicit scoped maintainer approval; the trusted merge gate performs authoritative changed-path classification. Releases, production deployments, paid actions, critical permissions and other security-sensitive changes retain explicit approval. Do not skip the prior gate to start G07 code. Once its prerequisite is closed, the host resumes the existing documented task, without repeating completed implementation.

## Shared clients, not vendor lock-in

Any safe client identifier can use this protocol; `cli` is the default. Cursor/Codex are optional consumers, not dependencies. Both read/write the same checkout's ignored `.local/chatdev/memory.sqlite3`. PM reports carry monotonically increasing IDs; a process lock serializes SQLite recording and atomic `status.json` publication. Last-observed client entries are observations, not claims that an IDE agent is running. The latest 100 derived reports are retained; tasks, results and knowledge are preserved separately.

The [adapter guide](chatdev-adapters.md) documents the universal file handoff, optional official interfaces and Orqen reuse boundary. No installer, model call or live-session change is implicit. This implementation uses Linux/POSIX process locking; adapt that boundary before a future Windows port.

## Validation

Unit tests exercise PM decisions and evidence. Integration tests spy on actual PM calls from the coordinator, inspect persisted output and run separate CLI processes labelled as clients. Concurrent publication, fresh CI dispatch, blocked merge continuation, dependency cycles and stale proof are tested. They prove the protocol and builtin PM execution, not a running Cursor/Codex model session. Exact executed counts and CI links belong in HANDOFF.md.
