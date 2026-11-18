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
// entity_spritegod: while on, sprays sprites toward a point
// or toward a target entity (sparks, splashes, drips)
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

class CSpriteGod : public CBaseEntity
{
public:
	CSpriteGod( void );
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT UseToggle( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT SpewEvent( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t	m_iszSplashSprite;	// spritename
	string_t	m_iszTargetEnt;		// targetent: spray toward it
	int			m_iSprite;
	int			m_iNoise;
	int			m_iSpeed;
	int			m_iOn;
	int			m_iCount;			// sprites per spray
	int			m_iFrequency;		// tenths of a second between sprays, in theory
	Vector		m_vecDir;			// spritex/y/z: the point to spray toward
};

LINK_ENTITY_TO_CLASS( entity_spritegod, CSpriteGod );
LINK_ENTITY_TO_CLASS( test_entity2, CSpriteGod );

TYPEDESCRIPTION	CSpriteGod::m_SaveData[] =
{
	DEFINE_FIELD( CSpriteGod, m_iNoise, FIELD_INTEGER ),
	DEFINE_FIELD( CSpriteGod, m_iSpeed, FIELD_INTEGER ),
	DEFINE_FIELD( CSpriteGod, m_iOn, FIELD_INTEGER ),
	DEFINE_FIELD( CSpriteGod, m_iCount, FIELD_INTEGER ),
	DEFINE_FIELD( CSpriteGod, m_iFrequency, FIELD_INTEGER ),
	DEFINE_FIELD( CSpriteGod, m_vecDir, FIELD_VECTOR ),
	DEFINE_FIELD( CSpriteGod, m_iszSplashSprite, FIELD_STRING ),
	DEFINE_FIELD( CSpriteGod, m_iszTargetEnt, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CSpriteGod, CBaseEntity );

CSpriteGod::CSpriteGod( void )
{
	m_iNoise = 50;
	m_iSpeed = 0;
	m_iOn = 0;
	m_iCount = 1;
	m_iFrequency = 1;
	m_iszSplashSprite = ALLOC_STRING( "sprites/gibbeak.spr" );
	m_vecDir = Vector( 0, 0, 0 );
	m_iszTargetEnt = 0;
}

void CSpriteGod::Precache( void )
{
	PRECACHE_MODEL( "models/null.mdl" );

	if ( m_iszSplashSprite )
		m_iSprite = PRECACHE_MODEL( (char *)STRING( m_iszSplashSprite ) );
}

void CSpriteGod::Spawn( void )
{
	Precache();

	SET_MODEL( ENT( pev ), "models/null.mdl" );
	pev->takedamage = DAMAGE_NO;
	pev->effects |= EF_NODRAW;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	SetTouch( NULL );
	SetUse( &CSpriteGod::UseToggle );
	SetThink( &CSpriteGod::SpewEvent );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CSpriteGod::KeyValue( KeyValueData *pkvd )
{
	// sic: separate ifs, as in func_wind
	if ( FStrEq( pkvd->szKeyName, "spritename" ) )
	{
		m_iszSplashSprite = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "spritenoise" ) )
	{
		m_iNoise = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "spritespeed" ) )
	{
		m_iSpeed = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "spritestartstate" ) )
	{
		m_iOn = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "spritecount" ) )
	{
		m_iCount = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "spritefreq" ) )
	{
		m_iFrequency = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "spritex" ) )
	{
		m_vecDir.x = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "spritey" ) )
	{
		m_vecDir.y = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "spritex" ) )	// sic: spritez is never read, z takes x
	{
		m_vecDir.z = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "targetent" ) )
	{
		m_iszTargetEnt = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CSpriteGod::UseToggle( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !m_iOn )
	{
		m_iOn = 1;
		SetThink( &CSpriteGod::SpewEvent );
		pev->nextthink = gpGlobals->time + m_iFrequency / 10;	// sic: integer division
	}
	else
	{
		m_iOn = 0;
		SetThink( NULL );
	}
}

void CSpriteGod::SpewEvent( void )
{
	if ( !m_iOn )
	{
		SetThink( NULL );
		return;
	}

	Vector vecDir;

	if ( !m_iszTargetEnt )
	{
		vecDir = ( m_vecDir - pev->origin ).Normalize();
	}
	else
	{
		CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( m_iszTargetEnt ) );

		// sic: without the target, the direction is never set
		if ( pTarget )
			vecDir = ( pTarget->pev->origin - pev->origin ).Normalize();
	}

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE_SPRAY );
		WRITE_COORD( pev->origin.x );	// position
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( vecDir.x );	// direction
		WRITE_COORD( vecDir.y );
		WRITE_COORD( vecDir.z );
		WRITE_SHORT( m_iSprite );
		WRITE_BYTE( m_iCount );
		WRITE_BYTE( m_iSpeed );
		WRITE_BYTE( m_iNoise );
	MESSAGE_END();

	pev->nextthink = gpGlobals->time + m_iFrequency / 10;	// sic: integer division
}
