/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//=========================================================
// the tank's brush parts (vehicle_tank_body, _turret, _barrel),
// the triggers that react to the tank, and the shells and rockets
// it fires. Retail compiled them apart from vehicletank.cpp: its
// data follows vehicletank.obj's strings. The file name is ours.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "effects.h"
#include "soundent.h"
#include "gamerules.h"
#include "plane.h"
#include "animation.h"
#include "decals.h"
#include "vehicletank.h"
#include "tankshots.h"

extern int gmsgTank;

//=========================================================
// vehicle_tank_barrel, vehicle_tank_turret, vehicle_tank_body
//=========================================================
TYPEDESCRIPTION	CVehicleTankBSP::m_SaveData[] =
{
	DEFINE_FIELD( CVehicleTankBSP, m_pPlayer, FIELD_CLASSPTR ),
	DEFINE_FIELD( CVehicleTankBSP, m_iVehicleID, FIELD_INTEGER ),
	DEFINE_FIELD( CVehicleTankBSP, m_vecLastValidPos, FIELD_VECTOR ),
	DEFINE_FIELD( CVehicleTankBSP, m_vecLastValidAng, FIELD_VECTOR ),
};

IMPLEMENT_SAVERESTORE( CVehicleTankBSP, CBaseMonster );

LINK_ENTITY_TO_CLASS( vehicle_tank_barrel, CVehicleTankBSP );
LINK_ENTITY_TO_CLASS( vehicle_tank_turret, CVehicleTankBSP );
LINK_ENTITY_TO_CLASS( vehicle_tank_body, CVehicleTankBSP );

void CVehicleTankBSP::Spawn( void )
{
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CVehicleTankBSP::WaitThink );
	pev->nextthink = pev->ltime + 1;
	SetUse( &CVehicleTankBSP::TankBSPUse );

	m_pPlayer = NULL;
	m_bBlocked = FALSE;
	pev->dmg = 35;
	pev->takedamage = DAMAGE_YES;
}

// damage to a part goes to the tank
int CVehicleTankBSP::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	CBaseEntity *pTank = UTIL_FindEntityByClassname( NULL, "vehicle_tank" );
	while ( pTank )
	{
		if ( ( (CVehicleTank *)pTank )->m_iVehicleID == m_iVehicleID )
			return pTank->TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
		pTank = UTIL_FindEntityByClassname( pTank, "vehicle_tank" );
	}
	return 0;
}

// using a part gets the player into the tank
void CVehicleTankBSP::TankBSPUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	ALERT( at_console, "Use Tank Body\n" );

	if ( !pActivator || !pActivator->IsPlayer() )
		return;

	CVehicleTank *pTank = (CVehicleTank *)UTIL_FindEntityByClassname( NULL, "vehicle_tank" );
	while ( pTank )
	{
		if ( pTank->m_iVehicleID == m_iVehicleID )
		{
			pTank->UseVehicle( pActivator, pCaller, useType, value );
			return;
		}
		pTank = (CVehicleTank *)UTIL_FindEntityByClassname( pTank, "vehicle_tank" );
	}
}

void CVehicleTankBSP::WaitThink( void )
{
	pev->nextthink = pev->ltime + 10;
}

void CVehicleTankBSP::Blocked( CBaseEntity *pOther )
{
	m_bBlocked = TRUE;
	pOther->TakeDamage( pev, pev, 1000, DMG_CRUSH );
	ALERT( at_console, "Tank: Blocked by %s\n", STRING( pOther->pev->classname ) );
}

void CVehicleTankBSP::Precache( void )
{
	UTIL_PrecacheOther( "vehicle_tank" );
}

//=========================================================
// trigger_tankshell - fires its target when a tank shell or
// rocket hits it, and explodes the shell
//=========================================================
class CVehicleTriggerTankShell : public CBaseEntity
{
public:
	void Spawn( void );
	void EXPORT TouchShell( CBaseEntity *pOther );
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS( trigger_tankshell, CVehicleTriggerTankShell );

void CVehicleTriggerTankShell::Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_PUSH;
	pev->effects |= EF_NODRAW;
	pev->classname = MAKE_STRING( "trigger_tankshell" );

	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CVehicleTriggerTankShell::TouchShell );
}

