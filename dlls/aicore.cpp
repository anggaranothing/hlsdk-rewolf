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
// the AI core and the wall plug it goes into
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"

extern int gmsgWeapPickup;
extern int gEvilImpulse101;

void FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity );

int g_fAiCorePlugged;	// the plug has finished taking the core

#define SF_AIWALLPLUG_ACTIVATE		256	// fire the targets as soon as the core is in

class CAiWallPlug : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int BloodColor( void ) { return DONT_BLEED; }

	int PlugIn( entvars_t *pevActivator );
	void EXPORT AiWallPlugThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CBaseEntity *m_pActivator;
	int m_iUsed;
};
LINK_ENTITY_TO_CLASS( button_aiwallplug, CAiWallPlug );

TYPEDESCRIPTION	CAiWallPlug::m_SaveData[] =
{
	DEFINE_FIELD( CAiWallPlug, m_pActivator, FIELD_CLASSPTR ),
	DEFINE_FIELD( CAiWallPlug, m_iUsed, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CAiWallPlug, CBaseMonster );


void CAiWallPlug::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT(pev), "models/aiwallplug.mdl" );
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ) );
	pev->takedamage = DAMAGE_NO;
	pev->classname = MAKE_STRING( "button_aiwallplug" );
	pev->frame = 1;
	pev->body = 1;
	pev->sequence = 0;
	ResetSequenceInfo( );
	m_iUsed = 0;
	m_pActivator = NULL;
	SetTouch( NULL );
	SetThink( NULL );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CAiWallPlug::Precache( void )
{
	PRECACHE_MODEL( "models/aiwallplug.mdl" );
	PRECACHE_SOUND( "mainframe/aiplug_activate_gs.wav" );
	PRECACHE_SOUND( "mainframe/aiplug_deactivate_gs.wav" );
	UTIL_PrecacheOther( "entity_aicoreTemp" );
}

//=========================================================
// PlugIn - returns 1 if already used, 2 if the targets
// fire at once, 0 if they fire when the plug is done
//=========================================================
int CAiWallPlug::PlugIn( entvars_t *pevActivator )
{
	if ( !m_iUsed )
	{
		m_iUsed = 1;
		pev->frame = 0;
		pev->sequence = LookupSequence( "activating" );
		ResetSequenceInfo( );
		pev->body = 0;
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "mainframe/aiplug_activate_gs.wav", 1, ATTN_NORM );

		m_pActivator = CBaseEntity::Instance( pevActivator );

		if ( FBitSet( pev->spawnflags, SF_AIWALLPLUG_ACTIVATE ) )
		{
			pev->sequence = LookupSequence( "active" );
			ResetSequenceInfo( );
			if ( m_pActivator )
				SUB_UseTargets( m_pActivator, USE_ON, 0 );
			return 2;
		}

		pev->sequence = LookupSequence( "deactivating" );
		ResetSequenceInfo( );
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "mainframe/aiplug_deactivate_gs.wav", 1, ATTN_NORM );

		if ( pevActivator )
		{
			// hold the player still while the plug works
			pevActivator->velocity = g_vecZero;
			pevActivator->flags |= FL_FROZEN;
			UTIL_SetOrigin( pevActivator, pevActivator->origin );
		}
		return 0;
	}
	return 1;
}

void CAiWallPlug::AiWallPlugThink( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_iUsed != 2 && m_fSequenceFinished )
	{
		m_iUsed = 2;
		SUB_UseTargets( m_pActivator, USE_ON, 0 );
		pev->sequence = LookupSequence( "inactive" );
		ResetSequenceInfo( );
		pev->body = 1;
		pev->skin = 1;

		CBaseEntity *pCore = DropItem( "entity_aicoreTemp", pev->origin, pev->angles );
		if ( pCore )
		{
			UTIL_MakeVectors( pev->angles );
			pCore->pev->velocity = gpGlobals->v_forward * 75;
			pCore->pev->owner = edict();
		}

		if ( m_pActivator )
		{
			m_pActivator->pev->flags &= ~FL_FROZEN;
			UTIL_SetOrigin( m_pActivator->pev, m_pActivator->pev->origin );
		}

		g_fAiCorePlugged = TRUE;
	}
}


