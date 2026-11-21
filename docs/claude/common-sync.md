# Findings: `common` repo <-> IDB sync (2026-11-20)

Repo: `common/` on branch `retail`. IDB: `demo/dlls/gunman.dll.i64` (the only IDB; retail has none). Direction: repo -> IDB. No repo change was needed. IDA's output was conclusive; Ghidra was not needed (its MCP backend was also unreachable).
Split out of the former combined `common-engine-pmshared-sync.md`; see also `engine-sync.md` and `pm_shared-sync.md` (same session, same IDB).

## Scope

Synced everything the server DLL can see: types of the directory.
Not synced (no consumer in the server IDB, and no client IDB exists): client/launcher-only headers `common/{beamdef,cl_entity,con_nprint,demo_api,dlight,dll_state,engine_launcher_api,event_api,exefuncs,net_api,particledef,qfont,ref_params,r_efx,screenfade,triangleapi,winsani_*}.h`. Preprocessor constants (`FL_*`, `MOVETYPE_*`, `TE_*`, `IN_*`, ...) were not turned into IDB enums; only the real enums were imported.
Already done in earlier syncs and left unchanged: `CRC32_t`, `cvar_t` (see `server/h_export-sync.md`).

## Types added (repo text and order; all parsed with 0 errors)

| Header | Types |
|---|---|
| `common/const.h` | `func_t`, `word`, `qboolean` (`int`, the C++ form), `color24`, `colorVec`, `PackedColorVec` (pack 2), `link_s`/`link_t`, `plane_t`, `trace_t` (0x38), anonymous enums for `BEAM_*`, `kRender*`, `kRenderFx*` |
| `common/mathlib.h` | `vec4_t`, `vec5_t`, `vec_s_t`, `vec3s_t`, `vec4s_t`, `vec5s_t`, `fixed4/8/16_t`, `DLONG` |
| `common/entity_state.h` | `entity_state_s`/`_t` (0x154; was a forward declaration), `clientdata_t` (0x1DC), `local_state_t` (0xE30) |
| `common/{usercmd,weaponinfo,pmtrace,netadr,event_args}.h` | `usercmd_t` (0x34), `weapon_data_t` (0x58), `pmplane_t`, `pmtrace_t` (0x44), `netadrtype_t`, `netadr_t` (0x14), `event_args_t` (0x48) |
| `common/studio_event.h` | `mstudioevent_t` (0x4C) |

Anonymous `typedef struct/enum {..} X` types appear twice in the type list (`$hash` and `X`), as in the earlier syncs.

## Layout checks against the binary (repo and demo agree)

- `pmtrace_t` stride 0x44 (`touchindex[]`, `PM_AddToTouched`), 68-byte locals in `PM_CheckStuck`/`PM_Duck` (see `pm_shared-sync.md`).
- Gunman's `FL_INVEHICLE` (= `FL_IMMUNE_LAVA`, 0x80000 in `flags` at +0xB8) is tested in `PM_Duck` (twice) and `PM_PlayerMove` (the `MOVETYPE_NOCLIP` case), as in the repo's `common/const.h`.

## Open

- `common/mathlib.h` functions and `vec3_origin`/`nanmask` live in `pm_shared/pm_math.c`; they are in `pm_shared-sync.md`.
