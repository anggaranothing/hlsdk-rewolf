# CMake build for the two Visual C++ 6.0 projects of this tree.
#
#   dlls/hl.dsp        ->  target `hl`      server DLL (Release: gunman.dll, other configurations: hl.dll)
#   cl_dll/cl_dll.dsp  ->  target `cl_dll`  client DLL (client.dll)
#
# Every compiler and linker option of the .dsp files is carried over, per configuration; the
# differences are listed at the end of this comment. The .dsp and .dsw files stay as they are.
#
# Configurations of hl.dsp: Release, Debug, Germany, Profile. cl_dll.dsp has Release and Debug only;
# the client builds its Germany and Profile configurations with the Release settings.
#
# Configure outside the source tree (there is no .gitignore):
#
#   Visual C++ 6.0, from a prompt that ran VCVARS32.BAT:
#     cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -S . -B build
#     cmake --build build
#   Later Visual Studio, 32-bit only:
#     cmake -G "Visual Studio 17 2022" -A Win32 -S . -B build
#     cmake --build build --config Release
#   Ninja Multi-Config works too, from an x86 prompt; Germany and Profile are configurations like the others.
#
# Differences from the .dsp files:
#   * IDE-only options are dropped: /FD (minimal rebuild), /Fr and /FR (browse info), /Gm.
#     /YX (VC6's automatic precompiled headers) is off unless GUNMAN_VC6_AUTO_PCH is set.
#   * CMake's own default flags are cleared (see below), so CMAKE_C_FLAGS, CMAKE_CXX_FLAGS and the
#     CMAKE_*_FLAGS_<CONFIG> variables have no effect. Use GUNMAN_EXTRA_COMPILE_OPTIONS instead.
#   * /nologo, /dll, /machine, /out, /implib and /pdb come from CMake.
#   * The "Custom Build" steps that copied client.dll to \quiver\valve\cl_dlls (Release) and
#     \half-life\mp\cl_dlls (Debug) are replaced by GUNMAN_CLIENT_COPY_DIR.
#   * Visual Studio 2005 and later do not know /G5 and /YX, and spell /GX /EHsc. Those are
#     translated; nothing else is, so the code may need changes to compile there.
#
cmake_minimum_required(VERSION 3.20)

project(hlsdk-rewolf LANGUAGES C CXX)

if(NOT MSVC)
  message(FATAL_ERROR "These projects are for the Microsoft compiler (Visual C++ 6.0 or later).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "The game DLLs are 32-bit: use an x86 toolchain (-A Win32 with a Visual Studio generator).")
endif()

# Visual C++ 6.0 is _MSC_VER 1200.
if(MSVC_VERSION LESS 1300)
  set(GUNMAN_VC6 ON)
else()
  set(GUNMAN_VC6 OFF)
endif()

option(GUNMAN_VC6_AUTO_PCH
  "Pass /YX (VC6's automatic precompiled headers) as the .dsp files do, with their per-file /Fp. It changes code layout; needs VC6, a full serial build in project order (NMake Makefiles), and no parallel jobs" OFF)
set(GUNMAN_CLIENT_COPY_DIR "" CACHE PATH
  "If set, client.dll is copied here after each build (the .dsp's \"Custom Build\" step)")
set(GUNMAN_EXTRA_COMPILE_OPTIONS "" CACHE STRING
  "Extra cl.exe options for every source of both DLLs, as a ;-separated list (build_dsp.py's --cpp-add)")

if(GUNMAN_VC6_AUTO_PCH AND NOT GUNMAN_VC6)
  message(WARNING "GUNMAN_VC6_AUTO_PCH needs Visual C++ 6.0 (/YX no longer exists): ignored.")
  set(GUNMAN_VC6_AUTO_PCH OFF)
endif()

# ---- configurations ---------------------------------------------------------------------------
set(GUNMAN_CONFIGS Release Debug Germany Profile)
get_property(_gunman_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_gunman_multi_config)
  set(CMAKE_CONFIGURATION_TYPES "${GUNMAN_CONFIGS}" CACHE STRING "Build configurations (those of hl.dsp)" FORCE)
else()
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
  endif()
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${GUNMAN_CONFIGS})
  string(TOLOWER "${GUNMAN_CONFIGS}" _gunman_known)
  string(TOLOWER "${CMAKE_BUILD_TYPE}" _gunman_asked)
  if(NOT _gunman_asked IN_LIST _gunman_known)
    message(FATAL_ERROR "CMAKE_BUILD_TYPE must be one of: ${GUNMAN_CONFIGS}")
  endif()
endif()

# ---- CMake's defaults out of the way ----------------------------------------------------------
# CMake seeds its flag variables for every MSVC (for VC6: /Zm1000 /GX /Ob2 /MD, and odbc32.lib and
# odbccp32.lib on every link). They change code generation and the import table, and the .dsp files
# already list every option, so the targets set all of them and these start empty.
foreach(_lang C CXX)
  set(CMAKE_${_lang}_FLAGS "")
  set(CMAKE_${_lang}_STANDARD_LIBRARIES "")
  foreach(_cfg DEBUG RELEASE MINSIZEREL RELWITHDEBINFO)
    set(CMAKE_${_lang}_FLAGS_${_cfg} "")
  endforeach()
endforeach()
foreach(_cfg DEBUG RELEASE MINSIZEREL RELWITHDEBINFO)
  set(CMAKE_SHARED_LINKER_FLAGS_${_cfg} "")
endforeach()
# (CMAKE_SHARED_LINKER_FLAGS itself holds /machine:X86 or I386 and stays.)

# /MT, or /MTd in Debug: the static C runtime, as in every configuration of both .dsp files.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(dlls)
add_subdirectory(cl_dll)
