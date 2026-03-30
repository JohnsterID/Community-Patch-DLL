# SQLite Usage Review — Community-Patch-DLL

## Engine SQLite Version

**SQLite 3.7.17** — confirmed via string in the Linux binary (`Civ5XP.c`). Matches community reports that it coincides with the BNW (Brave New World) release timeframe (2013-05-20).

## Compatibility Verdict

**All 273 SQL files and 117 Lua DB.Query/CreateQuery calls are fully compatible with 3.7.17.** No post-3.7.17 features detected.

### Features NOT used (good — they'd break):
| Feature | Requires | Status |
|---|---|---|
| WITH / CTE | 3.8.3 | ✅ Not used |
| UPSERT (ON CONFLICT) | 3.24.0 | ✅ Not used |
| Window functions (OVER) | 3.25.0 | ✅ Not used |
| IIF() | 3.32.0 | ✅ Not used |
| RENAME COLUMN | 3.25.0 | ✅ Not used |
| RETURNING | 3.35.0 | ✅ Not used |
| RIGHT/FULL OUTER JOIN | 3.39.0 | ✅ Not used |
| TRUE/FALSE keywords | 3.23.0 | ✅ Not used |

### Features used (all 3.7.17-safe):
- Standard DML: INSERT, UPDATE, DELETE, SELECT
- ALTER TABLE ADD COLUMN (1,013 statements — bulk of schema extension)
- CREATE TABLE, CREATE TRIGGER (14 total), CREATE INDEX IF NOT EXISTS
- DROP TABLE
- REPLACE INTO (5 uses)
- CAST (including CAST AS NUMERIC)
- CASE WHEN (23 uses)
- Subqueries in UPDATE SET and WHERE
- Compound VALUES: `INSERT INTO t VALUES (...), (...)` (needs 3.7.11 ✓)
- PRAGMA table_info (introspection)
- ORDER BY Random() LIMIT 1

## SQL File Statistics

273 `.sql` files containing:
| Statement | Count |
|---|---|
| UPDATE | 4,297 |
| INSERT | 1,809 |
| ALTER TABLE | 1,013 |
| SELECT | 376 |
| DELETE | 340 |
| CREATE | 145 |
| DROP | 98 |
| REPLACE | 5 |

## Lua-Side DB Usage

117 `DB.Query` / `DB.CreateQuery` calls across Lua files.

**6 queries use string concatenation instead of parameterized queries:**

1. `VPUI_core.lua:150` — `PRAGMA table_info(` .. strTable .. `)` — unavoidable (PRAGMA doesn't support `?`)
2. `PlotMouseoverInclude.lua:271` — `WHERE Leaders.ID = ` .. iLeader
3. `CivilopediaScreen.lua:2777` — same pattern
4. `CivilopediaScreen.lua:6388` — same pattern
5. `TopPanel.lua:389` (EUI) — same pattern
6. `TopPanel.lua:470` (EUI compat) — same pattern

Items 2–6 concatenate integer IDs from trusted game API calls — no injection risk in practice, but could use `DB.Query("... WHERE Leaders.ID = ?", iLeader)` for consistency.

## Performance Awareness

The project is well-optimized:
- `docs/db.md` documents `GameInfo` (slow) vs `DB.Query` (medium) vs `DB.CreateQuery` (fast) with benchmarks
- 2025 migration adds 60+ custom indexes (`Database Migrations/2025_01_27_AddTableIndexes.sql`)
- Hot-path Lua code uses `DB.CreateQuery` for precompiled statements
- VP Lua has largely moved away from `GameInfo.*` to direct SQL

## Notable Patterns

### Triggers (14 total)
Well-written compatibility triggers in `Triggers.sql` — auto-propagate modmodder additions (e.g., new melee units auto-get Polynesia's Fishing Boats build). Clean AFTER INSERT/UPDATE/DELETE with proper WHERE NOT EXISTS guards.

### Schema Extension Strategy
The project extends Civ5's base schema heavily via `ALTER TABLE ADD COLUMN` (1,013 times). New columns are added with sensible defaults. No `ALTER TABLE` incompatibilities.

### CREATE INDEX Migration
`2025_01_27_AddTableIndexes.sql` adds 60+ indexes with `IF NOT EXISTS` — safe, idempotent, improves query performance on frequently-joined columns.

## What's Missing Because of 3.7.17

CTEs (`WITH ... AS`) would clean up some complex queries, but none of the current SQL actually needs them. The project's SQL is straightforward — mostly single-table UPDATEs/INSERTs with WHERE clauses.

## Summary

The project's SQL is clean, conservative, and fully compatible with the engine's SQLite 3.7.17. No changes needed for compatibility. The only minor improvement would be parameterizing 5 Lua queries that concatenate integer IDs, but this is cosmetic — no functional risk.
