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
// meteor_god: while on, drops meteors at random spots of a
// rectangle around itself. meteor_target: drops one meteor
// toward itself from a height above it when it is used.
// The meteors are streakers (a lava ball that burns out in
// the air), small and large smoking asteroids that blow up
// with lava ball or rock debris where they land.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"

#define METEOR_STREAKER		0
#define METEOR_SMALL		1
#define METEOR_LARGE		2

#define SF_METEORGOD_START_OFF	1

// model indexes; g_sSmoke is the meteors' own index of sprites/smoke.spr,
// besides g_sModelIndexSmoke. Nothing reads g_sImpact.
short g_sImpact;
short g_sRockGibs;
short g_sLavaball;
short g_sSmoke;

class CMeteorGod : public CBaseEntity
{
public:
	CMeteorGod( void );
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT Think( void );
	void EXPORT Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL	m_bActive;
	float	m_fDropSizeX;		// dropsizex: half the width of the drop area
	float	m_fDropSizeY;		// dropsizey
	float	m_fMinDropFreq;		// mindropfreq: seconds between meteors
	float	m_fMaxDropFreq;		// maxdropfreq
	int		m_iStreakers;		// typestreak: weights of the meteor types
	int		m_iSmalls;			// typesmall
	int		m_iLarges;			// typelarge
	Vector	m_vecDir;			// pitch and yaw, then the direction they make
	BOOL	m_bDebris;
};

class CMeteor : public CBaseEntity
{
public:
	void Spawn( void );
	static CMeteor *MeteorCreate( Vector vecOrigin, int iType, Vector vecDir, BOOL bDebris );
	void StartTrail( void );
	void EXPORT MeteorTouch( CBaseEntity *pOther );
	void EXPORT MeteorChipsThink( void );
	void EXPORT StreakerThink( void );
	void EXPORT KillMeteorThink( void );
	void EXPORT LittleSmokingMeteorThink( void );
	void EXPORT BigSmokingMeteorThink( void );
	void Debris( Vector vecOrigin, Vector vecVelocity, int iModel, int iCount );
	void BigMeteorExplode( void );
	void LittleMeteorExplode( void );

	// IsInWorld without the speed test
	BOOL InWorld( void )
	{
		return pev->origin.x >= -4096 && pev->origin.x <= 4096 &&
			pev->origin.y >= -4096 && pev->origin.y <= 4096 &&
			pev->origin.z >= -4096 && pev->origin.z <= 4096;
	}

	int		m_iType;
	BOOL	m_bDebris;
};

class CMeteorExplosion : public CBaseEntity
{
public:
	void Spawn( void );
	static CMeteorExplosion *ExplosionCreate( Vector vecOrigin, float flScale );
	void EXPORT Animate( void );
};

class CMeteorChip : public CBaseEntity
{
public:
	void Spawn( void );
	static CMeteorChip *ChipCreate( Vector vecOrigin, Vector vecVelocity );
	void EXPORT Touch( CBaseEntity *pOther );
	void EXPORT Animate( void );
};

class CMeteorTarget : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	float	m_flHeight;			// heightabove
	int		m_iType;			// type
};

class CMeteorSound : public CBaseEntity
{
public:
	void Spawn( void );
	static CMeteorSound *SoundCreate( Vector vecOrigin );
};

//=========================================================
// meteor_god
//=========================================================
LINK_ENTITY_TO_CLASS( meteor_god, CMeteorGod );

