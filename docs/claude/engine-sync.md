# Findings: `engine` repo <-> IDB sync (2026-11-20)

Repo: `engine/` on branch `retail`. IDB: `demo/dlls/gunman.dll.i64` (the only IDB; retail has none). Direction: repo -> IDB. No repo change was needed. IDA's output was conclusive; Ghidra was not needed (its MCP backend was also unreachable).
Split out of the former combined `common-engine-pmshared-sync.md`; see also `common-sync.md` and `pm_shared-sync.md` (same session, same IDB).

## Scope

Synced everything the server DLL can see: types of the directory and the entity API table.
Not synced (no consumer in the server IDB, and no client IDB exists): `engine/cdll_int.h`, `engine/keydefs.h`, `engine/Anorms.h`.
Already done in earlier syncs and left unchanged (see `server/h_export-sync.md`): `ALERT_TYPE`, `PRINT_TYPE`, `FORCE_TYPE`, `enginefuncs_t`, `globalvars_t`, `entvars_t` (re-checked: identical to repo, 0x2A4), `TraceResult`, `KeyValueData`, `TYPEDESCRIPTION`/`FIELDTYPE`, `g_engfuncs`, `gpGlobals`.

## Types added (repo text and order; all parsed with 0 errors)

| Header | Types |
|---|---|
| `engine/edict.h` | `edict_s`/`edict_t` completed from a forward declaration to 0x324 (the known Half-Life size) |
| `engine/custom.h` | `resourcetype_t`, `_resourceinfo_t`, `resourceinfo_t`, `resource_t` (0x88), `customization_t` (0xA4) |
| `engine/eiface.h` | `CDStatus`, `LEVELLIST` (0x50), `ENTITYTABLE` (0x18), `saverestore_s`/`SAVERESTOREDATA` (0x574), `DLL_FUNCTIONS` (0xC8, 50 slots), `NEW_DLL_FUNCTIONS`, `NEW_DLL_FUNCTIONS_FN`, `APIFUNCTION`, `APIFUNCTION2` |
| `engine/Progs.h`, `engine/shake.h` | `event_info_t` (0x58), `event_state_t` (0x1600), `ScreenShake`, `ScreenFade` |
| `engine/studio.h` | `studiohdr_t` (0xF4), `studioseqhdr_t`, `mstudiobone_t`, `mstudiobonecontroller_t`, `mstudiobbox_t`, `cache_user_t`, `mstudioseqgroup_t`, `mstudioseqdesc_t` (0xB0), `mstudiopivot_t`, `mstudioattachment_t`, `mstudioanim_t`, `mstudioanimvalue_t`, `mstudiobodyparts_t`, `mstudiotexture_t`, `mstudiomodel_t`, `mstudiomesh_t` |

Anonymous `typedef struct/enum {..} X` types appear twice in the type list (`$hash` and `X`), as in the earlier syncs.

## Entity API (named and typed)

`GetEntityAPI` 10006EE0 (`int __cdecl(DLL_FUNCTIONS *, int)`), `GetEntityAPI2` 10006F10 (`int __cdecl(DLL_FUNCTIONS *, int *)`), table `gFunctionTable` 101041F0 typed `DLL_FUNCTIONS`. Both exports check version 140 (`INTERFACE_VERSION`; the repo header gives 001 only under `HLDEMO_BUILD`).
`gFunctionTable` slots 34..36 point at `PM_Move`, `PM_Init`, `PM_FindTextureType` (matched by order), which confirms the `pm_shared` function mapping independently (see `pm_shared-sync.md`).

## Open

- `edict_t`-typed members now have fields, but `model_s`, `delta_s` and `hull_s` pointers elsewhere stay opaque or forward declarations where the repo only forward-declares them.
- `gFunctionTable` targets (`DispatchSpawn`, `ClientConnect`, `StartFrame`, ...) are `dlls/` code and still `sub_*`; they could be named and typed from `DLL_FUNCTIONS` next. Slots 26, 33 and 47 (`ParmsNewLevel`, `Sys_Error`, `CreateInstancedBaselines`) all point at one shared empty function, currently labelled `?Smack@CFists@@QAEXXZ`.
- `gmsgShake`/`gmsgFade` (declared in `shake.h`, defined in `dlls/`) not touched.
