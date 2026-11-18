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
// the Mayan traps: monster_darttrap shoots blow darts
// (monster_dart), monster_targetrocket launches a
// target_rocket that flies at its target
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"

//=========================================================
// monster_dart - a blow dart; it hurts what it hits and
// sticks in the world
//=========================================================
class CDart : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );

	static void Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );

	void EXPORT ShotTouch( CBaseEntity *pOther );
};

LINK_ENTITY_TO_CLASS( monster_dart, CDart );

void CDart::Precache( void )
{
	PRECACHE_MODEL( "models/dart.mdl" );
	PRECACHE_SOUND( "darts/blowdartimpact1.wav" );
	PRECACHE_SOUND( "darts/blowdartimpact2.wav" );
	PRECACHE_SOUND( "darts/blowdartimpact3.wav" );
	PRECACHE_SOUND( "darts/blowdart2.wav" );
	PRECACHE_SOUND( "darts/blowdart3.wav" );
	PRECACHE_SOUND( "debris/flesh1.wav" );
}

void CDart::Spawn( void )
{
	Precache( );

	pev->classname = MAKE_STRING( "monster_dart" );
	SET_MODEL( ENT( pev ), "models/dart.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	UTIL_MakeVectors( pev->angles );
	pev->avelocity.z = RANDOM_LONG( -200, 200 );
}

void CDart::Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity )
{
	CDart *pDart = GetClassPtr( (CDart *)NULL );
	pDart->Spawn( );

	UTIL_SetOrigin( pDart->pev, vecStart );
	pDart->pev->owner = ENT( pevOwner );
	pDart->pev->velocity = vecVelocity;
	pDart->pev->angles = UTIL_VecToAngles( vecVelocity );

	pDart->SetThink( NULL );
	pDart->SetTouch( &CDart::ShotTouch );
	pDart->pev->nextthink = gpGlobals->time + 0.1;

	if ( UTIL_PercentChance( 85 ) )
		EMIT_SOUND_DYN( ENT( pDart->pev ), CHAN_VOICE, "darts/blowdart3.wav", 1, 0.8, 0, PITCH_NORM );
	else
		EMIT_SOUND_DYN( ENT( pDart->pev ), CHAN_VOICE, "darts/blowdart2.wav", 1, 0.8, 0, PITCH_NORM );
}

void CDart::ShotTouch( CBaseEntity *pOther )
{
	pev->nextthink = gpGlobals->time;
	pev->avelocity = g_vecZero;
	SetThink( &CBaseEntity::SUB_Remove );

	if ( pOther->pev->takedamage == DAMAGE_NO )
	{
		if ( !FClassnameIs( pOther->pev, "worldspawn" ) )
			return;

		// stick in the wall
		Vector vecDir = pev->velocity.Normalize( );
		UTIL_SetOrigin( pev, pev->origin - vecDir * 12 );
		pev->angles = UTIL_VecToAngles( vecDir );
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = Vector( 0, 0, 0 );
		pev->nextthink = gpGlobals->time + 10.0;

		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "darts/blowdartimpact1.wav", 0.7, 0.8, 0, PITCH_NORM );	break;
		case 1:	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "darts/blowdartimpact2.wav", 0.7, 0.8, 0, PITCH_NORM );	break;
		case 2:	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "darts/blowdartimpact3.wav", 0.7, 0.8, 0, PITCH_NORM );	break;
		}
	}
	else
	{
		pOther->TakeDamage( pev, pev, 8, DMG_BULLET );
		if ( pOther->IsPlayer( ) )
			EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "debris/flesh1.wav", 1, 0.8, 0, 110 );
	}
}

//=========================================================
// monster_darttrap - when used, shoots dartcount darts (-1
// for no end) along its angles, one every timebetweenshots
// seconds, spread degrees off, then removes itself
//=========================================================
class CDartTrap : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void EXPORT OnUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT DartShootThink( void );

	int		m_iNumDartsAvalible;
	int		m_iSpread;
	float	m_fTimeBetweenShots;
};

LINK_ENTITY_TO_CLASS( monster_darttrap, CDartTrap );

