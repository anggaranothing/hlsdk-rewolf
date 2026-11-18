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
// leaf_god: an invisible brush that drops leaves at random
// places around it; inside a func_wind, the leaves blow
// with its wind
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "funcwind.h"

// a falling leaf, gone when it touches anything
class CLeaf : public CBaseEntity
{
public:
	void Spawn( void );
	virtual int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
	void EXPORT KillTouch( CBaseEntity *pOther );
	void EXPORT LeafThink( void );

	static CLeaf *CreateLeaf( entvars_t *pevOwner, Vector vecOrigin );

	Vector	*m_pWind;		// the func_wind's wind, or NULL
	Vector	m_vecDir;		// set when dropped, never read
};

LINK_ENTITY_TO_CLASS( leaf, CLeaf );

class CLeafGod : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT PreWindThink( void );
	void EXPORT PostWindThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	Vector	*m_pWind;		// the func_wind we are in, found by PreWindThink
	float	m_fDropRad;
	float	m_fMinDropFreq;
	float	m_fMaxDropFreq;
};

LINK_ENTITY_TO_CLASS( leaf_god, CLeafGod );

TYPEDESCRIPTION	CLeafGod::m_SaveData[] =
{
	DEFINE_FIELD( CLeafGod, m_fMinDropFreq, FIELD_FLOAT ),
	DEFINE_FIELD( CLeafGod, m_fMaxDropFreq, FIELD_FLOAT ),
	DEFINE_FIELD( CLeafGod, m_fDropRad, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CLeafGod, CBaseEntity );

void CLeafGod::Spawn( void )
{
	Precache();

	pev->angles = g_vecZero;
	pev->effects |= EF_NODRAW;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	UTIL_SetOrigin( pev, pev->origin );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );

	// let the func_winds spawn first
	SetThink( &CLeafGod::PreWindThink );
	pev->nextthink = gpGlobals->time + 1;
}

void CLeafGod::Precache( void )
{
	PRECACHE_MODEL( "models/leaf.mdl" );
}

void CLeafGod::KeyValue( KeyValueData *pkvd )
{
	// sic: separate ifs, as in func_wind
	if ( FStrEq( pkvd->szKeyName, "dropradius" ) )
	{
		m_fDropRad = atof( pkvd->szValue );
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
	else
		CBaseEntity::KeyValue( pkvd );
}

// find the func_wind whose volume holds us
void CLeafGod::PreWindThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	CBaseEntity *pWind = UTIL_FindEntityByClassname( NULL, "func_wind" );
	while ( pWind )
	{
		if ( pev->origin.x < pWind->pev->absmax.x && pev->origin.x > pWind->pev->absmin.x &&
			pev->origin.y < pWind->pev->absmax.y && pev->origin.y > pWind->pev->absmin.y &&
			pev->origin.z < pWind->pev->absmax.z && pev->origin.z > pWind->pev->absmin.z )
			break;

		pWind = UTIL_FindEntityByClassname( pWind, "func_wind" );
	}

	if ( pWind )
		m_pWind = &((CFuncWind *)pWind)->m_Wind;
	else
		m_pWind = NULL;

	SetThink( &CLeafGod::PostWindThink );
}

void CLeafGod::PostWindThink( void )
{
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( m_fMinDropFreq, m_fMaxDropFreq );

	Vector vecSrc = pev->origin + RANDOM_LONG( 0, m_fDropRad ) * Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), 0 );

	if ( UTIL_PointContents( vecSrc ) != CONTENTS_SOLID )
	{
		CLeaf *pLeaf = CLeaf::CreateLeaf( pev, vecSrc );
		pLeaf->m_pWind = m_pWind;

		if ( m_pWind )
			pLeaf->m_vecDir = m_pWind->Normalize() * RANDOM_FLOAT( 0.6, 1.4 ) + Vector( RANDOM_FLOAT( -0.2, 0.2 ), RANDOM_FLOAT( -0.2, 0.2 ), 0 );
		else
			pLeaf->m_vecDir.Normalize();	// sic: the result is dropped
	}
}

CLeaf *CLeaf::CreateLeaf( entvars_t *pevOwner, Vector vecOrigin )
{
	CLeaf *pLeaf = GetClassPtr( (CLeaf *)NULL );

	UTIL_SetSize( pLeaf->pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	UTIL_SetOrigin( pLeaf->pev, vecOrigin );
	pLeaf->Spawn();

	return pLeaf;
}

void CLeaf::Spawn( void )
{
	pev->classname = MAKE_STRING( "leaf" );
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;

	SET_MODEL( ENT( pev ), "models/leaf.mdl" );

	pev->gravity = RANDOM_FLOAT( 1, 2 );
	pev->avelocity.x = RANDOM_FLOAT( -150, 150 );
	pev->avelocity.y = RANDOM_FLOAT( -150, 150 );
	pev->avelocity.z = RANDOM_FLOAT( -150, 150 );

	SetTouch( &CLeaf::KillTouch );
	SetThink( &CLeaf::LeafThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CLeaf::KillTouch( CBaseEntity *pOther )
{
	SetTouch( NULL );
	UTIL_Remove( this );
}

// blow with the wind, spinning faster the stronger it is
void CLeaf::LeafThink( void )
{
	if ( m_pWind )
	{
		pev->nextthink = gpGlobals->time + 0.05;
		pev->velocity = *m_pWind;
		pev->avelocity = pev->avelocity.Normalize() * ( m_pWind->Length() * 10 + 90 );
	}
}
