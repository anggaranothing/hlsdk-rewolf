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
// monster_tank: the Mayan chapter's scripted tank, a
// generic monster whose model events fire homing rockets
// (monster_tank_rocket) at the entity it targets
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "soundent.h"
#include "decals.h"

#define SF_GENERICMONSTER_NOTSOLID		4
#define SF_GENERICMONSTER_POINTENTITY	32

// the drivable tank's shell (vehicletank.cpp declares it in full; vehicletank.h
// can't, triggers.cpp includes it without weapons.h)
class CVehicleTankShell : public CGrenade
{
public:
	static CVehicleTankShell *Create( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner );
};

// genericmonster.cpp's class
class CGenericMonster : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int ISoundMask ( void );
	int CanPlaySentence( BOOL fDisregardState );
};

//=========================================================
// monster_tank_rocket - vehicle_tank_rocket's copy for the
// scripted tank: it homes in on its enemy
//=========================================================
class CMonsterTankRocket : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_ALIEN_BIOWEAPON; }

	static CMonsterTankRocket *Create( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CBaseEntity *pEnemy, float flDamage );

	void EXPORT RocketTouch( CBaseEntity *pOther );
	void EXPORT RocketSmokeThink( void );
	void EXPORT RocketThink( void );

	int m_iTrail;
	int m_iExplosion;
	int m_iTrail2;
	CSprite *m_pFlame;
	CSprite *m_pSmoke;
};

LINK_ENTITY_TO_CLASS( monster_tank_rocket, CMonsterTankRocket );

void CMonsterTankRocket::Precache( void )
{
	PRECACHE_MODEL( "models/dmlrocket.mdl" );
	m_iTrail = PRECACHE_MODEL( "sprites/firebeam.spr" );
	m_iTrail2 = PRECACHE_MODEL( "sprites/white2.spr" );
	PRECACHE_SOUND( "ambience/flameburst1.wav" );
	PRECACHE_SOUND( "weapons/explode4.wav" );
	PRECACHE_MODEL( "sprites/part2.spr" );
	m_iExplosion = PRECACHE_MODEL( "sprites/explosion4.spr" );
	PRECACHE_SOUND( "tank/firering.wav" );
	PRECACHE_MODEL( "sprites/smokering.spr" );
	PRECACHE_MODEL( "sprites/flame.spr" );
}

CMonsterTankRocket *CMonsterTankRocket::Create( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CBaseEntity *pEnemy, float flDamage )
{
	CMonsterTankRocket *pRocket = GetClassPtr( (CMonsterTankRocket *)NULL );

	UTIL_SetOrigin( pRocket->pev, vecOrigin );
	pRocket->pev->angles = vecAngles;
	pRocket->m_hEnemy = pEnemy;
	pRocket->Spawn();
	pRocket->SetTouch( &CMonsterTankRocket::RocketTouch );
	if ( pOwner )
		pRocket->pev->owner = pOwner->edict();
	pRocket->pev->dmg = flDamage;

	return pRocket;
}

void CMonsterTankRocket::Spawn( void )
{
	Precache( );

	m_flFieldOfView = -1;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->gravity = 0.75;

	SET_MODEL( ENT( pev ), "models/dmlrocket.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	pev->classname = MAKE_STRING( "monster_tank_rocket" );

	// sic: 8 isn't one of util.h's SND_ flags (vehicle_tank_rocket plays
	// this with EMIT_SOUND_DYN on CHAN_WEAPON)
	UTIL_EmitAmbientSound( ENT( pev ), pev->origin, "tank/firering.wav", 1, 0.8, 8, PITCH_NORM );

	UTIL_MakeVectors( pev->angles );
	pev->velocity = gpGlobals->v_forward * 2000 + gpGlobals->v_up * 50;
	UTIL_SetOrigin( pev, pev->origin );

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

	pev->movetype = MOVETYPE_FLY;
	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "ambience/flameburst1.wav", 1, 0.5 );

	SetThink( &CMonsterTankRocket::RocketThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CMonsterTankRocket::RocketTouch( CBaseEntity *pOther )
{
	TraceResult tr;

	// don't hit the drivable tank's turret or barrel
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
				WRITE_SHORT( m_iExplosion );
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

			if ( m_pFlame )
			{
				UTIL_Remove( m_pFlame );
				m_pFlame = NULL;
			}

			if ( pev->dmg )
				::RadiusDamage( pev->origin, pev, pevOwner, pev->dmg, pev->dmg * 1.8, CLASS_NONE, DMG_BLAST );

			pev->effects |= EF_NODRAW;
			pev->movetype = MOVETYPE_NONE;
			pev->velocity = g_vecZero;
			UTIL_SetOrigin( pev, pev->origin );

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

			SetThink( &CMonsterTankRocket::RocketSmokeThink );
		}

		pev->nextthink = gpGlobals->time;
	}
}