TYPEDESCRIPTION	CDartTrap::m_SaveData[] =
{
	DEFINE_FIELD( CDartTrap, m_iNumDartsAvalible, FIELD_INTEGER ),
	DEFINE_FIELD( CDartTrap, m_fTimeBetweenShots, FIELD_FLOAT ),
	DEFINE_FIELD( CDartTrap, m_iSpread, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CDartTrap, CBaseMonster );

void CDartTrap::KeyValue( KeyValueData *pkvd )
{
	// sic: separate ifs, as in func_wind
	if ( FStrEq( pkvd->szKeyName, "dartcount" ) )
	{
		m_iNumDartsAvalible = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "timebetweenshots" ) )
	{
		m_fTimeBetweenShots = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "spread" ) )
	{
		m_iSpread = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CDartTrap::Precache( void )
{
	UTIL_PrecacheOther( "monster_dart" );
	PRECACHE_MODEL( "models/dart.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );
}

void CDartTrap::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT( pev ), "models/null.mdl" );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	pev->takedamage = DAMAGE_NO;

	SetUse( &CDartTrap::OnUse );
	SetThink( &CDartTrap::DartShootThink );
}

void CDartTrap::OnUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	pev->nextthink = gpGlobals->time + m_fTimeBetweenShots;
}

void CDartTrap::DartShootThink( void )
{
	if ( m_iNumDartsAvalible != -1 )
	{
		if ( m_iNumDartsAvalible == 0 )
		{
			UTIL_Remove( this );
			return;
		}
		m_iNumDartsAvalible--;
	}

	Vector vecSpread = m_iSpread * VECTOR_CONE_1DEGREES;

	// as CBaseEntity::FireBullets spreads its shots
	float x, y, z;
	do {
		x = RANDOM_FLOAT( -0.5, 0.5 ) + RANDOM_FLOAT( -0.5, 0.5 );
		y = RANDOM_FLOAT( -0.5, 0.5 ) + RANDOM_FLOAT( -0.5, 0.5 );
		z = x * x + y * y;
	} while ( z > 1 );

	UTIL_MakeVectors( pev->angles );
	Vector vecDir = gpGlobals->v_forward;
	vecDir = vecDir + x * vecSpread.x * gpGlobals->v_right + y * vecSpread.y * gpGlobals->v_up;
	vecDir = vecDir.Normalize( );
	vecDir = vecDir * 800;

	CDart::Shoot( pev, pev->origin, vecDir );

	pev->nextthink = gpGlobals->time + m_fTimeBetweenShots;
}

//=========================================================
// target_rocket - flies along its angles, speeding up and
// steering towards its target, and explodes on touch
//=========================================================
class CTargetRocket : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void EXPORT RocketExplodeTouch( CBaseEntity *pOther );
	void EXPORT IgniteThink( void );
	void EXPORT AccelerateThink( void );

	int			m_iTrail;
	Vector		m_vecForward;
	CSprite		*m_pFlare;
	CBaseEntity	*m_pTargetEntity;
};

LINK_ENTITY_TO_CLASS( target_rocket, CTargetRocket );

TYPEDESCRIPTION	CTargetRocket::m_SaveData[] =
{
	DEFINE_FIELD( CTargetRocket, m_vecForward, FIELD_VECTOR ),
	DEFINE_FIELD( CTargetRocket, m_pFlare, FIELD_CLASSPTR ),
	DEFINE_FIELD( CTargetRocket, m_pTargetEntity, FIELD_CLASSPTR ),
};

IMPLEMENT_SAVERESTORE( CTargetRocket, CGrenade );

void CTargetRocket::Precache( void )
{
	PRECACHE_MODEL( "models/rocket.mdl" );
	m_iTrail = PRECACHE_MODEL( "sprites/firebeam.spr" );
	PRECACHE_MODEL( "sprites/flame.spr" );
	PRECACHE_SOUND( "weapons/rocket1.wav" );
	PRECACHE_SOUND( "gunner/gunner_boom1.wav" );
	PRECACHE_SOUND( "gunner/gunner_boom2.wav" );
	PRECACHE_SOUND( "gunner/gunner_boom3.wav" );
}

void CTargetRocket::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT( pev ), "models/rocket.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CTargetRocket::IgniteThink );
	SetTouch( &CTargetRocket::RocketExplodeTouch );

	UTIL_MakeAimVectors( pev->angles );
	m_vecForward = gpGlobals->v_forward;
	pev->gravity = 2;
	pev->dmg = 80;

	pev->nextthink = gpGlobals->time + 0.1;
}