TYPEDESCRIPTION	CMeteorGod::m_SaveData[] =
{
	DEFINE_FIELD( CMeteorGod, m_bActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMeteorGod, m_vecDir, FIELD_VECTOR ),
	DEFINE_FIELD( CMeteorGod, m_fMinDropFreq, FIELD_FLOAT ),
	DEFINE_FIELD( CMeteorGod, m_fMaxDropFreq, FIELD_FLOAT ),
	DEFINE_FIELD( CMeteorGod, m_fDropSizeX, FIELD_FLOAT ),
	DEFINE_FIELD( CMeteorGod, m_fDropSizeY, FIELD_FLOAT ),
	DEFINE_FIELD( CMeteorGod, m_iStreakers, FIELD_INTEGER ),
	DEFINE_FIELD( CMeteorGod, m_iSmalls, FIELD_INTEGER ),
	DEFINE_FIELD( CMeteorGod, m_iLarges, FIELD_INTEGER ),
	DEFINE_FIELD( CMeteorGod, m_bDebris, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CMeteorGod, CBaseEntity );

CMeteorGod::CMeteorGod( void )
{
	Vector vecDown( -90, 0, 0 );	// pitch -90: straight down

	m_bActive = FALSE;
	m_vecDir = vecDown;
	m_fDropSizeY = 0;
	m_fDropSizeX = 0;
	m_fMinDropFreq = 1;
	m_fMaxDropFreq = 5;
	m_iStreakers = 1;
	m_iSmalls = 1;
	m_iLarges = 1;
	m_bDebris = TRUE;
}

void CMeteorGod::Precache( void )
{
	g_sRockGibs = PRECACHE_MODEL( "models/rockgibsbig.mdl" );
	g_sLavaball = PRECACHE_MODEL( "models/lavaball.mdl" );
	PRECACHE_MODEL( "models/asteroid.mdl" );
	g_sSmoke = PRECACHE_MODEL( "sprites/smoke.spr" );
	g_sImpact = PRECACHE_MODEL( "sprites/impact.spr" );
	PRECACHE_SOUND( "meteor/meteor_hit1.wav" );
	PRECACHE_SOUND( "meteor/meteor_hit2.wav" );
	PRECACHE_SOUND( "meteor/meteor_hit3.wav" );
	PRECACHE_MODEL( "sprites/chip.spr" );
}

void CMeteorGod::KeyValue( KeyValueData *pkvd )
{
	// sic: separate ifs, each key is tested against all of them
	if ( FStrEq( pkvd->szKeyName, "typestreak" ) )
	{
		m_iStreakers = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "typelarge" ) )
	{
		m_iLarges = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "typesmall" ) )
	{
		m_iSmalls = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "dropsizex" ) )
	{
		m_fDropSizeX = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "dropsizey" ) )
	{
		m_fDropSizeY = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "mindropfreq" ) )
	{
		m_fMinDropFreq = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "maxdropfreq" ) )
	{
		m_fMaxDropFreq = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "pitch" ) )
	{
		m_vecDir.x = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "debris" ) )
	{
		// sic: debris 1 turns the debris off
		if ( atoi( pkvd->szValue ) == 1 )
			m_bDebris = FALSE;
		else
			m_bDebris = TRUE;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CMeteorGod::Spawn( void )
{
	Precache();

	pev->effects |= EF_NODRAW;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	UTIL_SetOrigin( pev, pev->origin );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );

	pev->nextthink = gpGlobals->time + 0.1;

	m_vecDir.y = pev->angles.y;
	UTIL_MakeVectors( m_vecDir );
	m_vecDir = gpGlobals->v_forward;

	if ( FBitSet( pev->spawnflags, SF_METEORGOD_START_OFF ) )
		m_bActive = FALSE;
	else
		m_bActive = TRUE;
}

void CMeteorGod::Think( void )
{
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( m_fMinDropFreq, m_fMaxDropFreq );

	if ( !m_bActive )
		return;

	Vector vecSpot;
	vecSpot = pev->origin + Vector( RANDOM_LONG( -m_fDropSizeX, m_fDropSizeX ), RANDOM_LONG( -m_fDropSizeY, m_fDropSizeY ), 0 );

	// pick a type by the weights
	int iRand = RANDOM_LONG( 0, m_iStreakers + m_iSmalls + m_iLarges );
	int iLimit = m_iStreakers;
	int iType;

	if ( iRand <= iLimit )
		iType = METEOR_STREAKER;
	else
	{
		iLimit += m_iSmalls;
		if ( iRand <= iLimit )
			iType = METEOR_SMALL;
		else
		{
			iLimit += m_iLarges;
			if ( iRand <= iLimit )
				iType = METEOR_LARGE;
		}
	}

	CMeteor::MeteorCreate( vecSpot, iType, m_vecDir, m_bDebris );
}

void CMeteorGod::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_bActive = !m_bActive;
}

