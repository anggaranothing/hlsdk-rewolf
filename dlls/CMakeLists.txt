# dlls/hl.dsp
#
# Object files are handed to the linker in the order of the sources below, which is the project
# order of the .dsp (it decides code layout). Keep it. placeholder_object1.cpp and
# placeholder_object2.cpp are last on purpose (docs/gunman-chronicles.md).

set(HL_SOURCES
    airtank.cpp
    animating.cpp
    animation.cpp
    bmodels.cpp
    buttons.cpp
    cbase.cpp
    client.cpp
    combat.cpp
    defaultai.cpp
    doors.cpp
    effects.cpp
    explode.cpp
    flyingmonster.cpp
    func_break.cpp
    func_tank.cpp
    game.cpp
    gamerules.cpp
    genericmonster.cpp
    ggrenade.cpp
    globals.cpp
    h_ai.cpp
    h_battery.cpp
    h_cine.cpp
    h_cycler.cpp
    h_export.cpp
    healthkit.cpp
    items.cpp
    lights.cpp
    maprules.cpp
    monstermaker.cpp
    monsters.cpp
    monsterstate.cpp
    mortar.cpp
    multiplay_gamerules.cpp
    nodes.cpp
    pathcorner.cpp
    plane.cpp
    plats.cpp
    player.cpp
    ../pm_shared/pm_debug.c
    ../pm_shared/pm_math.c
    ../pm_shared/pm_shared.c
    schedule.cpp
    scripted.cpp
    singleplay_gamerules.cpp
    skill.cpp
    sound.cpp
    soundent.cpp
    spectator.cpp
    squadmonster.cpp
    subs.cpp
    talkmonster.cpp
    teamplay_gamerules.cpp
    tempmonster.cpp
    triggers.cpp
    turret.cpp
    util.cpp
    weapons.cpp
    world.cpp
    gunman_pending.cpp
    funcwind.cpp
    lava.cpp
    leaf.cpp
    speakers.cpp
    boulder.cpp
    sphereexplosion.cpp
    spritegod.cpp
    volcanospew.cpp
    bird.cpp
    chopper.cpp
    cricket.cpp
    butterfly.cpp
    gator.cpp
    hatchetfish.cpp
    microraptor.cpp
    ourano.cpp
    raptor.cpp
    renesaur.cpp
    scorpion.cpp
    aigirl.cpp
    decoreexplode.cpp
    aicore.cpp
    beamgun.cpp
    dml.cpp
    dmlgrenade.cpp
    dmlrocket.cpp
    fists.cpp
    gausspistol.cpp
    grenade_chem.cpp
    minigun.cpp
    shotgun.cpp
    spchemicalgun.cpp
    vehicletank.cpp
    vehicletankbsp.cpp
    cluster.cpp
    darttrap.cpp
    monstertank.cpp
    meteor.cpp
    bandit.cpp
    demoman.cpp
    friendlygunman.cpp
    friendlyunarmed.cpp
    banditgrunt.cpp
    hostilescientist.cpp
    rustflier.cpp
    rust.cpp
    rustbit.cpp
    rustbot.cpp
    gunner.cpp
    beak.cpp
    critter.cpp
    maggot.cpp
    tube.cpp
    tubequeen.cpp
    tuberocket.cpp
    xenome.cpp
    camera.cpp
    decore.cpp
    hologram.cpp
    torch.cpp
    endboss.cpp
    scientist.cpp
    placeholder_object1.cpp
    placeholder_object2.cpp
)

set(HL_HEADERS
    activity.h
    activitymap.h
    animation.h
    basemonster.h
    cbase.h
    cdll_dll.h
    client.h
    decals.h
    defaultai.h
    doors.h
    effects.h
    ../engine/eiface.h
    enginecallback.h
    explode.h
    extdll.h
    flyingmonster.h
    func_break.h
    gamerules.h
    hornet.h
    items.h
    monsterevent.h
    monsters.h
    nodes.h
    plane.h
    player.h
    ../pm_shared/pm_debug.h
    ../pm_shared/pm_defs.h
    ../pm_shared/pm_info.h
    ../pm_shared/pm_materials.h
    ../pm_shared/pm_movevars.h
    ../pm_shared/pm_shared.h
    saverestore.h
    schedule.h
    scripted.h
    scriptevent.h
    skill.h
    soundent.h
    spectator.h
    squadmonster.h
    talkmonster.h
    teamplay_gamerules.h
    trains.h
    util.h
    vector.h
    weapons.h
)

add_library(hl SHARED ${HL_SOURCES} ${HL_HEADERS})

# The groups of the .dsp, for the IDE.
source_group("Source Files" FILES ${HL_SOURCES})
source_group("Header Files" FILES ${HL_HEADERS})

# ---- compiler ---------------------------------------------------------------------------------
# /G5 (optimise for the Pentium) is VC6's; later compilers dropped it. hl.dsp has no /GX: the
# server is built without C++ exception handling, and VC6 has no RTTI unless asked, which /GR- says.
if(GUNMAN_VC6)
  set(_g5 /G5)
  set(_common /W3)
else()
  set(_g5 "")
  set(_common /W3 /GR-)
endif()

set(HL_OPTIONS_RELEASE ${_g5} /Zi /O2)
set(HL_OPTIONS_DEBUG   ${_g5} /ZI /Od)
set(HL_OPTIONS_GERMANY /O2)
set(HL_OPTIONS_PROFILE ${_g5} /Zi /O2)

