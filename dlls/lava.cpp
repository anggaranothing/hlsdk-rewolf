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
// lava_god: an invisible brush that tosses balls of lava up
// at random places around it, each with a glowing trail
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

int g_sModelIndexLavaTrail;

// a ball of lava, gone when it touches anything
class CLava : public CBaseEntity
{
public:
	void Spawn( void );
	virtual void EXPORT Think( void );
	void EXPORT KillTouch( CBaseEntity *pOther );

	static CLava *CreateLava( entvars_t *pevOwner, Vector vecOrigin );
	void LavaTrail( void );
};

LINK_ENTITY_TO_CLASS( lava, CLava );

class CLavaGod : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	virtual void EXPORT Think( void );
	virtual void EXPORT Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void LavaOff( void );
	void LavaOn( void );
	BOOL IsOn( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL	m_bActive;
	float	m_fRadius;
	float	m_fMinDropFreq;
	float	m_fMaxDropFreq;
	float	m_fMaxVel;
};

TYPEDESCRIPTION	CLavaGod::m_SaveData[] =
{
	DEFINE_FIELD( CLavaGod, m_bActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CLavaGod, m_fRadius, FIELD_FLOAT ),
	DEFINE_FIELD( CLavaGod, m_fMinDropFreq, FIELD_FLOAT ),
	DEFINE_FIELD( CLavaGod, m_fMaxDropFreq, FIELD_FLOAT ),
	DEFINE_FIELD( CLavaGod, m_fMaxVel, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CLavaGod, CBaseEntity );

LINK_ENTITY_TO_CLASS( lava_god, CLavaGod );

void CLavaGod::Spawn( void )
{
	Precache();

	pev->angles = g_vecZero;
	pev->effects |= EF_NODRAW;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	UTIL_SetOrigin( pev, pev->origin );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );

	pev->nextthink = gpGlobals->time + 1;

	if ( pev->spawnflags & 1 )
		LavaOff();
	else
		LavaOn();
}

void CLavaGod::Precache( void )
{
	PRECACHE_MODEL( "models/lavaball.mdl" );
	g_sModelIndexLavaTrail = PRECACHE_MODEL( "sprites/laserbeam.spr" );
}

void CLavaGod::KeyValue( KeyValueData *pkvd )
{
	// sic: separate ifs, as in func_wind
	if ( FStrEq( pkvd->szKeyName, "dropradius" ) )
	{
		m_fRadius = atof( pkvd->szValue );
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
	if ( FStrEq( pkvd->szKeyName, "maxvelocity" ) )
	{
		m_fMaxVel = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CLavaGod::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( IsOn() )
		LavaOff();
	else
		LavaOn();
}

void CLavaGod::Think( void )
{
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( m_fMinDropFreq, m_fMaxDropFreq );

	if ( IsOn() )
	{
		// somewhere in the square around the god, up to dropradius out
		Vector vecSrc;
		vecSrc = pev->origin + RANDOM_LONG( 0, m_fRadius ) * Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), 0 );

		CLava *pLava = CLava::CreateLava( pev, vecSrc );
		pLava->pev->velocity.z
 = RANDOM_FLOAT( 25, m_fMaxVel );
		pLava->LavaTrail();
	}
}

void CLavaGod::LavaOff( void )
{
	m_bActive = FALSE;
}

void CLavaGod::LavaOn( void )
{
	m_bActive = TRUE;
}

BOOL CLavaGod::IsOn( void )
{
	return m_bActive;
}

CLava *CLava::CreateLava( entvars_t *pevOwner, Vector vecOrigin )
{
	CLava *pLava = GetClassPtr( (CLava *)NULL );

	pLava->Spawn();
	UTIL_SetOrigin( pLava->pev, vecOrigin );

	return pLava;
}

void CLava::Spawn( void )
{
	pev->classname = MAKE_STRING( "lava" );
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;

	SET_MODEL( ENT( pev ), "models/lavaball.mdl" );
	SetTouch( &CLava::KillTouch );

	pev->gravity = RANDOM_FLOAT( 0.5, 1.25 );
	pev->velocity.x = RANDOM_FLOAT( -1, 1 );
	pev->velocity.y = RANDOM_FLOAT( -1, 1 );
	pev->velocity = pev->velocity * RANDOM_LONG( 50, 100 );

	pev->nextthink = gpGlobals->time + 0.1;
}

void CLava::KillTouch( CBaseEntity *pOther )
{
	SetTouch( NULL );
	UTIL_Remove( this );
}

void CLava::Think( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
	UTIL_ParticleEffect( pev->origin, Vector( 0, 0, -1 ), 2, 20 );
}

void CLava::LavaTrail( void )
{
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( g_sModelIndexLavaTrail );	// model
		WRITE_BYTE( 6 );	// life
		WRITE_BYTE( RANDOM_LONG( 2, 6 ) );	// width
		WRITE_BYTE( RANDOM_LONG( 160, 240 ) );	// r
		WRITE_BYTE( RANDOM_LONG( 20, 80 ) );	// g
		WRITE_BYTE( RANDOM_LONG( 0, 24 ) );	// b
		WRITE_BYTE( RANDOM_LONG( 100, 130 ) );	// brightness
	MESSAGE_END();
}
