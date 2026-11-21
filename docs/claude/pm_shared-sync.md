# Findings: `pm_shared` repo <-> IDB sync (2026-11-20)

Repo: `pm_shared/` on branch `retail`. IDB: `demo/dlls/gunman.dll.i64` (the only IDB; retail has none). Direction: repo -> IDB. No repo change was needed. IDA's output was conclusive; Ghidra was not needed (its MCP backend was also unreachable).
Split out of the former combined `common-engine-pmshared-sync.md`; see also `common-sync.md` and `engine-sync.md` (same session, same IDB).

## Scope

Synced everything the server DLL can see: types, functions, globals of the directory. The types it uses from `common/` and `engine/` are in the other two docs.

## Types added (repo text and order; all parsed with 0 errors)

| Header | Types |
|---|---|
| `pm_shared/pm_defs.h`, `pm_movevars.h` | `movevars_t` (0x84), `physent_t` (0xE0), `playermove_t` (0x4F5C0) |
| `pm_shared/pm_shared.c` (file-local) | `modtype_t`, `dclipnode_t`, `mplane_t`, `hull_t` |

## Layout checks against the binary (repo and demo agree)

- `physent_t` stride 0xE0: `add edi, 0E0h` in `PM_Ladder` (`moveents[]` loop).
- `pmtrace_t` stride 0x44 (`touchindex[]`, `PM_AddToTouched`), 68-byte locals in `PM_CheckStuck`/`PM_Duck` (`pmtrace_t` itself is in `common-sync.md`).
- `playermove_t` offsets seen in code: `numtouch` +0x4548C, `physinfo` +0x4F3F0, `runfuncs` +0x4F5B0; every `pmove->` access in the decompiled functions resolves to a field name.
- Gunman's `FL_INVEHICLE` (= `FL_IMMUNE_LAVA`, 0x80000 in `flags` at +0xB8) is tested in `PM_Duck` (twice) and `PM_PlayerMove` (the `MOVETYPE_NOCLIP` case), as in the repo.
- `gFunctionTable` slots 34..36 point at the functions named below by order-matching (`PM_Move`, `PM_Init`, `PM_FindTextureType`), which confirms the mapping independently (table: `engine-sync.md`).

## Functions named and typed (`__cdecl`, C names)

`pm_math.c` (only the functions the server links survive): `AngleVectors` 10040000, `VectorCompare` 10040100, `VectorMA` 10040130, `CrossProduct` 10040170, `Length` 100401B0, `VectorNormalize` 100401E0, `VectorScale` 10040230.

`pm_shared.c`, in source order (matched by order, size, strings and call graph):

| Address | Function | Address | Function |
|---|---|---|---|
| 10040260 | `PM_SwapTextures` | 100432F0 | `PM_Duck` |
| 10040320 | `PM_SortTextures` | 10043690 | `PM_LadderMove` |
| 10040390 | `PM_InitTextureTypes` | 10043A00 | `PM_Ladder` |
| 10040630 | `PM_FindTextureType` | 10043AD0 | `PM_WaterJump` |
| 10040690 | `PM_PlayStepSound` | 10043B90 | `PM_AddGravity` |
| 10040E10 | `PM_MapTextureTypeStepType` | 10043BF0 | `PM_PushEntity` |
| 10040E90 | `PM_CatagorizeTextureType` | 10043CC0 | `PM_Physics_Toss` |
| 10040F90 | `PM_UpdateStepSound` | 10043FD0 | `PM_NoClip` |
| 100412A0 | `PM_AddToTouched` | 10044080 | `PM_Jump` |
| 10041340 | `PM_CheckVelocity` | 100443E0 | `PM_CheckWaterJump` |
| 10041430 | `PM_ClipVelocity` | 10044650 | `PM_CheckFalling` |
| 100414D0 | `PM_AddCorrectGravity` | 10044800 | `PM_PlayWaterSounds` |
| 10041550 | `PM_FixupGravityVelocity` | 100448F0 | `PM_CalcRoll` |
| 100415B0 | `PM_FlyMove` | 10044990 | `PM_DropPunchAngle` |
| 10041A90 | `PM_Accelerate` | 100449E0 | `PM_CheckParamters` |
| 10041B30 | `PM_WalkMove` | 10044C10 | `PM_ReduceTimers` |
| 10042050 | `PM_Friction` | 10044D00 | `PM_PlayerMove` |
| 100421F0 | `PM_AirAccelerate` | 10045150 | `PM_CreateStuckTable` |
| 100422B0 | `PM_WaterMove` | 10045370 | `PM_Move` |
| 100425C0 | `PM_AirMove` | 100453E0 | `PM_Init` |
| 10042700 | `PM_InWater` | 10042720 | `PM_CheckWater` |
| 100428E0 | `PM_CatagorizePosition` | 10042A10 | `PM_GetRandomStuckOffsets` |
| 10042A60 | `PM_ResetStuckOffsets` | 10042A80 | `PM_CheckStuck` |
| 10042E00 | `GetChaseOrigin` | 10042ED0 | `PM_SpectatorMove` |
| 10043230 | `PM_SplineFraction` | 10043250 | `PM_FixPlayerCrouchStuck` |

## Globals named and typed

| Address | Name | Type |
|---|---|---|
| 101244F4 | `vec3_origin` | `vec3_t` |
| 1010EB10 | `nanmask` | `int` |
| 1010EB14 | `PM_CheckWater_current_table` (static local) | `vec3_t[6]` |
| 10124500 | `rgv3tStuckTable` | `vec3_t[54]` |
| 10124788 | `PM_CheckStuck_rgStuckCheckTime` (static local) | `float[32][2]` |
| 10124888 | `rgStuckLast` | `int[32][2]` |
| 10124988 | `grgszTextureName` | `char[512][13]` |
| 10126388 | `grgchTextureType` | `char[512]` |
| 10126588 | `pm_shared_initialized` | `int` |
| 1012658C | `pmove` | `playermove_t *` |
| 10126590 | `gcTextures` | `int` |
| 10126594 | `g_onladder` | `int` |
| 10126598 | `PM_InitTextureTypes_bTextureTypeInit` (static local) | `qboolean` |
| 1012659C | `PM_PlayStepSound_iSkipStep` (static local) | `int` |

Static locals got a `Function_name` prefix because IDA names can't contain `::`. Sizes tile exactly (stuck table 54*12, texture names 512*13), which fixed the boundaries.

## Differences between repo and demo binary

- Not in the demo image (unreferenced, so not linked): `PM_GetInfo`, all of `pm_debug.c` (`PM_ShowClipBox`, `PM_ParticleLine`, `PM_DrawRectangle`, `PM_DrawPhysEntBBox`, `PM_DrawBBox`, `PM_ViewEntity`), and the other `pm_math.c` functions (`anglemod`, `AngleVectorsTranspose`, `AngleMatrix`, `AngleIMatrix`, `VectorTransform`, `_DotProduct`, `_VectorSubtract`, `_VectorAdd`, `_VectorCopy`, `VectorInverse`, `Q_log2`, `VectorMatrix`, `VectorAngles`). Whether retail links `PM_GetInfo` is unknown (no retail IDB).
- `pm_shared.c`'s `CLIENT_DLL` spectator globals (`vecNewViewAngles`, ... `iIsSpectator`) are client-only and absent, as expected.
- Everything else compared (function order, strings, constants, hull/plane structs, jump/duck/ladder logic) matches the repo.

## Open

- `physent_t` keeps `struct model_s *` opaque (the repo only forward-declares it).
- C runtime calls inside `pm_shared` (`stricmp`, `strnicmp`, `isspace`, ...) remain unnamed.
