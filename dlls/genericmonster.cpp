/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// Generic Monster - purely for scripted sequence work.
//=========================================================
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"

// For holograms, make them not solid so the player can walk through them
#define	SF_GENERICMONSTER_NOTSOLID					4 
#define	SF_GENERICMONSTER_POINTENTITY				32 // no size, flies, can't be hurt

//=========================================================
// Monster's Anim Events Go Here
//=========================================================

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
LINK_ENTITY_TO_CLASS( monster_generic, CGenericMonster );

//=========================================================
// CanPlaySentence - scripts can always make it talk
//=========================================================
int CGenericMonster :: CanPlaySentence( BOOL fDisregardState )
{
	return TRUE;
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CGenericMonster :: Classify ( void )
{
	return	CLASS_NONE;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CGenericMonster :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
	default:
		ys = 90;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CGenericMonster :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case 0:
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// ISoundMask - generic monster can't hear.
//=========================================================
int CGenericMonster :: ISoundMask ( void )
{
	return	NULL;
}

//=========================================================
// Spawn
//=========================================================
void CGenericMonster :: Spawn()
{
	Precache();

	SET_MODEL( ENT(pev), STRING(pev->model) );

/*
	if ( FStrEq( STRING(pev->model), "models/player.mdl" ) )
		UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);
	else
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
*/

	if ( FStrEq( STRING(pev->model), "models/player.mdl" ) || FStrEq( STRING(pev->model), "models/holo.mdl" ) )
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
	else
		UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->health			= 8;
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	MonsterInit();

	pev->takedamage = DAMAGE_YES;

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
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CGenericMonster :: Precache()
{
	PRECACHE_MODEL( (char *)STRING(pev->model) );
}	

//=========================================================
// CFlashlightMonsterBeam - the light cone of a monster's
// flashlight
//=========================================================
class CFlashlightMonsterBeam : public CBaseMonster
{
public:
	void Spawn( void );
	BOOL IsAlive( void ) { return FALSE; }

	void EXPORT SetAdditive( void );
	void EXPORT SetNormal( void );

	static CFlashlightMonsterBeam *BeamCreate( CBaseEntity *pOwner );
};
LINK_ENTITY_TO_CLASS( flashlight_beam, CFlashlightMonsterBeam );

//=========================================================
// CFlashlightMonster - a generic monster holding a
// flashlight
//=========================================================
class CFlashlightMonster : public CGenericMonster
{
public:
	void Spawn( void );
};
LINK_ENTITY_TO_CLASS( monster_flashlight, CFlashlightMonster );

void CFlashlightMonster :: Spawn( void )
{
	CGenericMonster::Spawn();
	SetBodygroup( 2, 1 );
}

void CFlashlightMonsterBeam :: Spawn( void )
{
	pev->classname = MAKE_STRING( "flashlight_beam" );
	PRECACHE_MODEL( "models/flashbeam.mdl" );
	SET_MODEL( ENT(pev), "models/flashbeam.mdl" );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_NO;
	pev->effects = 0;
	pev->health = 60;
	pev->rendermode = kRenderTransAdd;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 255;
	pev->velocity = g_vecZero;
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	SetTouch( NULL );
}

void CFlashlightMonsterBeam :: SetAdditive( void )
{
	pev->rendermode = kRenderTransAdd;
	SetThink( &CFlashlightMonsterBeam::SetNormal );
	pev->nextthink = gpGlobals->time + 2.0;
}

void CFlashlightMonsterBeam :: SetNormal( void )
{
	pev->rendermode = kRenderNormal;
	SetThink( &CFlashlightMonsterBeam::SetAdditive );
	pev->nextthink = gpGlobals->time + 2.0;
}

CFlashlightMonsterBeam *CFlashlightMonsterBeam :: BeamCreate( CBaseEntity *pOwner )
{
	CFlashlightMonsterBeam *pBeam = GetClassPtr( (CFlashlightMonsterBeam *)NULL );
	if ( !pBeam )
		return NULL;

	pBeam->Spawn();
	UTIL_SetOrigin( pBeam->pev, pOwner->pev->origin );
	pBeam->pev->owner = pOwner->edict();
	pBeam->pev->skin = ENTINDEX( pOwner->edict() );
	pBeam->pev->body = 0;
	pBeam->pev->aiment = pOwner->edict();
	pBeam->pev->movetype = MOVETYPE_FOLLOW;
	pBeam->pev->rendermode = kRenderNormal;
	pBeam->pev->renderfx = kRenderFxNone;
	pBeam->pev->renderamt = 255;
	pBeam->pev->rendercolor.x = 255;
	pBeam->pev->rendercolor.y = 255;
	pBeam->pev->rendercolor.z = 255;
	UTIL_SetOrigin( pBeam->pev, pBeam->pev->origin );

	return pBeam;
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