//=========================================================
// the AI core
//=========================================================
enum aicore_e {
	AICORE_IDLE = 0,
	AICORE_PLUGIN,
	AICORE_DRAW,
};

class CAicore : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 9; }
	int GetItemInfo(ItemInfo *p);
	BOOL Deploy( void );
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void WeaponIdle( void );
	void Holster( int skiplocal = 0 );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int m_fUsed;
	float m_flRetireWeapon;
	CAiWallPlug *m_WallPlug;
	TraceResult m_trHit;
	float m_flNextDeploy;
	float m_flNextAttackAnim;
};
LINK_ENTITY_TO_CLASS( weapon_aicore, CAicore );

TYPEDESCRIPTION	CAicore::m_SaveData[] =
{
	DEFINE_FIELD( CAicore, m_flRetireWeapon, FIELD_TIME ),
	DEFINE_FIELD( CAicore, m_fUsed, FIELD_INTEGER ),
	DEFINE_FIELD( CAicore, m_WallPlug, FIELD_CLASSPTR ),
	DEFINE_FIELD( CAicore, m_flNextDeploy, FIELD_TIME ),
	DEFINE_FIELD( CAicore, m_flNextAttackAnim, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CAicore, CBasePlayerWeapon );


void CAicore::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_aicore");
	Precache( );
	SET_MODEL(ENT(pev), "models/w_aicore.mdl");
	m_iId = WEAPON_AICORE;
	m_iClip = -1;
	m_fUsed = 0;
	m_flRetireWeapon = 0;
	m_flNextDeploy = 0;

	FallInit();// get ready to fall down.
}

void CAicore::Precache( void )
{
	PRECACHE_MODEL("models/v_aicore.mdl");
	PRECACHE_MODEL("models/w_aicore.mdl");
	PRECACHE_SOUND("buttons/button7.wav");
}

int CAicore::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 8;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_AICORE;
	p->iWeight = -1;
	return 1;
}

BOOL CAicore::Deploy( )
{
	m_flNextDeploy = 0;
	return DefaultDeploy( "models/v_aicore.mdl", "models/v_aicore.mdl", AICORE_DRAW, "aicore" );
}

int CAicore::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		if ( !gEvilImpulse101 )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
				WRITE_BYTE( m_iId );
			MESSAGE_END();
		}
		return TRUE;
	}
	return FALSE;
}


void CAicore::PrimaryAttack( void )
{
	if ( m_flRetireWeapon != 0 && m_flRetireWeapon < gpGlobals->time )
	{
		// the use animation is done: is there a plug in front of us?
		TraceResult tr;

		UTIL_MakeVectors (m_pPlayer->pev->v_angle);
		Vector vecSrc	= m_pPlayer->GetGunPosition( );
		Vector vecEnd	= vecSrc + gpGlobals->v_forward * 32;

		UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

		if ( tr.flFraction >= 1.0 )
		{
			UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
			if ( tr.flFraction < 1.0 )
			{
				CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
				if ( !pHit || pHit->IsBSPModel() )
					FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
				vecEnd = tr.vecEndPos;
			}
		}

		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		if ( pEntity && FClassnameIs( pEntity->pev, "button_aiwallplug" ) )
		{
			m_WallPlug = (CAiWallPlug *)pEntity;
			m_trHit = tr;
			m_flNextPrimaryAttack = gpGlobals->time + 0.1;
			m_fUsed = 1;
			m_flTimeWeaponIdle = gpGlobals->time + 8.0;

			int iResult = m_WallPlug->PlugIn( m_pPlayer->pev );
			m_WallPlug->SetThink( &CAiWallPlug::AiWallPlugThink );
			m_WallPlug->pev->nextthink = gpGlobals->time + 0.1;

			if ( iResult == 0 )
			{
				RetireWeapon( );
				m_flRetireWeapon = 0;
				return;
			}
			else if ( iResult == 1 )
			{
				EMIT_SOUND( ENT(pev), CHAN_WEAPON, "buttons/button7.wav", 1, ATTN_NORM );
				m_flRetireWeapon = 0;
				return;
			}
			else if ( iResult == 2 )
			{
				RetireWeapon( );
				m_pPlayer->pev->weapons &= ~(1<<WEAPON_AICORE);
				SetThink( &CBasePlayerItem::DestroyItem );
				pev->nextthink = gpGlobals->time + 0.1;
			}
			m_flRetireWeapon = 0;
			return;
		}
		m_flRetireWeapon = 0;
	}
	else
	{
		m_flNextPrimaryAttack = gpGlobals->time + 0.1;
	}

	if ( m_flNextAttackAnim < gpGlobals->time )
	{
		SendWeaponAnim( AICORE_PLUGIN );
		m_flNextAttackAnim = gpGlobals->time + 1.0;
		m_flRetireWeapon = gpGlobals->time + 0.6;
	}

	m_flNextPrimaryAttack = gpGlobals->time;
	m_flTimeWeaponIdle = gpGlobals->time + 4.0;
}


