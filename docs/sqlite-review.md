# SQLite Usage Review — Community-Patch-DLL

## Engine SQLite Version

**SQLite 3.7.17** (2013-05-20) — confirmed via `sqlite3_libversion()` and `sqlite3_sourceid()` strings in the Linux binary (`Civ5XP.c`). Shipped with the BNW (Brave New World) expansion.

### Can We Upgrade?

**No.** SQLite is statically compiled into the engine's `CvGameDatabase` DLL (precompiled, no source). All SQL goes through the `Database::Connection` abstraction which wraps the embedded SQLite. Both the game EXE and the CvGameDatabase DLL embed their own copy. We link against `CvGameDatabaseWin32.lib` (import library only) and cannot rebuild the DLL.

## Engine Architecture

### Database::Connection API

The `Database::Connection` class in `CvGameDatabase/include/Database.h` wraps SQLite with:

| Method | Description |
|---|---|
| `Open(filename, flags)` | Opens database; defaults to `:memory:` if filename is NULL |
| `Execute(sql)` | Execute single SQL statement |
| `ExecuteMultiple(sql)` | Execute multiple concatenated statements |
| `Analyze()` | Runs `ANALYZE` via `sqlite3_exec` |
| `Vacuum()` | Runs `VACUUM` (no-op on `:memory:` databases) |
| `Count(table)` | Cached row count with prepared statements |
| `ClearCountCache()` | Clears prepared statement cache for Count() |
| `BeginTransaction()` | Transaction control |
| `SetSavePoint(name)` | Savepoint support |
| `ValidateFKConstraints()` | Foreign key validation |
| `CalculateMemoryStats()` | Returns memory usage string for logging |
| `GetSQLite3()` | Raw `sqlite3*` handle (internal use) |

### Lua DB Bindings

The engine exposes these functions to Lua via `cvLuaDBLibrary`:

| Lua Function | Description |
|---|---|
| `DB.Query(sql, ...)` | Parse + bind parameters + execute; returns row iterator |
| `DB.CreateQuery(sql)` | Precompile statement; returns reusable closure |
| `DB.StatementCount()` | Count of active prepared statements |
| `DB.StatementSQL(n)` | SQL text of the n-th prepared statement |
| `DB.GetMemoryUsage()` | SQLite memory usage |
| `DB.CollectMemoryUsage()` | Release SQLite memory |

`DB.Query` binds parameters by position, supporting types: nil→NULL, boolean→int, number→int64, string→text. `DB.CreateQuery` returns a closure that accepts the same parameter types when called.

No `DB.Analyze()` or `DB.Execute()` is exposed to Lua — all analysis must happen from C++.

### Memory Configuration

Configured in `Database::Connection::InitMemoryManagement()`:

| sqlite3_config | Setting | Value |
|---|---|---|
| Mode 4 (SQLITE_CONFIG_MALLOC) | Custom allocator | Firaxis memory manager |
| Mode 7 (SQLITE_CONFIG_PAGECACHE) | Page cache | 5,000 pages × 1,168 bytes = 5.84 MB static buffer |
| Mode 6 (SQLITE_CONFIG_SCRATCH) | Scratch memory | 1 slot × 7,008 bytes static buffer |
| Mode 13 (SQLITE_CONFIG_URI) | URI filenames | Disabled |

These values match the constants in `Database.h`: `DB_NUM_PAGES=5000`, `DB_PAGECACHE_SIZE=1168`, `DB_NUM_THREADS=1`.

### Database Open Modes

| Flag Value | Flags | Usage |
|---|---|---|
| `32774` (0x8006) | READWRITE \| CREATE \| NOMUTEX | Gameplay database (`:memory:`, single-threaded) |
| `65538` (0x10002) | READWRITE \| FULLMUTEX | Thread-safe database access |
| `65542` (0x10006) | READWRITE \| CREATE \| FULLMUTEX | Thread-safe creation |
| `6` (0x0006) | READWRITE \| CREATE | Standard file database |

The main gameplay database is opened as `:memory:` with `NOMUTEX` — no thread safety overhead.

### Engine ANALYZE Timing

The engine calls `ANALYZE` at two points:
1. **After initial schema loading** — `Vacuum()` then `Analyze()` before mods load
2. **After mod SQL loading** — `Analyze()` then `Vacuum()`, but **only if** `TotalChanges()` changed (INSERT/UPDATE/DELETE)

Problem: `CREATE INDEX` does not increment `TotalChanges()`, so the engine's post-mod `ANALYZE` may not fire after VP's 60+ index additions. Our `DB.Analyze()` call in `CvDllDatabaseUtility::CacheGameDatabaseData()` addresses this gap.

### Compile Options (Inferred)

The binary reports 7 compile options (`sqlite3_compileoption_get` iterates 0–6). Confirmed from function presence/absence analysis:

| Option | Confidence | Evidence |
|---|---|---|
| `THREADSAFE=1` | Confirmed | `sqlite3_threadsafe()` returns 1 |
| `OMIT_AUTHORIZATION` | Confirmed | `sqlite3_set_authorizer` absent from binary |
| `OMIT_PROGRESS_CALLBACK` | Confirmed | `sqlite3_progress_handler` absent from binary |
| `OMIT_DEPRECATED` | Confirmed | All deprecated functions absent (`sqlite3_expired`, `sqlite3_global_recover`, etc.) |
| `OMIT_AUTOINIT` | Likely | Engine explicitly calls `sqlite3_initialize()` in Connection constructor |
| `TEMP_STORE=2` | Likely | All databases are `:memory:`, temp storage in memory is consistent |
| `HAVE_ISNAN` | Likely | Standard on Linux/GCC platforms where the binary was compiled |

