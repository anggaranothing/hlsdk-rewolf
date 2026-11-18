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
// boulder: a scripted rolling rock. Triggered, it drops,
// rolls along its angles, bumps down two steps, falls and
// settles, crushing what is near it all the way
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"


class CBoulder : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT DistibuteThinks( void );	// sic
	void EXPORT UseBoulder( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void BoulderDrop( void );
	void BoulderRoll( void );
	void BoulderBounce( void );
	void BoulderSink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	Vector	m_vecOldOrigin;	// where the current step of the path starts
	Vector	m_vecDir;		// rolling direction, from the angles
	float	m_fZOffSet;		// how far down the current step the rock is
	int		m_iStep;		// which step of the path
	int		m_iThink;		// 0 waiting, 1 drop, 2 falling, 3 rolling, 4 sinking
	float	m_fChangeThink;	// when the drop ends
};

LINK_ENTITY_TO_CLASS( boulder, CBoulder );

TYPEDESCRIPTION	CBoulder::m_SaveData[] =
{
	DEFINE_FIELD( CBoulder, m_vecOldOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( CBoulder, m_vecDir, FIELD_VECTOR ),
	DEFINE_FIELD( CBoulder, m_fZOffSet, FIELD_FLOAT ),
	DEFINE_FIELD( CBoulder, m_iStep, FIELD_INTEGER ),
	DEFINE_FIELD( CBoulder, m_iThink, FIELD_INTEGER ),
	DEFINE_FIELD( CBoulder, m_fChangeThink, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CBoulder, CBaseEntity );

void CBoulder::Precache( void )
{
	PRECACHE_MODEL( "models/rock.mdl" );
	PRECACHE_SOUND( "rock/mayan_rock.wav" );
}

void CBoulder::Spawn( void )
{
	Precache();

	pev->movetype = MOVETYPE_FLYMISSILE;
	pev->solid = SOLID_BBOX;

	UTIL_SetOrigin( pev, pev->origin );
	SET_MODEL( ENT( pev ), "models/rock.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	SetObjectCollisionBox();

	SetThink( NULL );
	SetTouch( NULL );
	SetUse( &CBoulder::UseBoulder );

	pev->dmg = 500;
	m_fZOffSet = 0;
	m_iStep = 0;

	UTIL_MakeVectors( pev->angles );
	m_vecDir = gpGlobals->v_forward;
	m_iThink = 0;
	m_fChangeThink = gpGlobals->time;
}

void CBoulder::DistibuteThinks( void )
{
	switch ( m_iThink )
	{
	case 1:
		BoulderDrop();
		m_iThink++;
		m_fChangeThink = gpGlobals->time + 332 / pev->velocity.Length();
		break;

	case 2:
		if ( m_fChangeThink < gpGlobals->time )
		{
			UTIL_ScreenShake( pev->origin, 24, 7, 0.25, 1200 );
			BoulderRoll();
			m_iThink++;
		}
		break;

	case 3:
		UTIL_ScreenShake( pev->origin, 12, 7, 0.25, 1200 );
		BoulderBounce();
		break;

	case 4:
		UTIL_ScreenShake( pev->origin, 12, 7, 0.25, 1200 );
		BoulderSink();
		break;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	::RadiusDamage( pev->origin, pev, NULL, 1000, 48, CLASS_NONE, DMG_CRUSH );
}

void CBoulder::UseBoulder( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetUse( NULL );
	pev->nextthink = gpGlobals->time + 0.1;
	SetThink( &CBoulder::DistibuteThinks );
	m_iThink++;
}

// fall straight down, 332 units
void CBoulder::BoulderDrop( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "rock/mayan_rock.wav", 1.0, 0.8, 0, PITCH_NORM );

	pev->avelocity = Vector( -200, 0, 0 );
	pev->velocity = Vector( 0, 0, -600 );
	m_vecOldOrigin = pev->origin - Vector( 0, 0, 332 );

}

void CBoulder::BoulderRoll( void )
{
	pev->avelocity = Vector( -400, 0, 0 );
	pev->velocity = m_vecDir * 350;
}

// roll on, bumping down the steps of the path; the distance
// rolled is measured along y only
void CBoulder::BoulderBounce( void )
{
	float flDist = pev->origin.y - m_vecOldOrigin.y;
	if ( flDist < 0 )
		flDist *= -1;

	if ( m_iStep == 0 )
	{
		if ( flDist > 144 )
		{
			m_vecOldOrigin = m_vecOldOrigin + m_vecDir * 144;
			m_iStep = 1;
		}
	}
	else if ( m_iStep == 1 )
	{
		if ( flDist > 266 )
		{
			m_vecOldOrigin = m_vecOldOrigin + m_vecDir * 224 - Vector( 0, 0, 144 );
			m_iStep = 2;
			m_fZOffSet = 0;
		}
		else
			m_fZOffSet = flDist * 0.6494;
	}
	else if ( m_iStep == 2 )
	{
		if ( flDist > 1520 )
		{
			pev->velocity = Vector( 0, 0, -100 );
			m_iThink++;
			return;
		}
	}

	pev->velocity.z -= 32;

	if ( m_vecOldOrigin.z - m_fZOffSet >= pev->origin.z )
		pev->velocity.z = RANDOM_FLOAT( 32, 48 );
}

// slow the fall until the rock rests
void CBoulder::BoulderSink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->velocity.z == 0 )
	{
		STOP_SOUND( ENT( pev ), CHAN_STATIC, "rock/mayan_rock.wav" );
		SetThink( NULL );
		return;
	}

	pev->velocity.z -= 40;
}