//=========================================================
// meteor - one falling meteor, made by meteor_god and
// meteor_target
//=========================================================
CMeteor *CMeteor::MeteorCreate( Vector vecOrigin, int iType, Vector vecDir, BOOL bDebris )
{
	CMeteor *pMeteor = GetClassPtr( (CMeteor *)NULL );

	pMeteor->m_iType = iType;
	pMeteor->Spawn();
	UTIL_SetOrigin( pMeteor->pev, vecOrigin );

	vecDir.x += RANDOM_FLOAT( -0.1, 0.1 );
	pMeteor->pev->velocity = vecDir;
	pMeteor->pev->velocity = pMeteor->pev->velocity * RANDOM_FLOAT( -1600, -1200 );
	pMeteor->m_bDebris = bDebris;

	return pMeteor;
}

void CMeteor::Spawn( void )
{
	pev->classname = MAKE_STRING( "meteor" );
	pev->movetype = MOVETYPE_FLYMISSILE;
	pev->solid = SOLID_TRIGGER;

	pev->avelocity.x = RANDOM_FLOAT( -150, 150 );
	pev->avelocity.y = RANDOM_FLOAT( -150, 150 );
	pev->avelocity.z = RANDOM_FLOAT( -150, 150 );

	m_bDebris = TRUE;
	SetTouch( &CMeteor::MeteorTouch );
	StartTrail();

	switch ( m_iType )
	{
	case METEOR_STREAKER:
		UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
		SET_MODEL( ENT( pev ), "models/lavaball.mdl" );
		pev->dmg = 0;
		pev->nextthink = gpGlobals->time + 0.1;
		SetThink( &CMeteor::StreakerThink );
		break;

	case METEOR_SMALL:
		UTIL_SetSize( pev, Vector( -8, -8, -8 ), Vector( 8, 8, 8 ) );
		SET_MODEL( ENT( pev ), "models/asteroid.mdl" );
		pev->nextthink = gpGlobals->time + 0.1;
		SetThink( &CMeteor::LittleSmokingMeteorThink );
		break;

	case METEOR_LARGE:
		UTIL_SetSize( pev, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ) );
		SET_MODEL( ENT( pev ), "models/asteroid.mdl" );
		pev->body = 1;
		pev->nextthink = gpGlobals->time + 0.1;
		SetThink( &CMeteor::BigSmokingMeteorThink );
		break;
	}
}

void CMeteor::StartTrail( void )
{
	if ( m_iType == METEOR_STREAKER )
		pev->effects |= EF_LIGHT;

	TraceResult tr;
	UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, 4096 ), ignore_monsters, ENT( pev ), &tr );

	// sic: the sound entity makes no sound
	CMeteorSound::SoundCreate( tr.vecEndPos );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( g_sSmoke );	// model
		WRITE_BYTE( m_iType + 2 );	// life
		WRITE_BYTE( m_iType * 2 + 1 );	// width
		WRITE_BYTE( 224 );	// r, g, b
		WRITE_BYTE( 224 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 100 );	// brightness
	MESSAGE_END();
}

void CMeteor::MeteorTouch( CBaseEntity *pOther )
{
	SetTouch( NULL );

	::RadiusDamage( pev->origin, pev, pev, 50, 100, CLASS_NONE, DMG_CRUSH );

	switch ( m_iType )
	{
	case METEOR_SMALL:
		LittleMeteorExplode();
		break;

	case METEOR_LARGE:
		BigMeteorExplode();
		break;

	default:
		UTIL_Remove( this );
		break;
	}
}

// sic: nothing uses this think
void CMeteor::MeteorChipsThink( void )
{
	if ( InWorld() )
	{
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.05, 0.1 );

		Vector vecVelocity = pev->velocity;
		Vector vecOrigin;

		vecVelocity = vecVelocity * 0.3;
		vecVelocity.x += RANDOM_FLOAT( -0.08, 0.08 );
		vecVelocity.y += RANDOM_FLOAT( -0.08, 0.08 );
		vecVelocity.z += RANDOM_FLOAT( -100, 200 );

		vecOrigin = pev->origin + pev->velocity.Normalize() * -48;
		vecOrigin = vecOrigin + Vector( RANDOM_FLOAT( -8, 8 ), RANDOM_FLOAT( -8, 8 ), 0 );

		CMeteorChip::ChipCreate( vecOrigin, vecVelocity );
	}
	else
	{
		UTIL_Remove( this );
	}
}

