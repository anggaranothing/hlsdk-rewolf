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
// sphere_explosion: when used, plays the sphere explosion
// event (the client draws the sphere of tracers), with an
// optional shockwave ring, screen shake and screen fade,
// then removes itself
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "sphereexplosion.h"

LINK_ENTITY_TO_CLASS( sphere_explosion, CSphereExplosion );

TYPEDESCRIPTION	CSphereExplosion::m_SaveData[] =
{
	DEFINE_FIELD( CSphereExplosion, m_flMaxRadius, FIELD_FLOAT ),
	DEFINE_FIELD( CSphereExplosion, m_flRadiusStep, FIELD_FLOAT ),
	DEFINE_FIELD( CSphereExplosion, m_iTrans, FIELD_INTEGER ),
	DEFINE_FIELD( CSphereExplosion, m_iTransStep, FIELD_INTEGER ),
	DEFINE_FIELD( CSphereExplosion, m_iTracers, FIELD_INTEGER ),
	DEFINE_FIELD( CSphereExplosion, m_bShockwave, FIELD_BOOLEAN ),
	DEFINE_FIELD( CSphereExplosion, m_bImplosion, FIELD_BOOLEAN ),
	DEFINE_FIELD( CSphereExplosion, m_iShakeDuration, FIELD_FLOAT ),
	DEFINE_FIELD( CSphereExplosion, m_vFadeColor, FIELD_VECTOR ),
	DEFINE_FIELD( CSphereExplosion, m_flFadeTime, FIELD_FLOAT ),
	DEFINE_FIELD( CSphereExplosion, m_flHoldTime, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CSphereExplosion, CBaseEntity );

CSphereExplosion::CSphereExplosion( void )
{
}

void CSphereExplosion::Precache( void )
{
	PRECACHE_MODEL( "models/null.mdl" );
	m_iShockwaveSprite = PRECACHE_MODEL( "sprites/clustershock.spr" );
	m_usSphereExplode = PRECACHE_EVENT( 1, "events/sphereexplode.sc" );
}

void CSphereExplosion::Spawn( void )
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
	SetThink( NULL );
	SetUse( &CSphereExplosion::UseToggle );
}

void CSphereExplosion::UseToggle( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( &CSphereExplosion::ExplodeEvent );
	pev->nextthink = gpGlobals->time;
}

CSphereExplosion *CSphereExplosion::CreateSphereExplosion( Vector vecOrigin, CBaseEntity *pOwner,
	float flMaxRadius, float flRadiusStep, int iTrans, int iTransStep, int iTracers,
	BOOL bShockwave, BOOL bImplosion, float flShakeDuration, float flFadeTime, float flHoldTime )
{
	CSphereExplosion *pSphere = GetClassPtr( (CSphereExplosion *)NULL );

	if ( !pSphere )
		return NULL;

	pSphere->Spawn();
	UTIL_SetOrigin( pSphere->pev, vecOrigin );

	if ( pOwner )
		pSphere->pev->owner = pOwner->edict();

	pSphere->m_flMaxRadius = flMaxRadius;
	pSphere->m_flRadiusStep = flRadiusStep;
	pSphere->m_iTrans = iTrans;
	pSphere->m_iTransStep = iTransStep;
	pSphere->m_iTracers = iTracers;
	pSphere->m_bImplosion = bImplosion;
	pSphere->m_iShakeDuration = flShakeDuration;
	pSphere->m_flFadeTime = flFadeTime;
	pSphere->m_flHoldTime = flHoldTime;
	pSphere->m_bShockwave = bShockwave;


	return pSphere;
}

void CSphereExplosion::KeyValue( KeyValueData *pkvd )
{
	// sic: separate ifs, as in func_wind
	if ( FStrEq( pkvd->szKeyName, "maxradius" ) )
	{
		m_flMaxRadius = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "radiusstep" ) )
	{
		m_flRadiusStep = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "trans" ) )
	{
		m_iTrans = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "transstep" ) )
	{
		m_iTransStep = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "numtracers" ) )
	{
		m_iTracers = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "shockwave" ) )
	{
		m_bShockwave = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "implosion" ) )
	{
		m_bImplosion = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "shakeduration" ) )
	{
		m_iShakeDuration = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "fadetime" ) )
	{
		m_flFadeTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "fadeholdtime" ) )
	{
		m_flHoldTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CSphereExplosion::ExplodeEvent( void )
{
	float flTime;

	PLAYBACK_EVENT_FULL( 0, ENT( pev ), m_usSphereExplode, 0, (float *)&g_vecZero, (float *)&g_vecZero,
		m_flMaxRadius, m_flRadiusStep, m_iTrans, m_iTransStep, m_iTracers, m_bImplosion );

	if ( m_bShockwave )
	{
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_BEAMCYLINDER );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z + 8 );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z + 8 + m_flMaxRadius * 1.5 );	// reach
			WRITE_SHORT( m_iShockwaveSprite );
			WRITE_BYTE( 0 );	// startframe
			WRITE_BYTE( 0 );	// framerate
			WRITE_BYTE( m_flMaxRadius / ( m_flRadiusStep * 30 ) * 10 );	// life: as long as the sphere grows
			WRITE_BYTE( 16 );	// width
			WRITE_BYTE( 0 );	// noise
			WRITE_BYTE( 255 );	// r, g, b
			WRITE_BYTE( 255 );
			WRITE_BYTE( 255 );
			WRITE_BYTE( 255 );	// brightness
			WRITE_BYTE( 0 );	// speed
		MESSAGE_END();
	}

	flTime = m_iShakeDuration;
	if ( flTime )
		UTIL_ScreenShake( pev->origin, 25, 150, flTime, 3600 );

	flTime = m_flFadeTime;
	if ( flTime )
		UTIL_ScreenFadeAll( m_vFadeColor, flTime, m_flHoldTime, 150, 0 );

	UTIL_Remove( this );
}