void CVehicleTriggerTankShell::TouchShell( CBaseEntity *pOther )
{
	if ( FClassnameIs( pOther->pev, "vehicle_tank_shell" ) )
	{
		( (CVehicleTankShell *)pOther )->ShellTouch( this );
		FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
		UTIL_Remove( this );
	}

	if ( FClassnameIs( pOther->pev, "vehicle_tank_rocket" ) )
	{
		( (CVehicleTankRocket *)pOther )->RocketTouch( this );
		FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
		UTIL_Remove( this );
	}
}

//=========================================================
// trigger_tank - fires its target when a driven tank enters
//=========================================================
class CVehicleTriggerTank : public CBaseEntity
{
public:
	void Spawn( void );
	void EXPORT TouchTank( CBaseEntity *pOther );
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS( trigger_tank, CVehicleTriggerTank );

void CVehicleTriggerTank::Spawn( void )
{
	pev->classname = MAKE_STRING( "trigger_tank" );
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_PUSH;
	pev->effects |= EF_NODRAW;

	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CVehicleTriggerTank::TouchTank );
}

void CVehicleTriggerTank::TouchTank( CBaseEntity *pOther )
{
	// a tank stands for its driver
	if ( FClassnameIs( pOther->pev, "vehicle_tank" ) && ( (CVehicleTank *)pOther )->m_pPlayer )
		pOther = ( (CVehicleTank *)pOther )->m_pPlayer;

	if ( pOther->Classify() != CLASS_PLAYER )
		return;

	if ( ( (CBasePlayer *)pOther )->m_pVehicle )
	{
		FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
		UTIL_Remove( this );
	}
}

//=========================================================
// trigger_tankoutofgas - the tank driving through runs dry
//=========================================================
class CVehicleTriggerTankGas : public CBaseEntity
{
public:
	void Spawn( void );
	void EXPORT TouchTank( CBaseEntity *pOther );
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS( trigger_tankoutofgas, CVehicleTriggerTankGas );

void CVehicleTriggerTankGas::Spawn( void )
{
	pev->classname = MAKE_STRING( "trigger_tankoutofgas" );
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_PUSH;
	pev->effects |= EF_NODRAW;

	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CVehicleTriggerTankGas::TouchTank );
}

void CVehicleTriggerTankGas::TouchTank( CBaseEntity *pOther )
{
	if ( FClassnameIs( pOther->pev, "vehicle_tank" ) && ( (CVehicleTank *)pOther )->m_pPlayer )
		pOther = ( (CVehicleTank *)pOther )->m_pPlayer;

	if ( pOther->Classify() != CLASS_PLAYER )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	if ( pPlayer->m_pVehicle )
	{
		CVehicleTank *pTank = (CVehicleTank *)(CBaseEntity *)pPlayer->m_pVehicle;
		pTank->m_iGasState = TANK_GAS_EMPTY;

		MESSAGE_BEGIN( MSG_ONE, gmsgTank, NULL, pOther->pev );
			WRITE_SHORT( 2 );
		MESSAGE_END();

		pTank->SetTankAnim( TANK_OFFLINE );
		EMIT_SOUND_DYN( ENT( pPlayer->m_pVehicle->pev ), CHAN_STATIC, "tank/outofgas.wav", 1.0, 0.8, 0, PITCH_NORM );
		FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
		UTIL_Remove( this );
	}
}

//=========================================================
// effects helpers
//=========================================================

// an explosion (nothing calls it)
void ExplosionAt( Vector vecOrigin )
{
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_SHORT( g_sModelIndexWExplosion );
		WRITE_BYTE( 10 );	// scale * 10
		WRITE_BYTE( 10 );	// framerate
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END();
}