void CMeteor::StreakerThink( void )
{
	if ( InWorld() )
	{
		// burn out somewhere between a quarter of the way and the ground
		TraceResult tr;
		Vector vecStart;

		vecStart = pev->origin;
		UTIL_TraceLine( vecStart, vecStart + pev->velocity.Normalize() * 4096, ignore_monsters, ENT( pev ), &tr );

		float flTime = ( tr.vecEndPos - vecStart ).Length() / pev->velocity.Length();

		pev->nextthink = gpGlobals->time + RANDOM_FLOAT( flTime * 0.25, flTime );
		SetThink( &CMeteor::KillMeteorThink );
	}
	else
	{
		UTIL_Remove( this );
	}
}

void CMeteor::KillMeteorThink( void )
{
	CMeteorChip::ChipCreate( pev->origin, pev->velocity );

	SetTouch( NULL );
	UTIL_Remove( this );
}

void CMeteor::LittleSmokingMeteorThink( void )
{
	if ( InWorld() )
	{
		pev->renderfx = kRenderFxGlowShell;
		pev->renderamt = 100;
		pev->rendercolor.x = 255;
		pev->rendercolor.y = 130;
		pev->rendercolor.z = 30;

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x + RANDOM_LONG( -8, 8 ) );
			WRITE_COORD( pev->origin.y + RANDOM_LONG( -8, 8 ) );
			WRITE_COORD( pev->origin.z + 32 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 50 );	// scale * 10
			WRITE_BYTE( 20 );	// framerate
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x + RANDOM_LONG( -16, 16 ) );
			WRITE_COORD( pev->origin.y + RANDOM_LONG( -16, 16 ) );
			WRITE_COORD( pev->origin.z + 128 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 30 );	// scale * 10
			WRITE_BYTE( 20 );	// framerate
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x + RANDOM_LONG( -32, 32 ) );
			WRITE_COORD( pev->origin.y + RANDOM_LONG( -32, 32 ) );
			WRITE_COORD( pev->origin.z + 256 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 15 );	// scale * 10
			WRITE_BYTE( 20 );	// framerate
		MESSAGE_END();

		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		UTIL_Remove( this );
	}
}

void CMeteor::BigSmokingMeteorThink( void )
{
	if ( InWorld() )
	{
		pev->renderfx = kRenderFxGlowShell;
		pev->renderamt = 100;
		pev->rendercolor.x = 255;
		pev->rendercolor.y = 130;
		pev->rendercolor.z = 30;

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x + RANDOM_LONG( -8, 8 ) );
			WRITE_COORD( pev->origin.y + RANDOM_LONG( -8, 8 ) );
			WRITE_COORD( pev->origin.z + 32 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 25 );	// scale * 10
			WRITE_BYTE( 20 );	// framerate
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x + RANDOM_LONG( -16, 16 ) );
			WRITE_COORD( pev->origin.y + RANDOM_LONG( -16, 16 ) );
			WRITE_COORD( pev->origin.z + 256 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 75 );	// scale * 10
			WRITE_BYTE( 20 );	// framerate
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x + RANDOM_LONG( -32, 32 ) );
			WRITE_COORD( pev->origin.y + RANDOM_LONG( -32, 32 ) );
			WRITE_COORD( pev->origin.z + 512 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 100 );	// scale * 10
			WRITE_BYTE( 20 );	// framerate
		MESSAGE_END();

		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		UTIL_Remove( this );
	}
}

//=========================================================
// Debris - throws iCount rocks of iModel around vecOrigin
//=========================================================
void CMeteor::Debris( Vector vecOrigin, Vector vecVelocity, int iModel, int iCount )
{
	if ( !m_bDebris )
		return;

	// sic: the velocity passed in is never used, and the
	// later rocks live longer than the first ones
	while ( iCount > 0 )
	{
		vecOrigin.x += RANDOM_LONG( -5, 5 );
		vecOrigin.y += RANDOM_LONG( -5, 5 );
		vecOrigin.z += RANDOM_LONG( -5, 5 );

		vecVelocity.x = RANDOM_LONG( -200, 200 );
		vecVelocity.y = RANDOM_LONG( -200, 200 );
		vecVelocity.z = RANDOM_LONG( -300, -100 );

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
			WRITE_BYTE( TE_MODEL );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z );
			WRITE_COORD( vecVelocity.x );
			WRITE_COORD( vecVelocity.y );
			WRITE_COORD( vecVelocity.z );
			WRITE_ANGLE( pev->angles.y );
			WRITE_SHORT( iModel );
			WRITE_BYTE( TE_BOUNCE_NULL );
			WRITE_BYTE( 60 / iCount );	// life
		MESSAGE_END();

		iCount--;
	}
}