void CMonsterTankRocket::RocketSmokeThink( void )
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

void CMonsterTankRocket::RocketThink( void )
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

//=========================================================
// monster_tank - a generic monster with the scripted tank
// model; its model events fire rockets at its target
//=========================================================

// monster_tank's model events
#define TANK_AE_ROCKET			1	// a rocket that does damage
#define TANK_AE_ROCKET_NODAMAGE	2	// one that only looks it

class CMonsterTank : public CGenericMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void PrescheduleThink( void );
	void IdleSound( void );

	void FireRocket( float flDamage );
	void FireShell( Vector vecAngles );

	float m_flNextIdleSound;
};

LINK_ENTITY_TO_CLASS( monster_tank, CMonsterTank );

void CMonsterTank::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT( pev ), "models/script_tank.mdl" );
	m_flNextIdleSound = 0;
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 72 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->health			= 8;
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	pev->takedamage		= DAMAGE_NO;

	MonsterInit();

	if ( pev->spawnflags & SF_GENERICMONSTER_NOTSOLID )
	{
		pev->solid = SOLID_NOT;
		pev->takedamage = DAMAGE_NO;
	}

	if ( pev->spawnflags & SF_GENERICMONSTER_POINTENTITY )
	{
		pev->solid = SOLID_NOT;
		pev->takedamage = DAMAGE_NO;
		pev->movetype = MOVETYPE_FLY;
		UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
		UTIL_SetOrigin( pev, pev->origin );
	}

	pev->takedamage = DAMAGE_NO;
	UTIL_SetOrigin( pev, pev->origin );
}

void CMonsterTank::Precache( void )
{
	PRECACHE_MODEL( "models/script_tank.mdl" );
	PRECACHE_SOUND( "weapons/explode4.wav" );
	UTIL_PrecacheOther( "vehicle_tank_shell" );
	UTIL_PrecacheOther( "monster_tank_rocket" );
	PRECACHE_SOUND( "tank/engineidle.wav" );
}

// sic: empty, though PrescheduleThink calls it every 2.36 seconds and
// Precache loads tank/engineidle.wav
void CMonsterTank::IdleSound( void )
{
}

void CMonsterTank::PrescheduleThink( void )
{
	if ( m_flNextIdleSound <= gpGlobals->time )
	{
		IdleSound( );
		m_flNextIdleSound = gpGlobals->time + 2.36;
	}
}

void CMonsterTank::SetYawSpeed( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
	default:
		ys = 45;
	}

	pev->yaw_speed = ys;
}

void CMonsterTank::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case TANK_AE_ROCKET:
		FireRocket( 300 );
		break;

	case TANK_AE_ROCKET_NODAMAGE:
		FireRocket( 0 );
		break;

	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

// a rocket from attachment 1, at the entity the tank targets
void CMonsterTank::FireRocket( float flDamage )
{
	Vector vecSrc;

	UTIL_ScreenShake( pev->origin, 25.0, 150.0, 0.75, 2000 );
	GET_ATTACHMENT( ENT( pev ), 1, vecSrc, NULL );

	CBaseEntity *pTarget = NULL;
	edict_t *pentTarget = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) );
	if ( !FNullEnt( pentTarget ) )
		pTarget = CBaseEntity::Instance( pentTarget );

	CMonsterTankRocket::Create( vecSrc, pev->angles, this, pTarget, flDamage );
}

// sic: nothing calls it; a vehicle_tank_shell from attachment 0
void CMonsterTank::FireShell( Vector vecAngles )
{
	Vector vecSrc;

	GET_ATTACHMENT( ENT( pev ), 0, vecSrc, NULL );
	CVehicleTankShell::Create( vecSrc, vecAngles, this );

	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "weapons/explode4.wav", 1, 0.8, 0, PITCH_NORM );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecSrc.x );
		WRITE_COORD( vecSrc.y );
		WRITE_COORD( vecSrc.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 20 );	// scale * 10
		WRITE_BYTE( 12 );	// framerate
	MESSAGE_END();
}