// White beam between two points (impulse 109 uses it).
void DrawDebugBeam( Vector vecStart, Vector vecEnd, int r, int g, int b )
{
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMPOINTS );
		WRITE_COORD( vecStart.x );
		WRITE_COORD( vecStart.y );
		WRITE_COORD( vecStart.z );
		WRITE_COORD( vecEnd.x );
		WRITE_COORD( vecEnd.y );
		WRITE_COORD( vecEnd.z );
		WRITE_SHORT( g_sModelIndexLaser );
		WRITE_BYTE( 0 );	// frame start
		WRITE_BYTE( 10 );	// framerate
		WRITE_BYTE( 1 );	// life
		WRITE_BYTE( 4 );	// width
		WRITE_BYTE( 0 );	// noise
		WRITE_BYTE( r );
		WRITE_BYTE( g );
		WRITE_BYTE( b );
		WRITE_BYTE( 255 );	// brightness
		WRITE_BYTE( 10 );	// speed
	MESSAGE_END();
}

float UTIL_AngleNormalize( float flAngle )
{
	if ( flAngle < 0 )
		flAngle += 360 * ( 1 - (int)( -flAngle / 360 ) );
	else if ( flAngle >= 360 )
		flAngle -= 360 * (int)( flAngle / 360 );

	if ( flAngle > 180 )
		flAngle -= 360;
	if ( flAngle <= -180 )
		flAngle += 360;

	return flAngle;
}

//=========================================================
// vehicle_tank_shell
//=========================================================
static short s_sRocketExplode;	// explosion4.spr (names ours)
static short s_sShellExplode;		// tank1.spr

LINK_ENTITY_TO_CLASS( vehicle_tank_shell, CVehicleTankShell );

void CVehicleTankShell::Precache( void )
{
	PRECACHE_MODEL( "models/artillary.mdl" );
	m_iSmokeTrail = PRECACHE_MODEL( "sprites/dmlsmoke.spr" );
	PRECACHE_SOUND( "weapons/rocket1.wav" );
	s_sShellExplode = PRECACHE_MODEL( "sprites/tank1.spr" );
	PRECACHE_MODEL( "sprites/part2.spr" );
	m_iRockGibs = PRECACHE_MODEL( "models/rockgibs.mdl" );
	m_iMetalGibs = PRECACHE_MODEL( "models/metalgibs.mdl" );
	m_iWoodGibs = PRECACHE_MODEL( "models/woodgibs.mdl" );
}

CVehicleTankShell *CVehicleTankShell::Create( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner )
{
	CVehicleTankShell *pShell = GetClassPtr( (CVehicleTankShell *)NULL );

	UTIL_SetOrigin( pShell->pev, vecOrigin );
	pShell->pev->angles = vecAngles;
	pShell->Spawn();
	pShell->SetTouch( &CVehicleTankShell::ShellTouch );
	pShell->pev->owner = pOwner->edict();

	return pShell;
}

void CVehicleTankShell::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT( pev ), "models/artillary.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	pev->classname = MAKE_STRING( "vehicle_tank_shell" );

	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5, 0, PITCH_NORM );
	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "tank/firering.wav", 1, 0.8, 0, PITCH_NORM );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iSmokeTrail );	// model
		WRITE_BYTE( 3 );	// life
		WRITE_BYTE( 5 );	// width
		WRITE_BYTE( 255 );	// r, g, b
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();

	UTIL_MakeVectors( pev->angles );
	pev->velocity = gpGlobals->v_forward * 1800;
	pev->gravity = 0.75;
	pev->angles = UTIL_VecToAngles( pev->velocity );
	pev->angles.y += 90;
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CVehicleTankShell::OrientThink );
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmg = 300;
}