void CMeteor::BigMeteorExplode( void )
{
	pev->origin = pev->origin + pev->velocity.Normalize() * RANDOM_LONG( -156, -128 );
	UTIL_SetOrigin( pev, pev->origin );

	Vector vecSpot = pev->origin;
	CMeteorExplosion *pExplosion = CMeteorExplosion::ExplosionCreate( vecSpot, RANDOM_FLOAT( 0.7, 1.0 ) );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		UTIL_EmitAmbientSound( pExplosion->edict(), pExplosion->pev->origin, "meteor/meteor_hit1.wav", 1.0, 0.8, 0, 100 );
		break;
	case 1:
		UTIL_EmitAmbientSound( pExplosion->edict(), pExplosion->pev->origin, "meteor/meteor_hit2.wav", 1.0, 0.8, 0, 100 );
		break;
	case 2:
		UTIL_EmitAmbientSound( pExplosion->edict(), pExplosion->pev->origin, "meteor/meteor_hit3.wav", 1.0, 0.8, 0, 100 );
		break;
	}

	// sic: nothing sets pev->dmg for a large meteor
	::RadiusDamage( pev->origin, pev, NULL, pev->dmg, 400, CLASS_NONE, DMG_BLAST );

	Debris( pev->origin, pev->velocity, g_sRockGibs, RANDOM_LONG( 5, 10 ) );

	SetTouch( NULL );
	SetThink( NULL );
	UTIL_Remove( this );
}

void CMeteor::LittleMeteorExplode( void )
{
	// sic: no UTIL_SetOrigin after the move
	pev->origin = pev->origin + pev->velocity.Normalize() * -50;

	Vector vecSpot = pev->origin;
	Debris( pev->origin, pev->velocity, g_sLavaball, RANDOM_LONG( 5, 10 ) );

	CMeteorExplosion *pExplosion = CMeteorExplosion::ExplosionCreate( vecSpot, RANDOM_FLOAT( 0.2, 0.4 ) );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND_DYN( pExplosion->edict(), CHAN_WEAPON, "meteor/meteor_hit1.wav", 1.0, 1.0, 0, 100 );
		break;
	case 1:
		EMIT_SOUND_DYN( pExplosion->edict(), CHAN_WEAPON, "meteor/meteor_hit2.wav", 1.0, 1.0, 0, 100 );
		break;
	case 2:
		EMIT_SOUND_DYN( pExplosion->edict(), CHAN_WEAPON, "meteor/meteor_hit3.wav", 1.0, 1.0, 0, 100 );
		break;
	}

	SetTouch( NULL );
	SetThink( NULL );
	UTIL_Remove( this );
}

//=========================================================
// meteorexplosion - the impact flash
//=========================================================
CMeteorExplosion *CMeteorExplosion::ExplosionCreate( Vector vecOrigin, float flScale )
{
	CMeteorExplosion *pExplosion = GetClassPtr( (CMeteorExplosion *)NULL );

	pExplosion->Spawn();
	UTIL_SetOrigin( pExplosion->pev, vecOrigin );
	pExplosion->pev->scale = flScale * 3;

	return pExplosion;
}

void CMeteorExplosion::Spawn( void )
{
	pev->classname = MAKE_STRING( "meteorexplosion" );
	pev->model = MAKE_STRING( "sprites/impact.spr" );
	pev->movetype = MOVETYPE_FLYMISSILE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	pev->angles = g_vecZero;
	pev->effects = 0;
	pev->rendermode = kRenderTransAdd;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 255;
	pev->frame = 0;

	UTIL_SetSize( pev, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ) );
	SET_MODEL( ENT( pev ), "sprites/impact.spr" );

	pev->nextthink = gpGlobals->time + 0.1;
	SetThink( &CMeteorExplosion::Animate );
}

void CMeteorExplosion::Animate( void )
{
	pev->nextthink = gpGlobals->time + 0.075;
	pev->frame += 1;

	if ( pev->frame >= 12 )
	{
		SetTouch( NULL );
		SetThink( NULL );
		UTIL_Remove( this );
	}
}

