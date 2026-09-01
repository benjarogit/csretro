# Rollen (bindend)

CS Retro wechselt **nicht** von NextClient auf cs16-client.
NextClient bleibt die funktionale Zielbasis. Ref A liefert nur den fehlenden Xash-Unterbau.

| Rolle | Was | Pfad | Darf |
|--------|-----|------|------|
| **CS Retro** | Produkt-Codebase / Worktree | Repo-Wurzel | hier wird entwickelt |
| **Xash3D-FWGS** | einzige Engine | `engine/` | Bindung, Build, Interface |
| **NextClient** | funktionales Zielverhalten des Clients | `client/nextclient/` (+ NitroApi/SDK als Herkunft) | Features herauslösen, auf Xash umbauen |
| **Ref A** (Velaron/cs16-client) | Xash-kompatible **Client-Body-Quelle** | `refs/a-cs16-client/` | nur A1-Allowlist, siehe unten |
| **Ref B** (FuryBaM) | bedingte UI-/Menü-Referenz | `refs/b-cs16-goldsrc/` | erst Phase 4, ein Feature |
| **Server** | eigener Bereich | `server/` | AMXX-Herkunft; Xash-GameDLL = eigene Entscheidung |
| **Bots** | eigener Bereich | `bots/` | leer; kein YaPB |

## Stapel (eine Client-Lib)

```
Xash3D-FWGS
  → CS-Retro Client-Export
    → CS-Client-Body aus Ref A (nach Phase-3-Vendor: client/body/, dann CS-Retro-Code)
      → darauf integrierte NextClient-Funktionen
```

Am Ende: **eine** `client.so` / `client.dll`, **eine** Implementierung. Kein zweiter Client neben NextClient. Ref A wird nicht weiterentwickelt.

## A1-Allowlist (Ref A → später `client/body/`)

Nur was der Xash-Client-Unterbau braucht:

- `cl_dll/`
- `pm_shared/`
- zwingend: `common/`, `public/`, `game_shared/`, `dlls/wpn_shared/` (Client-Build, nicht die GameDLL)

**Nicht:** YaPB, ReGameDLL, `mainui` / `mainui_cpp`, restliches `dlls/`, `3rdparty/` außer dem, was die Allowlist schon nennt. Keine stillen Imports.

A1 heißt nicht: cs16-client weiterpflegen und NextClient danebenlegen.
A1 heißt: den Unterbau holen, den NextClient bisher aus Steam-`client.dll` vorausgesetzt hat. Danach NextClient-Funktionen aus Hooks/NitroApi/8684 lösen und auf diesen Unterbau / Xash-Schnittstellen setzen.

Linux und die übrigen Xash-Plattformen von Anfang an mitdenken.

## Redundanz (Ref A Body vs. NextClient-Feature)

1. Überschneidung benennen.
2. Beide Varianten lesen.
3. Gewünschtes **NextClient-Verhalten** behalten.
4. Passende Technik des Unterbaus nutzen.
5. Eine CS-Retro-Implementierung — keine Doppel-Systeme.

## Ref-A-Regel (nach A1)

Ref A bleibt Referenz. **Ausnahme nur** die Allowlist. Sonst kein Copy. `refs/a-cs16-client/` nach dem späteren Vendor nicht als zweiter Client bauen.