The actual strings are stored as data pointers in the binary and could not be extracted as text from the decompiled output. Use `PRAGMA compile_options` in-game to confirm.

### Functions Present vs Absent

**Present** (not compiled out): `sqlite3_load_extension`, `sqlite3_trace`, `sqlite3_profile`, `sqlite3_enable_shared_cache`, `sqlite3_backup_init`, `sqlite3_blob_open/read/write`, WAL functions, virtual table functions, `sqlite3_commit_hook`, `sqlite3_update_hook`, `sqlite3_create_function`, UTF-16 functions.

**Absent** (compiled out): `sqlite3_set_authorizer`, `sqlite3_progress_handler`, `sqlite3_table_column_metadata`, all deprecated functions, FTS3/FTS4, RTREE, ICU.

**Stub** (returns 0): `sqlite3_release_memory` — always returns 0 (custom allocator doesn't support memory release via this API).

## Compatibility Verdict

**All 273 SQL files and 117 Lua DB.Query/CreateQuery calls are fully compatible with 3.7.17.** No post-3.7.17 features detected.

### Features NOT used (good — they'd break):
| Feature | Requires | Status |
|---|---|---|
| WITH / CTE | 3.8.3 | ✅ Not used |
| Partial indexes (CREATE INDEX ... WHERE) | 3.8.0 | ✅ Not used |
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
- PRAGMA table_info, user_version, foreign_key_list
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

88 `DB.Query` calls and 29 `DB.CreateQuery` calls across Lua files.

### Concatenated Queries (Parameterization)

6 queries originally used string concatenation. 5 have been converted to parameterized queries. 1 remains unavoidable:

| File | Status | Notes |
|---|---|---|
| `VPUI_core.lua:150` | Unavoidable | `PRAGMA table_info(` .. table .. `)` — PRAGMAs don't support `?` binding |
| `PlotMouseoverInclude.lua:271` | ✅ Fixed | `WHERE Leaders.ID = ?` |
| `CivilopediaScreen.lua:2777` | ✅ Fixed | `WHERE Leaders.ID = ?` |
| `CivilopediaScreen.lua:6388` | ✅ Fixed | `WHERE Leaders.ID = ?` |
| `TopPanel.lua:389` (EUI) | ✅ Fixed | `WHERE Leaders.ID = ?` |
| `TopPanel.lua:470` (EUI compat) | ✅ Fixed | `WHERE Leaders.ID = ?` |

## Improvements Made

### 1. ANALYZE After Mod Index Creation

Added `DB.Analyze()` call in `CvDllDatabaseUtility::CacheGameDatabaseData()` after `ClearCountCache()` and before `CalculateMemoryStats()`. This ensures the query planner has up-to-date statistics for the 60+ indexes added by `2025_01_27_AddTableIndexes.sql`.

Without this, the engine's own ANALYZE (which runs before mod loading, or conditionally after mod SQL if `TotalChanges()` changed) would leave the new indexes without statistics, potentially causing the query planner to ignore them.

### 2. Parameterized Queries

Converted 5 concatenated `DB.Query` calls to use `?` parameter binding. While the original code concatenated trusted integer IDs from game API calls (no injection risk), parameterized queries avoid per-call SQL reparsing and follow best practices.

## Performance Awareness

The project is well-optimized:
- `docs/db.md` documents `GameInfo` (slow) vs `DB.Query` (medium) vs `DB.CreateQuery` (fast) with benchmarks
- 2025 migration adds 60+ custom indexes (`Database Migrations/2025_01_27_AddTableIndexes.sql`)
- Hot-path Lua code uses `DB.CreateQuery` for precompiled statements
- VP Lua has largely moved away from `GameInfo.*` to direct SQL
- Base game has zero gameplay indexes; VP's 60+ indexes are a significant performance improvement

## Notable Patterns

### Triggers (14 total)
Well-written compatibility triggers in `CoreTriggers.sql` (3) and `Triggers.sql` (11) — auto-propagate modmodder additions (e.g., new melee units auto-get Polynesia's Fishing Boats build). Clean AFTER INSERT/UPDATE/DELETE with proper WHERE NOT EXISTS guards.

### Schema Extension Strategy
The project extends Civ5's base schema heavily via `ALTER TABLE ADD COLUMN` (1,013 times). New columns are added with sensible defaults. No `ALTER TABLE` incompatibilities.

### CREATE INDEX Migration
`2025_01_27_AddTableIndexes.sql` adds 60+ indexes with `IF NOT EXISTS` — safe, idempotent, improves query performance on frequently-joined columns.

## What's Missing Because of 3.7.17

| Feature | Would Help With | Required Version |
|---|---|---|
| CTEs (`WITH ... AS`) | Readability of complex queries | 3.8.3 |
| Partial indexes (`CREATE INDEX ... WHERE`) | Smaller indexes on filtered subsets | 3.8.0 |
| UPSERT (`INSERT ... ON CONFLICT`) | Simpler insert-or-update patterns | 3.24.0 |

None are showstoppers. The project's SQL is straightforward — mostly single-table UPDATEs/INSERTs with WHERE clauses.

## Summary

The project's SQL is clean, conservative, and fully compatible with the engine's SQLite 3.7.17. The SQLite version cannot be upgraded because it is statically compiled into the precompiled engine DLL. All parameterizable concatenated queries have been converted. An `ANALYZE` call has been added to ensure post-mod indexes are visible to the query planner.