target_compile_options(hl PRIVATE
  ${_common}
  "$<$<CONFIG:Release>:${HL_OPTIONS_RELEASE}>"
  "$<$<CONFIG:Debug>:${HL_OPTIONS_DEBUG}>"
  "$<$<CONFIG:Germany>:${HL_OPTIONS_GERMANY}>"
  "$<$<CONFIG:Profile>:${HL_OPTIONS_PROFILE}>"
  ${GUNMAN_EXTRA_COMPILE_OPTIONS})

target_compile_definitions(hl PRIVATE
  WIN32
  _WINDOWS
  QUIVER
  VOXEL
  QUAKE2
  VALVE_DLL
  "$<$<CONFIG:Release,Germany,Profile>:NDEBUG>"
  "$<$<CONFIG:Debug>:_DEBUG>"
  "$<$<CONFIG:Debug>:CLIENT_WEAPONS>"
  "$<$<CONFIG:Germany>:GERMANY>")

# ..\dlls ..\engine ..\common ..\pm_shared ..\game_shared (this tree has no game_shared), and in
# Debug also ..\ (the root of the tree).
set(HL_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/engine
  ${PROJECT_SOURCE_DIR}/common
  ${PROJECT_SOURCE_DIR}/pm_shared)
if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/game_shared)
  list(APPEND HL_INCLUDES ${PROJECT_SOURCE_DIR}/game_shared)
endif()
target_include_directories(hl PRIVATE
  ${HL_INCLUDES}
  "$<$<CONFIG:Debug>:${PROJECT_SOURCE_DIR}>")

# ---- linker -----------------------------------------------------------------------------------
# The import libraries are those of each configuration's "ADD LINK32" line, in that order.
set(HL_LIBS_RELEASE kernel32 user32 gdi32 winspool comdlg32 advapi32 shell32 ole32 oleaut32 uuid)
set(HL_LIBS_DEBUG   user32 advapi32)
set(HL_LIBS_GERMANY kernel32 user32 gdi32 winspool comdlg32 advapi32 shell32 ole32 oleaut32 uuid odbc32 odbccp32)
set(HL_LIBS_PROFILE kernel32 user32 gdi32 winspool comdlg32 advapi32 shell32 ole32 oleaut32 uuid)

target_link_libraries(hl PRIVATE
  "$<$<CONFIG:Release>:${HL_LIBS_RELEASE}>"
  "$<$<CONFIG:Debug>:${HL_LIBS_DEBUG}>"
  "$<$<CONFIG:Germany>:${HL_LIBS_GERMANY}>"
  "$<$<CONFIG:Profile>:${HL_LIBS_PROFILE}>")

# hl.def holds `LIBRARY hl`; a Release link to gunman.dll therefore warns LNK4070, as the IDE's does.
target_link_options(hl PRIVATE
  /subsystem:windows
  "/def:${CMAKE_CURRENT_SOURCE_DIR}/hl.def"
  "$<$<CONFIG:Release>:/map;/debug;/incremental:no>"
  "$<$<CONFIG:Debug>:/debug>"
  "$<$<CONFIG:Profile>:/profile;/debug>")

# ---- output: the .dsp's folders and names -----------------------------------------------------
# Release links to gunman.dll (its /out); the other configurations have no /out and keep the
# project's name, hl.dll. Debug writes its import library to Debug\hl.lib.
set(_out ${CMAKE_CURRENT_BINARY_DIR})
# DEFINE_SYMBOL: CMake would add -Dhl_EXPORTS to every shared library; the .dsp defines no such symbol.
set_target_properties(hl PROPERTIES
  PREFIX ""
  DEFINE_SYMBOL ""
  OUTPUT_NAME hl
  OUTPUT_NAME_RELEASE gunman
  PDB_NAME_RELEASE gunman
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_out}/Releasehl"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${_out}/debughl"
  RUNTIME_OUTPUT_DIRECTORY_GERMANY "${_out}/Germany"
  RUNTIME_OUTPUT_DIRECTORY_PROFILE "${_out}/Profilehl"
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${_out}/Releasehl"
  ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${_out}/Debug"
  ARCHIVE_OUTPUT_DIRECTORY_GERMANY "${_out}/Germany"
  ARCHIVE_OUTPUT_DIRECTORY_PROFILE "${_out}/Profilehl"
  LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/hl.def")

# ---- /YX, as the .dsp files have it (opt-in) --------------------------------------------------
# One .pch for the whole build, and for five files in Release a .pch of their own that a full
# build starts afresh (stand-ins for build history; see "Build stand-ins" in
# docs/gunman-chronicles.md). microraptor.cpp also names schedule.h as its "through header".
if(GUNMAN_VC6_AUTO_PCH)
  target_compile_options(hl PRIVATE /YX "/Fp$<TARGET_FILE_DIR:hl>/vc60.pch")
  foreach(_name ggrenade microraptor demoman friendlygunman banditgrunt)
    set(_pch "$<$<CONFIG:Release>:/Fp$<TARGET_FILE_DIR:hl>/${_name}.pch>")
    if(_name STREQUAL "microraptor")
      list(APPEND _pch "$<$<CONFIG:Release>:/YXschedule.h>")
    endif()
    set_source_files_properties(${_name}.cpp PROPERTIES COMPILE_OPTIONS "${_pch}")
  endforeach()
endif()