void CTargetRocket::RocketExplodeTouch( CBaseEntity *pOther )
{
	if ( m_pFlare )
		UTIL_Remove( m_pFlare );

	// sic: RANDOM_LONG( 0, 2 ) with cases 1 to 3; a third of the
	// explosions are silent
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 1:	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "gunner/gunner_boom1.wav", 1, 0.8, 0, PITCH_NORM );	break;
	case 2:	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "gunner/gunner_boom2.wav", 1, 0.8, 0, PITCH_NORM );	break;
	case 3:	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "gunner/gunner_boom3.wav", 1, 0.8, 0, PITCH_NORM );	break;
	}

	ExplodeTouch( pOther );
}

void CTargetRocket::IgniteThink( void )
{
	m_pFlare = CSprite::SpriteCreate( "sprites/flame.spr", pev->origin, TRUE );
	m_pFlare->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
	m_pFlare->SetAttachment( edict( ), 1 );
	m_pFlare->TurnOn( );

	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/rocket1.wav", 0.5, 0.5 );

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex( ) );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 15 );	// life
		WRITE_BYTE( 5 );	// width
		WRITE_BYTE( 224 );	// r, g, b
		WRITE_BYTE( 224 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END( );

	SetThink( &CTargetRocket::AccelerateThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CTargetRocket::AccelerateThink( void )
{
	if ( pev->origin.x >= -4096 && pev->origin.x <= 4096
		&& pev->origin.y >= -4096 && pev->origin.y <= 4096
		&& pev->origin.z >= -4096 && pev->origin.z <= 4096 )
	{
		// steer towards the target
		if ( m_pTargetEntity )
		{
			// sic: Normalize's result is dropped, so the rocket gains 0.7
			// of the whole distance to the target sideways and all of it
			// upwards, every tenth of a second
			Vector vecDir, vecVelocity;
			vecVelocity = pev->velocity;
			vecDir = m_pTargetEntity->pev->origin - pev->origin;
			vecDir.Normalize( );
			vecDir[0] = vecDir[0] * 0.7;
			vecDir[1] = vecDir[1] * 0.7;
			pev->velocity = vecDir + vecVelocity;
		}

		// and speed up
		if ( pev->velocity.Length( ) < 1800 )
			pev->velocity = pev->velocity + m_vecForward * 200;

		pev->angles = UTIL_VecToAngles( pev->velocity );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		if ( m_pFlare )
			UTIL_Remove( m_pFlare );
		UTIL_Remove( this );
	}
}

//=========================================================
// monster_targetrocket - launches a target_rocket at the
// entity it targets when used, then removes itself
//=========================================================
class CTargetRocketLauncher : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void EXPORT OnUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT RocketShootThink( void );

	CBaseEntity	*m_pTargetEntity;	// sic: never set
};

LINK_ENTITY_TO_CLASS( monster_targetrocket, CTargetRocketLauncher );

TYPEDESCRIPTION	CTargetRocketLauncher::m_SaveData[] =
{
	DEFINE_FIELD( CTargetRocketLauncher, m_pTargetEntity, FIELD_CLASSPTR ),
};

IMPLEMENT_SAVERESTORE( CTargetRocketLauncher, CBaseMonster );

void CTargetRocketLauncher::Precache( void )
{
	UTIL_PrecacheOther( "target_rocket" );
	PRECACHE_MODEL( "models/null.mdl" );
}

void CTargetRocketLauncher::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT( pev ), "models/null.mdl" );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;

	SetUse( &CTargetRocketLauncher::OnUse );
	SetThink( &CTargetRocketLauncher::RocketShootThink );
}

void CTargetRocketLauncher::OnUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	pev->nextthink = gpGlobals->time;
}

void CTargetRocketLauncher::RocketShootThink( void )
{
	edict_t *pentTarget = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) );
	if ( !pentTarget )
	{
		UTIL_Remove( this );
		return;
	}

	UTIL_MakeVectors( pev->angles );

	CTargetRocket *pRocket = GetClassPtr( (CTargetRocket *)NULL );
	pRocket->pev->angles = pev->angles;
	pRocket->Spawn( );
	pRocket->pev->target = pev->target;
	UTIL_SetOrigin( pRocket->pev, pev->origin );
	pRocket->pev->velocity = pev->velocity + gpGlobals->v_forward * 100;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 18 );	// scale * 10
		WRITE_BYTE( 12 );	// framerate
	MESSAGE_END( );

	pRocket->m_pTargetEntity = CBaseEntity::Instance( pentTarget );

	pev->nextthink = gpGlobals->time + 1;
	SetThink( &CBaseEntity::SUB_Remove );
}