void CVehicleTankShell::ShellTouch( CBaseEntity *pOther )
{
	TraceResult tr;

	// don't hit the tank that fired it
	if ( !FClassnameIs( ENT( pOther->pev ), "vehicle_tank_turret" ) && !FClassnameIs( ENT( pOther->pev ), "vehicle_tank_barrel" ) )
	{
		if ( UTIL_PointContents( pev->origin ) == CONTENTS_SKY )
		{
			SetThink( &CBaseEntity::SUB_Remove );
			pev->nextthink = gpGlobals->time;
			return;
		}

		STOP_SOUND( ENT( pev ), CHAN_VOICE, "weapons/rocket1.wav" );

		pev->enemy = pOther->edict();

		Vector vecSpot = pev->origin - pev->velocity.Normalize() * 32;
		UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, ENT( pev ), &tr );

		Debris( &tr, vecSpot, vecSpot + pev->velocity.Normalize() * 64 );

		pev->model = iStringNull;
		pev->solid = SOLID_NOT;
		pev->takedamage = DAMAGE_NO;

		// Pull out of the wall a bit
		if ( tr.flFraction != 1.0 )
			pev->origin = tr.vecEndPos + ( tr.vecPlaneNormal * 12 );

		int iContents = UTIL_PointContents( pev->origin );
		int iScale = 75;

		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_EXPLOSION );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_SHORT( s_sShellExplode );
			WRITE_BYTE( iScale );	// scale * 10
			WRITE_BYTE( 20 );	// framerate
			WRITE_BYTE( TE_EXPLFLAG_NOPARTICLES );
		MESSAGE_END();

		Vector vecDebris = tr.vecPlaneNormal * 50;	// sic: unused

		UTIL_DecalTrace( &tr, DECAL_SCORCH1 );

		CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, BIG_EXPLOSION_VOLUME, 3.0 );

		entvars_t *pevOwner;
		if ( pev->owner )
			pevOwner = VARS( pev->owner );
		else
			pevOwner = NULL;

		pev->owner = NULL; // can't traceline attack owner if this is set

		RadiusDamage( pev, pevOwner, pev->dmg, CLASS_NONE, DMG_BLAST );

		pev->effects |= EF_NODRAW;
		SetThink( &CVehicleTankShell::ShellSmokeThink );
		pev->nextthink = gpGlobals->time + 0.3;
		pev->movetype = MOVETYPE_NONE;
		pev->velocity = g_vecZero;
	}
}

