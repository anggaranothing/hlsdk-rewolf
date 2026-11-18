# cl_dll/cl_dll.dsp
#
# Object files are handed to the linker in the order of the sources below: the "hl" group of the
# .dsp first, then the rest (the project order of the .dsp, which decides code layout). Keep it.
# placeholder_object.cpp sits between vgui_HudDeathNotice.cpp and vgui_HudID.cpp on purpose
# (docs/gunman-chronicles.md).

# The "hl" group: the client-side weapon code, which includes files of ..\dlls.
set(CL_HL_SOURCES
    dmlcross.cpp
    ev_hldm.cpp
    ../dlls/beamgun.cpp
    ../dlls/dml.cpp
    ../dlls/dmlgrenade.cpp
    ../dlls/fists.cpp
    ../dlls/gausspistol.cpp
    ../dlls/minigun.cpp
    ../dlls/shotgun.cpp
    ../dlls/spchemicalgun.cpp
    hl/hl_baseentity.cpp
    hl/hl_events.cpp
    hl/hl_objects.cpp
    hl/hl_weapons.cpp
    zoom.cpp
)

set(CL_SOURCES
    ammo.cpp
    ammo_secondary.cpp
    ammohistory.cpp
    battery.cpp
    cdll_int.cpp
    com_weapons.cpp
    death.cpp
    demo.cpp
    entity.cpp
    ev_common.cpp
    events.cpp
    flashlight.cpp
    geiger.cpp
    health.cpp
    hud.cpp
    hud_msg.cpp
    hud_redraw.cpp
    hud_servers.cpp
    hud_update.cpp
    in_camera.cpp
    input.cpp
    inputw32.cpp
    menu.cpp
    message.cpp
    parsemsg.cpp
    ../pm_shared/pm_debug.c
    ../pm_shared/pm_math.c
    ../pm_shared/pm_shared.c
    saytext.cpp
    status_icons.cpp
    statusbar.cpp
    tank.cpp
    text_message.cpp
    train.cpp
    tri.cpp
    util.cpp
    vgui_AmmoPanel.cpp
    vgui_ClassMenu.cpp
    vgui_ConsolePanel.cpp
    vgui_ControlConfigPanel.cpp
    vgui_CustomizationMenu.cpp
    vgui_CustomObjects.cpp
    vgui_HudDeathNotice.cpp
    placeholder_object.cpp
    vgui_HudID.cpp
    vgui_HudWeaponStatus.cpp
    vgui_int.cpp
    vgui_MOTDWindow.cpp
    vgui_SchemeManager.cpp
    vgui_ScorePanel.cpp
    vgui_ServerBrowser.cpp
    vgui_TeamFortressViewport.cpp
    vgui_teammenu.cpp
    vgui_Temperature.cpp
    view.cpp
    parsemsg.h
)

# cl_dll.dsp also lists .\util.h, which is not in this tree (hl/hl_baseentity.cpp and
# hl/hl_weapons.cpp include "util.h" and find ..\dlls\util.h), so it is left out: CMake does
# not accept a source file that is missing.
set(CL_HEADERS
    ammo.h
    ammohistory.h
    camera.h
    cl_dll.h
    com_weapons.h
    demo.h
    ev_hldm.h
    eventscripts.h
    health.h
    hud.h
    hud_iface.h
    hud_servers.h
    hud_servers_priv.h
    in_defs.h
    kbutton.h
    ../pm_shared/pm_debug.h
    ../pm_shared/pm_defs.h
    ../pm_shared/pm_info.h
    ../pm_shared/pm_materials.h
    ../pm_shared/pm_movevars.h
    ../pm_shared/pm_shared.h
    util_vector.h
    vgui_ConsolePanel.h
    vgui_ControlConfigPanel.h
    vgui_CustomizationMenu.h
    vgui_HudDeathNotice.h
    vgui_HudID.h
    vgui_HudWeaponStatus.h
    vgui_int.h
    vgui_SchemeManager.h
    vgui_ScorePanel.h
    vgui_ServerBrowser.h
    vgui_TeamFortressViewport.h
    vgui_Temperature.h
    view.h
    wrect.h
)

add_library(cl_dll SHARED ${CL_HL_SOURCES} ${CL_SOURCES} ${CL_HEADERS})

# The groups of the .dsp, for the IDE. (parsemsg.h is in the "Source Files" group there.)
source_group("Source Files\\hl" FILES ${CL_HL_SOURCES})
source_group("Source Files" FILES ${CL_SOURCES})
source_group("Header Files" FILES ${CL_HEADERS})

# ---- per-file settings ------------------------------------------------------------------------
# /D "CLIENT_DLL" for these thirteen files, in both configurations. The other files, pm_shared.c
# included, are built without it.
set_source_files_properties(
    ev_hldm.cpp
    ../dlls/beamgun.cpp
    ../dlls/dml.cpp
    ../dlls/dmlgrenade.cpp
    ../dlls/fists.cpp
    ../dlls/gausspistol.cpp
    ../dlls/minigun.cpp
    ../dlls/shotgun.cpp
    ../dlls/spchemicalgun.cpp
    hl/hl_baseentity.cpp
    hl/hl_events.cpp
    hl/hl_objects.cpp
    hl/hl_weapons.cpp
  PROPERTIES COMPILE_DEFINITIONS CLIENT_DLL)

