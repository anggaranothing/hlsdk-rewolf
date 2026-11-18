# Findings: .dsp to CMake conversion (2026-11-17)

Task: convert `dlls/hl.dsp` and `cl_dll/cl_dll.dsp` (MSVC 6.0) to CMake.

## Delivered

| File | Content |
|---|---|
| `CMakeLists.txt` | Checks (MSVC, 32-bit), configurations, CMake default flags cleared, `/MT` runtime, options |
| `dlls/CMakeLists.txt` | Target `hl` (server): Release `gunman.dll`, other configurations `hl.dll` |
| `cl_dll/CMakeLists.txt` | Target `cl_dll` (client): `client.dll` |

The source lists were generated from the `.dsp` files by a script (project order kept, nothing typed by hand). The `.dsp`/`.dsw` files are untouched.

## Why CMake's defaults are cleared

CMake seeds MSVC builds with flags the `.dsp` files do not have. For VC6 (`MSVC_VERSION` 1200) that is `/Zm1000`, `/GX`, `/Ob2` (Release), `/MD`, `-GZ` (Debug), `/INCREMENTAL` and `/pdbtype:sept` (Debug link), and `odbc32.lib odbccp32.lib` on every link. These change code generation and the import table, which matters for a byte-matching project (docs/compilers.md notes that `/Ob0` alone changed T2DEF's client). So `CMAKE_C/CXX_FLAGS*`, `*_STANDARD_LIBRARIES` and `CMAKE_SHARED_LINKER_FLAGS_<CONFIG>` are set empty and every option comes from the targets. Consequence: `-DCMAKE_CXX_FLAGS=...` is ignored; use `-DGUNMAN_EXTRA_COMPILE_OPTIONS=...` (like `build_dsp.py --cpp-add`).

## Mapping

| `.dsp` | CMake |
|---|---|
| Configurations hl: Release, Debug, Germany, Profile | Same four (`CMAKE_CONFIGURATION_TYPES`, or `CMAKE_BUILD_TYPE`) |
| Configurations cl_dll: Release, Debug | Same two; Germany and Profile use Release settings (no `.dsp` equivalent) |
| `/MT` `/MTd` | `CMAKE_MSVC_RUNTIME_LIBRARY` = `MultiThreaded$<$<CONFIG:Debug>:Debug>` |
| `/G5 /W3 /Zi /ZI /O2 /Od /GX` | Compile options per configuration |
| `/D ...`, `/I ...` | `target_compile_definitions`, `target_include_directories` (same order) |
| `ADD LINK32` libraries | `target_link_libraries`, same order per configuration; `vgui.lib` is an imported target |
| `/subsystem:windows /map /debug /profile /incremental:no /def:` | `target_link_options` per configuration |
| `/out:".\Releasehl\gunman.dll"` | `OUTPUT_NAME_RELEASE gunman`, `RUNTIME_OUTPUT_DIRECTORY_*` = `Releasehl`, `debughl`, `Germany`, `Profilehl` (client: `Release`, `Debug`, ...) |
| Per-file `/D "CLIENT_DLL"` (13 client files) | `set_source_files_properties(... COMPILE_DEFINITIONS CLIENT_DLL)` in `cl_dll/` only |
| Per-file `/Fp` and `/YX"schedule.h"` | Only with `-DGUNMAN_VC6_AUTO_PCH=ON` |
| Custom Build copy of `client.dll` | `-DGUNMAN_CLIENT_COPY_DIR=<dir>` (the `.dsp`'s `\quiver\valve\cl_dlls` and `\half-life\mp\cl_dlls` are not kept) |

Dropped on purpose: `/FD`, `/Fr`, `/FR`, `/Gm` (IDE bookkeeping, as `build_dsp.py` drops them), `/nologo`, `/dll`, `/machine`, `/implib` (CMake supplies them). Also suppressed: CMake's `-D<target>_EXPORTS`.

Per-configuration differences kept: server Debug alone defines `CLIENT_WEAPONS` and adds `..\` (tree root) to the includes; Germany alone defines `GERMANY` and links `odbc32.lib odbccp32.lib`; client Debug alone defines `_MBCS`; server Germany has no `/G5` and no `/Zi`.

## Findings in the tree

- `cl_dll.dsp` lists `.\util.h` (Header Files). It does not exist in `cl_dll/` (`hl/hl_baseentity.cpp` and `hl/hl_weapons.cpp` include `"util.h"` and get `dlls/util.h` through `/I ..\dlls`). CMake rejects missing sources, so it is left out. No other dangling references: all 169 entries of `hl.dsp` (124 sources, 45 headers) and 107 of the 108 entries of `cl_dll.dsp` exist.
- `..\game_shared` is in the server's include list but the directory does not exist in this tree. It is added only if present.
- `hl.def` says `LIBRARY hl`, so a Release link to `gunman.dll` warns LNK4070 (as in the IDE build, per docs).
- `parsemsg.h` sits in the client's "Source Files" group in the `.dsp`; kept in that group.

## Validation done (Linux, no MSVC available)

Cloned `anggaranothing/hlsdk-rewolf` branch `retail` (`fd0e4f5`) and configured the three files against it with a faked `MSVC` (`CMAKE_PROJECT_INCLUDE`), then read the generated Ninja/Makefile commands:

- VC6 mode (`MSVC_VERSION` 1200), Ninja Multi-Config: compile flags, defines, include order, link libraries and options match the `.dsp` for all 4 + 2 configurations; link order equals project order; `CLIENT_DLL` on exactly 13 client files in each configuration and not on the server's copies of the same sources; `hl.def` is a link dependency.
- Modern mode (1930): `/G5` dropped, `/GX` becomes `/EHsc`, `/GR-` added.
- `GUNMAN_VC6_AUTO_PCH`, `GUNMAN_EXTRA_COMPILE_OPTIONS`, `GUNMAN_CLIENT_COPY_DIR`, a warning when `/YX` is requested on a modern compiler, and single-config `CMAKE_BUILD_TYPE` handling (default Release, `Germany` works, invalid value is rejected).

## Not verified

- No real compile or link with cl.exe (VC6 or later) was run. Not tested: output folders and file names on Windows (`Releasehl\gunman.dll`), `/def:` handling by the VS generator, `CMAKE_MSVC_RUNTIME_LIBRARY` with VC6's cl.exe, and whether the code compiles with a later MSVC.
- Byte-identical output is not claimed. `/YX` replay needs a full serial build in project order (`NMake Makefiles`); `tools/build_dsp.py` remains the reference for matching retail. CMake still adds `CMAKE_INTDIR` to the defines under multi-config generators (harmless unless used).
- Case: the `.dsp` folder is `debughl` (lowercase); kept.
