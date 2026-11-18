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
// entity_clustergod and env_clusterExplosion: they throw
// the DML's cluster bombs, the god once on spawn, the
// explosion each time it is used
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "dmlrocket.h"

#define SF_CLUSTEREXPLOSION_REPEATABLE	2	// not removed after use

//=========================================================
// entity_clustergod - throws four cluster bombs a tenth of
// a second after it spawns, then removes itself
//=========================================================
class CClusterGod : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT ShootClusterGrenades( void );
};

LINK_ENTITY_TO_CLASS( entity_clustergod, CClusterGod );

void CClusterGod::Spawn( void )
{
	Precache( );

	pev->takedamage = DAMAGE_NO;
	SET_MODEL( ENT( pev ), "models/null.mdl" );
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ) );

	SetTouch( NULL );
	pev->nextthink = gpGlobals->time + 0.1;
	SetThink( &CClusterGod::ShootClusterGrenades );
	pev->health = 20;

	ResetSequenceInfo( );
}

void CClusterGod::Precache( void )
{
	PRECACHE_MODEL( "models/artillary.mdl" );
	UTIL_PrecacheOther( "dml_cluster" );
	PRECACHE_MODEL( "models/null.mdl" );
}

// the DML rocket's cluster payload, thrown up at a random angle
void CClusterGod::ShootClusterGrenades( void )
{
	TraceResult tr;
	Vector vecDir;
	Vector vecEnd;

	pev->angles.x = RANDOM_FLOAT( -90, -70 );
	pev->angles.y = RANDOM_FLOAT( -90, 90 );
	pev->angles.z = 0;

	UTIL_MakeVectors( pev->angles );
	vecDir = gpGlobals->v_forward;
	vecEnd = pev->origin + vecDir * 2048;	// sic: unused

	UTIL_TraceLine( pev->origin, pev->origin + vecDir * 128, dont_ignore_monsters, pev->owner, &tr );

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
		if ( pHit && !( pHit->pev->flags & FL_CONVEYOR ) )
		{
			Vector angles = UTIL_VecToAngles( tr.vecPlaneNormal );
			UTIL_MakeVectors( angles );
			vecDir = gpGlobals->v_forward;
		}
	}

	for ( int i = 0; i < 4; i++ )
	{
		CClusterGrenade *pCluster = CClusterGrenade::CreateCluster( );
		pCluster->pev->owner = pev->owner;
		pCluster->pev->velocity = vecDir * RANDOM_LONG( 50, 250 ) + gpGlobals->v_right * RANDOM_LONG( -200, 200 );
		pCluster->pev->origin = pev->origin + vecDir * RANDOM_LONG( 5, 20 ) + gpGlobals->v_right * RANDOM_LONG( -15, 15 );
		SET_MODEL( ENT( pCluster->pev ), "models/artillary.mdl" );
	}

	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 5.0;
}

//=========================================================
// env_clusterExplosion - throws numGrenades cluster bombs
// of clusterDamage each when used
//=========================================================
class CClusterExplosion : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void EXPORT ClusterExplode( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int m_iDamage;
	int m_iNumGrenades;
};

LINK_ENTITY_TO_CLASS( env_clusterExplosion, CClusterExplosion );

TYPEDESCRIPTION	CClusterExplosion::m_SaveData[] =
{
	DEFINE_FIELD( CClusterExplosion, m_iDamage, FIELD_INTEGER ),
	DEFINE_FIELD( CClusterExplosion, m_iNumGrenades, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CClusterExplosion, CBaseMonster );

void CClusterExplosion::KeyValue( KeyValueData *pkvd )
{
	// sic: separate ifs, as in func_wind
	if ( FStrEq( pkvd->szKeyName, "clusterDamage" ) )
	{
		m_iDamage = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "numGrenades" ) )
	{
		m_iNumGrenades = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CClusterExplosion::Spawn( void )
{
	Precache( );

	pev->classname = MAKE_STRING( "env_clusterExplosion" );
	SET_MODEL( ENT( pev ), "models/null.mdl" );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_NO;
	pev->effects = 0;
	pev->health = 60;
	pev->velocity = g_vecZero;
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	SetTouch( NULL );
	SetThink( NULL );
	SetUse( &CClusterExplosion::ClusterExplode );

	UTIL_SetOrigin( pev, pev->origin );
}

void CClusterExplosion::Precache( void )
{
	PRECACHE_MODEL( "models/null.mdl" );
	UTIL_PrecacheOther( "dml_cluster" );
}

void CClusterExplosion::ClusterExplode( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	TraceResult tr;
	Vector vecDir;
	Vector vecEnd;

	pev->angles.x = RANDOM_FLOAT( -90, -70 );
	pev->angles.y = RANDOM_FLOAT( -90, 90 );
	pev->angles.z = 0;

	UTIL_MakeVectors( pev->angles );
	vecDir = gpGlobals->v_forward;
	vecEnd = pev->origin + vecDir * 2048;	// sic: unused

	UTIL_TraceLine( pev->origin, pev->origin + vecDir * 128, dont_ignore_monsters, pev->owner, &tr );

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
		if ( pHit && !( pHit->pev->flags & FL_CONVEYOR ) )
		{
			Vector angles = UTIL_VecToAngles( tr.vecPlaneNormal );
			UTIL_MakeVectors( angles );
			vecDir = gpGlobals->v_forward;
		}
	}

	for ( int i = 0; i < m_iNumGrenades; i++ )
	{
		CClusterGrenade *pCluster = CClusterGrenade::CreateCluster( );
		pCluster->pev->owner = pev->owner;
		pCluster->pev->velocity = vecDir * RANDOM_LONG( 50, 250 ) + gpGlobals->v_right * RANDOM_LONG( -200, 200 );
		pCluster->pev->origin = pev->origin + vecDir * RANDOM_LONG( 5, 20 ) + gpGlobals->v_right * RANDOM_LONG( -15, 15 );
		pCluster->pev->dmg = m_iDamage;
	}

	if ( !( pev->spawnflags & SF_CLUSTEREXPLOSION_REPEATABLE ) )
		UTIL_Remove( this );
}