# ---- compiler ---------------------------------------------------------------------------------
# /G5 is VC6's (Debug only here). /GX turns on C++ exception handling; later compilers spell it
# /EHsc. VC6 has no RTTI unless asked, which /GR- says elsewhere.
if(GUNMAN_VC6)
  set(_g5 /G5)
  set(_eh /GX)
  set(_common /W3)
else()
  set(_g5 "")
  set(_eh /EHsc)
  set(_common /W3 /GR-)
endif()

set(CL_OPTIONS_RELEASE ${_eh} /Zi /O2)
set(CL_OPTIONS_DEBUG   ${_g5} ${_eh} /ZI /Od)

# Germany and Profile do not exist in cl_dll.dsp: they take the Release settings.
target_compile_options(cl_dll PRIVATE
  ${_common}
  "$<$<CONFIG:Release,Germany,Profile>:${CL_OPTIONS_RELEASE}>"
  "$<$<CONFIG:Debug>:${CL_OPTIONS_DEBUG}>"
  ${GUNMAN_EXTRA_COMPILE_OPTIONS})

target_compile_definitions(cl_dll PRIVATE
  WIN32
  _WINDOWS
  "$<$<CONFIG:Release,Germany,Profile>:NDEBUG>"
  "$<$<CONFIG:Debug>:_DEBUG>"
  "$<$<CONFIG:Debug>:_MBCS>")

# ..\engine ..\common ..\pm_shared ..\utils\vgui\include ..\dlls
target_include_directories(cl_dll PRIVATE
  ${PROJECT_SOURCE_DIR}/engine
  ${PROJECT_SOURCE_DIR}/common
  ${PROJECT_SOURCE_DIR}/pm_shared
  ${PROJECT_SOURCE_DIR}/utils/vgui/include
  ${PROJECT_SOURCE_DIR}/dlls)

# ---- linker -----------------------------------------------------------------------------------
add_library(vgui STATIC IMPORTED)
set_target_properties(vgui PROPERTIES
  IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/utils/vgui/lib/win32_vc6/vgui.lib")

# The import libraries are those of each configuration's "ADD LINK32" line, in that order.
set(CL_LIBS_RELEASE kernel32 gdi32 winspool comdlg32 advapi32 shell32 ole32 winmm vgui wsock32 user32)
set(CL_LIBS_DEBUG   oleaut32 uuid odbc32 odbccp32 kernel32 user32 gdi32 winspool comdlg32 advapi32 shell32 ole32 winmm vgui wsock32)

target_link_libraries(cl_dll PRIVATE
  "$<$<CONFIG:Release,Germany,Profile>:${CL_LIBS_RELEASE}>"
  "$<$<CONFIG:Debug>:${CL_LIBS_DEBUG}>")

# Release links with /map and without /debug (so no PDB record, and identical functions are
# folded); Debug links with /debug.
target_link_options(cl_dll PRIVATE
  /subsystem:windows
  "$<$<CONFIG:Release,Germany,Profile>:/map>"
  "$<$<CONFIG:Debug>:/debug>")

# ---- output: the .dsp's folders and names -----------------------------------------------------
set(_out ${CMAKE_CURRENT_BINARY_DIR})
# DEFINE_SYMBOL: CMake would add -Dcl_dll_EXPORTS to every shared library; the .dsp defines no such symbol.
set_target_properties(cl_dll PROPERTIES
  PREFIX ""
  DEFINE_SYMBOL ""
  OUTPUT_NAME client
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_out}/Release"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${_out}/Debug"
  RUNTIME_OUTPUT_DIRECTORY_GERMANY "${_out}/Germany"
  RUNTIME_OUTPUT_DIRECTORY_PROFILE "${_out}/Profile"
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${_out}/Release"
  ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${_out}/Debug"
  ARCHIVE_OUTPUT_DIRECTORY_GERMANY "${_out}/Germany"
  ARCHIVE_OUTPUT_DIRECTORY_PROFILE "${_out}/Profile")

# The "Custom Build" step of the .dsp: copy client.dll into a game folder (cl_dlls).
if(GUNMAN_CLIENT_COPY_DIR)
  add_custom_command(TARGET cl_dll POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:cl_dll>" "${GUNMAN_CLIENT_COPY_DIR}"
    COMMENT "Copying client.dll to ${GUNMAN_CLIENT_COPY_DIR}")
endif()

# ---- /YX, as the .dsp files have it (opt-in) --------------------------------------------------
# One .pch for the whole build, and for four weapon files in Release a .pch of their own (see
# "Build stand-ins" in docs/gunman-chronicles.md).
if(GUNMAN_VC6_AUTO_PCH)
  target_compile_options(cl_dll PRIVATE /YX "/Fp$<TARGET_FILE_DIR:cl_dll>/vc60.pch")
  foreach(_name gausspistol minigun shotgun spchemicalgun)
    set_source_files_properties(../dlls/${_name}.cpp PROPERTIES
      COMPILE_OPTIONS "$<$<CONFIG:Release>:/Fp$<TARGET_FILE_DIR:cl_dll>/${_name}.pch>")
  endforeach()
endif()