void CAicore::WeaponIdle( void )
{
	if ( g_fAiCorePlugged )
	{
		g_fAiCorePlugged = FALSE;
		Deploy( );
		m_flTimeWeaponIdle = gpGlobals->time + 2.0;
		return;
	}

	if ( m_flRetireWeapon != 0 && m_flRetireWeapon < gpGlobals->time )
	{
		TraceResult tr;

		UTIL_MakeVectors (m_pPlayer->pev->v_angle);
		Vector vecSrc	= m_pPlayer->GetGunPosition( );
		Vector vecEnd	= vecSrc + gpGlobals->v_forward * 32;

		UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

		if ( tr.flFraction >= 1.0 )
		{
			UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
			if ( tr.flFraction < 1.0 )
			{
				CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
				if ( !pHit || pHit->IsBSPModel() )
					FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
				vecEnd = tr.vecEndPos;
			}
		}

		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		if ( pEntity && FClassnameIs( pEntity->pev, "button_aiwallplug" ) )
		{
			m_WallPlug = (CAiWallPlug *)pEntity;
			m_trHit = tr;
			m_flNextPrimaryAttack = gpGlobals->time + 0.1;
			m_fUsed = 1;
			m_flTimeWeaponIdle = gpGlobals->time + 8.0;

			int iResult = m_WallPlug->PlugIn( m_pPlayer->pev );
			m_WallPlug->SetThink( &CAiWallPlug::AiWallPlugThink );
			m_WallPlug->pev->nextthink = gpGlobals->time + 0.1;

			if ( iResult == 0 )
			{
				RetireWeapon( );
				m_flNextDeploy = gpGlobals->time + 2.2;
				m_flRetireWeapon = 0;
				return;
			}
			else if ( iResult == 1 )
			{
				EMIT_SOUND( ENT(pev), CHAN_WEAPON, "buttons/button7.wav", 1, ATTN_NORM );
				m_flRetireWeapon = 0;
				return;
			}
			else if ( iResult == 2 )
			{
				RetireWeapon( );
				m_pPlayer->pev->weapons &= ~(1<<WEAPON_AICORE);
				SetThink( &CBasePlayerItem::DestroyItem );
				pev->nextthink = gpGlobals->time + 0.1;
			}
			m_flRetireWeapon = 0;
			return;
		}
		m_flRetireWeapon = 0;
	}

	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle <= gpGlobals->time )
	{
		m_flTimeWeaponIdle = gpGlobals->time + 3.2;
		SendWeaponAnim( AICORE_IDLE );
	}
}


void CAicore::Holster( int skiplocal )
{
	m_flRetireWeapon = 0;
	CBasePlayerWeapon::Holster( skiplocal );
}


//=========================================================
// the core the plug spits out when it is done
//=========================================================
class CAicoreTemp : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT DestroyTouch( CBaseEntity *pOther );
};
LINK_ENTITY_TO_CLASS( entity_aicoreTemp, CAicoreTemp );

void CAicoreTemp::Spawn( void )
{
	Precache( );
	pev->takedamage = DAMAGE_NO;
	SET_MODEL( ENT(pev), "models/w_aicore.mdl" );
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	SetThink( NULL );
	SetTouch( &CAicoreTemp::DestroyTouch );
	ResetSequenceInfo( );
}

void CAicoreTemp::Precache( void )
{
	PRECACHE_MODEL( "models/w_aicore.mdl" );
}

void CAicoreTemp::DestroyTouch( CBaseEntity *pOther )
{
	if ( pOther && pOther->edict() != pev->owner )
		UTIL_Remove( this );
}