//=========================================================
// meteorchip - a spark a streaker leaves when it burns out
//=========================================================
CMeteorChip *CMeteorChip::ChipCreate( Vector vecOrigin, Vector vecVelocity )
{
	CMeteorChip *pChip = GetClassPtr( (CMeteorChip *)NULL );

	pChip->Spawn();
	UTIL_SetOrigin( pChip->pev, vecOrigin );
	pChip->pev->velocity = vecVelocity;

	return pChip;
}

void CMeteorChip::Spawn( void )
{
	pev->classname = MAKE_STRING( "meteorchip" );
	pev->model = MAKE_STRING( "sprites/chip.spr" );
	pev->movetype = MOVETYPE_FLYMISSILE;
	pev->solid = SOLID_TRIGGER;
	pev->takedamage = DAMAGE_NO;
	pev->effects = 0;
	pev->scale = 1.0;
	pev->rendermode = kRenderTransAdd;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 255;
	pev->frame = RANDOM_LONG( 0, 10 );

	UTIL_SetSize( pev, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ) );
	SET_MODEL( ENT( pev ), "sprites/chip.spr" );

	pev->nextthink = gpGlobals->time + 0.1;
	SetThink( &CMeteorChip::Animate );
}

void CMeteorChip::Touch( CBaseEntity *pOther )
{
	SetTouch( NULL );
	UTIL_Remove( this );
}

void CMeteorChip::Animate( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
	pev->frame += 1;

	if ( pev->frame >= 17 )
	{
		SetTouch( NULL );
		SetThink( NULL );
		UTIL_Remove( this );
	}
}

//=========================================================
// meteor_target
//=========================================================
LINK_ENTITY_TO_CLASS( meteor_target, CMeteorTarget );

TYPEDESCRIPTION	CMeteorTarget::m_SaveData[] =
{
	DEFINE_FIELD( CMeteorTarget, m_flHeight, FIELD_FLOAT ),
	DEFINE_FIELD( CMeteorTarget, m_iType, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CMeteorTarget, CBaseEntity );

void CMeteorTarget::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "pitch" ) )
	{
		pev->angles.x = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "heightabove" ) )
	{
		m_flHeight = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "type" ) )
	{
		m_iType = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

// sic: no rock gibs, lava ball or impact sounds, which only
// meteor_god precaches
void CMeteorTarget::Precache( void )
{
	PRECACHE_MODEL( "models/asteroid.mdl" );
	g_sSmoke = PRECACHE_MODEL( "sprites/smoke.spr" );
	g_sImpact = PRECACHE_MODEL( "sprites/impact.spr" );
	PRECACHE_MODEL( "sprites/chip.spr" );
	PRECACHE_MODEL( "sprites/impact.spr" );
}

void CMeteorTarget::Spawn( void )
{
	Precache();

	pev->effects |= EF_NODRAW;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	UTIL_SetOrigin( pev, pev->origin );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
}

void CMeteorTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	UTIL_MakeVectors( pev->angles );

	Vector vecForward = gpGlobals->v_forward;
	Vector vecStart = pev->origin;
	TraceResult tr;

	UTIL_TraceLine( vecStart, vecStart + vecForward.Normalize() * m_flHeight, ignore_monsters, ENT( pev ), &tr );

	if ( tr.flFraction < 1.0 )
	{
		ALERT( at_console, "Target Meteor Height hit wall, starting 64 below impact point\n" );
		vecStart = tr.vecEndPos - vecForward.Normalize() * 64;
	}
	else
	{
		vecStart = vecStart + vecForward.Normalize() * m_flHeight;
	}

	CMeteor *pMeteor = CMeteor::MeteorCreate( vecStart, m_iType, vecForward, FALSE );
	pMeteor->pev->velocity = vecForward;
	pMeteor->pev->velocity = pMeteor->pev->velocity * -1400;

	UTIL_Remove( this );
}

//=========================================================
// meteorsound - made under every meteor, removes itself
//=========================================================
CMeteorSound *CMeteorSound::SoundCreate( Vector vecOrigin )
{
	CMeteorSound *pSound = GetClassPtr( (CMeteorSound *)NULL );

	pSound->Spawn();
	UTIL_SetOrigin( pSound->pev, vecOrigin );

	return pSound;
}

void CMeteorSound::Spawn( void )
{
	pev->classname = MAKE_STRING( "meteorsound" );
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	pev->effects = EF_NODRAW;

	pev->nextthink = gpGlobals->time + 2;
	SetThink( &CBaseEntity::SUB_Remove );
}