void CVehicleTankShell::OrientThink( void )
{
	if ( pev->origin.x < -4096 || pev->origin.x > 4096
		|| pev->origin.y < -4096 || pev->origin.y > 4096
		|| pev->origin.z < -4096 || pev->origin.z > 4096 )
	{
		UTIL_Remove( this );
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	pev->angles = UTIL_VecToAngles( pev->velocity );
}

void CVehicleTankShell::ShellSmokeThink( void )
{
	CSprite *pSprite = CSprite::SpriteCreate( "sprites/part2.spr", pev->origin, TRUE );
	pSprite->SetScale( 7.0 );
	pSprite->AnimateAndDie( 20 );
	pSprite->SetTransparency( kRenderTransAlpha, 255, 255, 255, 255, kRenderFxNoDissipation );
	pSprite->pev->frame = 0;
	UTIL_SetOrigin( pSprite->pev, pev->origin );
	pSprite->pev->origin.z += 16;

	UTIL_Remove( this );
}

//=========================================================
// Debris - the shell's impact throws chunks of what it hit
//=========================================================
void CVehicleTankShell::Debris( TraceResult *pTrace, Vector vecSrc, Vector vecEnd )
{
	char chTextureType;
	char szbuffer[64];
	const char *pTextureName;
	float rgfl1[3];
	float rgfl2[3];

	if ( !pTrace )
		return;

	CBaseEntity *pEntity = CBaseEntity::Instance( pTrace->pHit );

	chTextureType = 0;

	if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		chTextureType = CHAR_TEX_FLESH;
	else
	{
		vecSrc.CopyToArray( rgfl1 );
		vecEnd.CopyToArray( rgfl2 );

		if ( pEntity )
			pTextureName = TRACE_TEXTURE( ENT( pEntity->pev ), rgfl1, rgfl2 );
		else
			pTextureName = TRACE_TEXTURE( ENT( 0 ), rgfl1, rgfl2 );

		if ( pTextureName )
		{
			if ( *pTextureName == '-' || *pTextureName == '+' )
				pTextureName += 2;

			if ( *pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ' )
				pTextureName++;

			strcpy( szbuffer, pTextureName );
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;
			chTextureType = TEXTURETYPE_Find( szbuffer );
		}
	}

	Vector vecPos = pTrace->vecEndPos;
	Vector vecVelocity = pTrace->vecPlaneNormal * 50;

	if ( chTextureType == CHAR_TEX_VENT || chTextureType == CHAR_TEX_TILE )
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecPos );
			WRITE_BYTE( TE_BREAKMODEL );
			WRITE_COORD( vecPos.x );
			WRITE_COORD( vecPos.y );
			WRITE_COORD( vecPos.z );
			WRITE_COORD( 1 );
			WRITE_COORD( 1 );
			WRITE_COORD( 1 );
			WRITE_COORD( vecVelocity.x );
			WRITE_COORD( vecVelocity.y );
			WRITE_COORD( vecVelocity.z );
			WRITE_BYTE( 20 );
			WRITE_SHORT( m_iRockGibs );
			WRITE_BYTE( 24 );
			WRITE_BYTE( 30 );
			WRITE_BYTE( BREAK_CONCRETE );
		MESSAGE_END();
	}
	else if ( chTextureType == CHAR_TEX_METAL )
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecPos );
			WRITE_BYTE( TE_BREAKMODEL );
			WRITE_COORD( vecPos.x );
			WRITE_COORD( vecPos.y );
			WRITE_COORD( vecPos.z );
			WRITE_COORD( 1 );
			WRITE_COORD( 1 );
			WRITE_COORD( 1 );
			WRITE_COORD( vecVelocity.x );
			WRITE_COORD( vecVelocity.y );
			WRITE_COORD( vecVelocity.z );
			WRITE_BYTE( 20 );
			WRITE_SHORT( m_iMetalGibs );
			WRITE_BYTE( 24 );
			WRITE_BYTE( 30 );
			WRITE_BYTE( BREAK_METAL );
		MESSAGE_END();
	}
	else if ( chTextureType == CHAR_TEX_GRATE )
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecPos );
			WRITE_BYTE( TE_BREAKMODEL );
			WRITE_COORD( vecPos.x );
			WRITE_COORD( vecPos.y );
			WRITE_COORD( vecPos.z );
			WRITE_COORD( 1 );
			WRITE_COORD( 1 );
			WRITE_COORD( 1 );
			WRITE_COORD( vecVelocity.x );
			WRITE_COORD( vecVelocity.y );
			WRITE_COORD( vecVelocity.z );
			WRITE_BYTE( 20 );
			WRITE_SHORT( m_iWoodGibs );
			WRITE_BYTE( 24 );
			WRITE_BYTE( 30 );
			WRITE_BYTE( BREAK_WOOD );
		MESSAGE_END();
	}
}

//=========================================================
// vehicle_tank_rocket
//=========================================================
LINK_ENTITY_TO_CLASS( vehicle_tank_rocket, CVehicleTankRocket );

void CVehicleTankRocket::Precache( void )
{
	PRECACHE_MODEL( "models/dmlrocket.mdl" );
	m_iTrail = PRECACHE_MODEL( "sprites/firebeam.spr" );
	m_iTrail2 = PRECACHE_MODEL( "sprites/white2.spr" );
	PRECACHE_SOUND( "ambience/flameburst1.wav" );
	PRECACHE_SOUND( "weapons/explode4.wav" );
	s_sRocketExplode = PRECACHE_MODEL( "sprites/explosion4.spr" );
	PRECACHE_SOUND( "tank/firering.wav" );
	PRECACHE_MODEL( "sprites/smokering.spr" );
	PRECACHE_MODEL( "sprites/flame.spr" );
}

CVehicleTankRocket *CVehicleTankRocket::Create( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CBaseEntity *pEnemy )
{
	CVehicleTankRocket *pRocket = GetClassPtr( (CVehicleTankRocket *)NULL );

	UTIL_SetOrigin( pRocket->pev, vecOrigin );
	pRocket->pev->angles = vecAngles;
	pRocket->m_hEnemy = pEnemy;
	pRocket->Spawn();
	pRocket->SetTouch( &CVehicleTankRocket::RocketTouch );
	if ( pOwner )
		pRocket->pev->owner = pOwner->edict();

	return pRocket;
}

void CVehicleTankRocket::Spawn( void )
{
	Precache( );

	m_flFieldOfView = -1;
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->gravity = 0.75;

	SET_MODEL( ENT( pev ), "models/dmlrocket.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	pev->classname = MAKE_STRING( "vehicle_tank_rocket" );

	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "tank/firering.wav", 1, 0.8, 0, PITCH_NORM );

	UTIL_MakeVectors( pev->angles );
	pev->velocity = gpGlobals->v_forward * 1000 + gpGlobals->v_up * 50;
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CVehicleTankRocket::RocketThink );
	pev->nextthink = gpGlobals->time + 0.45;
	pev->dmg = 200;
	pev->movetype = MOVETYPE_FLY;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 20 );	// life
		WRITE_BYTE( 5 );	// width
		WRITE_BYTE( 224 );	// r, g, b
		WRITE_BYTE( 224 );
		WRITE_BYTE( 100 );
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail2 );	// model
		WRITE_BYTE( 9 );	// life
		WRITE_BYTE( 1 );	// width
		WRITE_BYTE( 255 );	// r, g, b
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 150 );	// brightness
	MESSAGE_END();

	m_pFlame = CSprite::SpriteCreate( "sprites/flame.spr", pev->origin, TRUE );
	m_pFlame->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
	m_pFlame->pev->spawnflags |= SF_SPRITE_TEMPORARY;
	m_pFlame->SetAttachment( edict(), 0 );
	m_pFlame->pev->scale = 1.0;
	m_pFlame->pev->framerate = 15;
	m_pFlame->pev->frame = 0;
	m_pFlame->TurnOn();
}

void CVehicleTankRocket::TankIgniteThink( void )
{
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 20 );	// life
		WRITE_BYTE( 5 );	// width
		WRITE_BYTE( 255 );	// r, g, b
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();

	pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "ambience/flameburst1.wav", 1, 0.5, 0, PITCH_NORM );

	SetThink( &CVehicleTankRocket::RocketThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CVehicleTankRocket::RocketTouch( CBaseEntity *pOther )
{
	TraceResult tr;

	// don't hit the tank that fired it
	if ( !FClassnameIs( ENT( pOther->pev ), "vehicle_tank_turret" ) && !FClassnameIs( ENT( pOther->pev ), "vehicle_tank_barrel" ) )
	{
		if ( UTIL_PointContents( pev->origin ) == CONTENTS_SKY )
		{
			if ( m_pFlame )
			{
				UTIL_Remove( m_pFlame );
				m_pFlame = NULL;
			}
			SetThink( &CBaseEntity::SUB_Remove );
		}
		else
		{
			STOP_SOUND( edict(), CHAN_VOICE, "weapons/rocket1.wav" );

			pev->enemy = pOther->edict();

			Vector vecSpot = pev->origin - pev->velocity.Normalize() * 32;
			UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, ENT( pev ), &tr );

			pev->model = iStringNull;
			pev->solid = SOLID_NOT;
			pev->takedamage = DAMAGE_NO;

			// Pull out of the wall a bit
			pev->origin = pev->origin + tr.vecPlaneNormal * 20;
			UTIL_SetOrigin( pev, pev->origin );

			int iContents = UTIL_PointContents( pev->origin );
			Vector vecDebris = tr.vecPlaneNormal * 50;	// sic: unused

			MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
				WRITE_BYTE( TE_EXPLOSION );
				WRITE_COORD( pev->origin.x );
				WRITE_COORD( pev->origin.y );
				WRITE_COORD( pev->origin.z );
				WRITE_SHORT( s_sRocketExplode );
				WRITE_BYTE( 60 );	// scale * 10
				WRITE_BYTE( 15 );	// framerate
				WRITE_BYTE( TE_EXPLFLAG_NOPARTICLES );
			MESSAGE_END();

			UTIL_MakeVectors( pev->angles );
			UTIL_DecalTrace( &tr, DECAL_SCORCH1 );

			CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, BIG_EXPLOSION_VOLUME, 3.0 );

			entvars_t *pevOwner;
			if ( pev->owner )
				pevOwner = VARS( pev->owner );
			else
				pevOwner = NULL;

			pev->owner = NULL; // can't traceline attack owner if this is set

			if ( pev->dmg )
				::RadiusDamage( pev->origin, pev, pevOwner, pev->dmg, pev->dmg * 2.5, CLASS_NONE, DMG_BLAST );

			pev->effects |= EF_NODRAW;
			pev->movetype = MOVETYPE_NONE;
			pev->velocity = g_vecZero;
			UTIL_SetOrigin( pev, pev->origin );

			if ( m_pFlame )
			{
				UTIL_Remove( m_pFlame );
				m_pFlame = NULL;
			}

			CSprite *pSprite = CSprite::SpriteCreate( "sprites/part2.spr", pev->origin, TRUE );
			pSprite->SetScale( 7.0 );
			pSprite->AnimateAndDie( 10 );
			pSprite->SetTransparency( kRenderTransAlpha, 255, 255, 255, 255, kRenderFxNoDissipation );
			pSprite->pev->frame = 0;
			UTIL_SetOrigin( pSprite->pev, pev->origin );
			pSprite->pev->origin.z += 16;

			// a burst of tracers
			UTIL_MakeVectors( pev->angles );
			for ( int i = 0; i < 30; i++ )
			{
				Vector vecTracer = gpGlobals->v_forward * RANDOM_FLOAT( -600, 600 ) + gpGlobals->v_right * RANDOM_FLOAT( -600, 600 ) + gpGlobals->v_up * RANDOM_FLOAT( -600, 600 );
				if ( fabs( (int)vecTracer.Length() ) >= 100 )
				{
					MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
						WRITE_BYTE( TE_USERTRACER );
						WRITE_COORD( pev->origin.x );
						WRITE_COORD( pev->origin.y );
						WRITE_COORD( pev->origin.z );
						WRITE_COORD( vecTracer.x );
						WRITE_COORD( vecTracer.y );
						WRITE_COORD( vecTracer.z );
						WRITE_BYTE( 10 );	// life * 10
						WRITE_BYTE( 5 );	// color
						WRITE_BYTE( 10 );	// length * 10
					MESSAGE_END();
				}
			}

			SetThink( &CVehicleTankRocket::RocketSmokeThink );
		}

		pev->nextthink = gpGlobals->time;
	}
}

void CVehicleTankRocket::RocketSmokeThink( void )
{
	if ( !m_pSmoke )
	{
		m_pSmoke = CSprite::SpriteCreate( "sprites/smokering.spr", pev->origin, FALSE );
		m_pSmoke->pev->scale = 0.2;
		m_pSmoke->SetTransparency( kRenderTransAlpha, 255, 255, 255, 255, kRenderFxNoDissipation );
		m_pSmoke->pev->frame = 0;
		UTIL_SetOrigin( m_pSmoke->pev, pev->origin );
	}
	else
	{
		m_pSmoke->pev->scale += 0.2;
	}

	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_pSmoke->pev->scale > 3 )
	{
		UTIL_Remove( m_pSmoke );
		m_pSmoke = NULL;
		UTIL_Remove( this );
	}
}

void CVehicleTankRocket::RocketThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->origin.x < -4096 || pev->origin.x > 4096
		|| pev->origin.y < -4096 || pev->origin.y > 4096
		|| pev->origin.z < -4096 || pev->origin.z > 4096 )
	{
		UTIL_Remove( this );
		return;
	}

	// home in on the best enemy in sight
	if ( m_hEnemy == NULL )
	{
		Look( 700 );
		m_hEnemy = BestVisibleEnemy( );
	}

	if ( m_hEnemy != NULL )
	{
		float flSpeed = pev->velocity.Length();
		Vector vecTarget = m_hEnemy->BodyTarget( pev->origin ) - pev->origin;
		vecTarget = vecTarget.Normalize() * 128;
		Vector vecCurrent = pev->velocity.Normalize() * 128;
		Vector vecTurn = vecTarget - vecCurrent;
		if ( vecTurn.Length() > 20 )
			vecTurn = vecTurn.Normalize() * 20;

		pev->velocity = vecTurn + vecCurrent;
		pev->velocity = pev->velocity.Normalize() * flSpeed;
	}

	pev->angles = UTIL_VecToAngles( pev->velocity );
}
