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
// the DML grenade: thrown, it bursts or bounces; set as a
// tripmine, it waits for its beam to be crossed. Either can
// carry cluster bombs.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "effects.h"
#include "dmlrocket.h"
#include "dmlgrenade.h"
#include "gamerules.h"
#include "soundent.h"

extern int gmsgWeapPickup;
extern int gmsgSliderX;

enum dmlgrenade_e {
	DMLGRENADE_IDLE = 0,
	DMLGRENADE_IDLE2,
	DMLGRENADE_IDLEFIDGET,
	DMLGRENADE_TAKEOUT,
	DMLGRENADE_ARMING,
	DMLGRENADE_THROW,
	DMLGRENADE_TRIPMINE,
	DMLGRENADE_DRAW
};


class CTripmineGrenade : public CGrenade
{
	void Spawn( void );
	void Precache( void );

#ifndef CLIENT_DLL
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];
#endif

	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	void EXPORT TripmineTouch( CBaseEntity *pOther );
	void EXPORT WarningThink( void );
	void EXPORT PowerupThink( void );
	void EXPORT BeamBreakThink( void );
	void EXPORT DelayDeathThink( void );
	void Killed( entvars_t *pevAttacker, int iGib );

	void MakeBeam( void );
	void KillBeam( void );
	void TripExplode( TraceResult *pTrace, int bitsDamageType );
	void ShootClusters( void );

	float		m_flPowerUp;
	Vector		m_vecDir;
	Vector		m_vecEnd;
	float		m_flBeamLength;

	EHANDLE		m_hOwner;
	CBeam		*m_pBeam;
	Vector		m_posOwner;
	Vector		m_angleOwner;
	edict_t		*m_pRealOwner;// tracelines don't hit PEV->OWNER, which means a player couldn't detonate his own trip mine, so we store the owner here.
	Vector		m_vOrigin;		// where the beam starts
	float		m_flMakeSolidTime;

public:
	int			m_iPayload;
	int			m_iExplosion;	// explosion sprite, from the launcher
	int			m_iUnused;
};

LINK_ENTITY_TO_CLASS( monster_tripmine, CTripmineGrenade );


class CDmlHandGrenade : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );

#ifndef CLIENT_DLL
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
#endif

	static CDmlHandGrenade *ShootTimed( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, float time, int iExplosion, int iPayload, int iDetonate );

	void EXPORT DmlGrenExplodeTouch( CBaseEntity *pOther );
	void EXPORT DmlHandGrenTumbleThink( void );
	void EXPORT DmlHandGrenDetonate( void );
	void DmlGrenExplode( TraceResult *pTrace, int bitsDamageType );
	void ShootClusters( void );

	int		m_iUnused274;
	int		m_iUnused278;
	int		m_iPayload;
	int		m_iDetonate;
	int		m_iExplosion;
	int		m_iSmoke;
};

LINK_ENTITY_TO_CLASS( DmlHandGrenade, CDmlHandGrenade );


#ifndef CLIENT_DLL
TYPEDESCRIPTION	CTripmineGrenade::m_SaveData[] =
{
	DEFINE_FIELD( CTripmineGrenade, m_flPowerUp, FIELD_TIME ),
	DEFINE_FIELD( CTripmineGrenade, m_vecDir, FIELD_VECTOR ),
	DEFINE_FIELD( CTripmineGrenade, m_vecEnd, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CTripmineGrenade, m_flBeamLength, FIELD_FLOAT ),
	DEFINE_FIELD( CTripmineGrenade, m_hOwner, FIELD_EHANDLE ),
	DEFINE_FIELD( CTripmineGrenade, m_pBeam, FIELD_CLASSPTR ),
	DEFINE_FIELD( CTripmineGrenade, m_posOwner, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CTripmineGrenade, m_angleOwner, FIELD_VECTOR ),
	DEFINE_FIELD( CTripmineGrenade, m_pRealOwner, FIELD_EDICT ),
	DEFINE_FIELD( CTripmineGrenade, m_vOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( CTripmineGrenade, m_iPayload, FIELD_INTEGER ),
	DEFINE_FIELD( CTripmineGrenade, m_flMakeSolidTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE(CTripmineGrenade,CGrenade);
#endif


//=========================================================
// the weapon
//=========================================================
LINK_ENTITY_TO_CLASS( weapon_dmlGrenade, CDmlGrenade );

#ifndef CLIENT_DLL
TYPEDESCRIPTION	CDmlGrenade::m_SaveData[] =
{
	DEFINE_FIELD( CDmlGrenade, m_flTimePlacedTripmine, FIELD_TIME ),
	DEFINE_FIELD( CDmlGrenade, m_iModelIndexRocketExpl, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlGrenade, m_iModelIndexSmoke, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlGrenade, m_Config.detonate, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlGrenade, m_Config.payload, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlGrenade, m_flEjectShell1, FIELD_TIME ),
	DEFINE_FIELD( CDmlGrenade, m_flEjectShell2, FIELD_TIME ),
	DEFINE_FIELD( CDmlGrenade, m_iShell1, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlGrenade, m_iShell2, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlGrenade, m_flTakeoutAnim, FIELD_TIME ),
	DEFINE_FIELD( CDmlGrenade, m_flReleaseThrow, FIELD_TIME ),
	DEFINE_FIELD( CDmlGrenade, m_flStartThrow, FIELD_TIME ),
	DEFINE_FIELD( CDmlGrenade, m_flTimeNextCustomAnim, FIELD_TIME ),
	DEFINE_FIELD( CDmlGrenade, m_bRetired, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlGrenade, m_flTimeWeaponIdleThrow, FIELD_TIME ),
	DEFINE_FIELD( CDmlGrenade, m_fDeployTakeout, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CDmlGrenade, CBasePlayerWeapon );


TYPEDESCRIPTION	CDmlHandGrenade::m_SaveData[] =
{
	DEFINE_FIELD( CDmlHandGrenade, m_iPayload, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlHandGrenade, m_iDetonate, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CDmlHandGrenade, CGrenade );
#endif


//=========================================================
// the thrown grenade
//=========================================================
CDmlHandGrenade *CDmlHandGrenade::ShootTimed( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, float time, int iExplosion, int iPayload, int iDetonate )
{
	CDmlHandGrenade *pGrenade = GetClassPtr( (CDmlHandGrenade *)NULL );
	pGrenade->Spawn();
	UTIL_SetOrigin( pGrenade->pev, vecStart );
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles(pGrenade->pev->velocity);
	pGrenade->pev->owner = ENT(pevOwner);

	if ( iDetonate == 2 )
	{
		// timed: bounce until it goes off
		pGrenade->SetTouch( &CGrenade::BounceTouch );
		pGrenade->SetThink( &CDmlHandGrenade::DmlHandGrenTumbleThink );
	}
	else
	{
		pGrenade->SetTouch( &CDmlHandGrenade::DmlGrenExplodeTouch );
		pGrenade->SetThink( NULL );
	}

	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->pev->nextthink = gpGlobals->time + 0.1;
	if (time < 0.1 && iDetonate == 2)
	{
		pGrenade->pev->nextthink = gpGlobals->time;
		pGrenade->pev->velocity = Vector( 0, 0, 0 );
	}

	pGrenade->pev->sequence = RANDOM_LONG( 3, 6 );
	pGrenade->pev->framerate = 1.0;

	pGrenade->pev->gravity = 0.5;
	pGrenade->pev->friction = 0.8;

	SET_MODEL(ENT(pGrenade->pev), "models/grenadecore.mdl");
	pGrenade->pev->dmg = 100;

	pGrenade->m_iDetonate = iDetonate;
	pGrenade->m_iPayload = iPayload;

	return pGrenade;
}

void CDmlHandGrenade::Spawn( void )
{
	Precache( );
	CGrenade::Spawn( );
	pev->classname = MAKE_STRING( "DmlHandGrenade" );
}

void CDmlHandGrenade::Precache( void )
{
	CGrenade::Precache( );
	m_iExplosion = PRECACHE_MODEL( "sprites/part1.spr" );
	m_iSmoke = PRECACHE_MODEL( "sprites/part2.spr" );
}

void CDmlHandGrenade::DmlGrenExplodeTouch( CBaseEntity *pOther )
{
	TraceResult tr;
	Vector		vecSpot;// trace starts here!

	vecSpot = pev->origin + Vector ( 0 , 0 , 8 );
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -40 ),  ignore_monsters, ENT(pev), & tr);

	DmlGrenExplode( &tr, DMG_BLAST );
}

void CDmlHandGrenade::DmlGrenExplode( TraceResult *pTrace, int bitsDamageType )
{
	float		flRndSound;// sound randomizer

	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible

	pev->takedamage = DAMAGE_NO;

	if ( m_iPayload == 2 )
	{
		ShootClusters( );
		return;
	}

	// Pull out of the wall a bit
	if ( pTrace->flFraction != 1.0 )
	{
		pev->origin = pTrace->vecEndPos + (pTrace->vecPlaneNormal * (pev->dmg - 24) * 0.6);
	}

	int iContents = UTIL_PointContents ( pev->origin );

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( pev->origin.x );	// Send to PAS because of the sound
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( m_iExplosion );
		WRITE_BYTE( 20  ); // scale * 10
		WRITE_BYTE( 15  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END();

	CSprite *pSprite = CSprite::SpriteCreate( "sprites/part2.spr", pev->origin, TRUE );
	pSprite->SetScale( 4 );
	pSprite->AnimateAndDie( 10 );
	pSprite->SetTransparency( kRenderTransAlpha, 255, 255, 255, 255, kRenderFxNoDissipation );
	pSprite->pev->frame = 0;
	UTIL_SetOrigin( pSprite->pev, pev->origin );
	pSprite->pev->origin.z += 16;

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	RadiusDamage ( pev, pevOwner, pev->dmg, CLASS_NONE, bitsDamageType );

	flRndSound = RANDOM_FLOAT( 0 , 1 );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
		case 0:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/kabam1.wav", 1, ATTN_NORM);	break;
		case 1:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/kabam2.wav", 1, ATTN_NORM);	break;
		case 2:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/kabam3.wav", 1, ATTN_NORM);	break;
	}

	pev->effects |= EF_NODRAW;
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 0.3;

	if (iContents != CONTENTS_WATER)
	{
		int sparkCount = RANDOM_LONG(0,3);
		for ( int i = 0; i < sparkCount; i++ )
			Create( "spark_shower", pev->origin, pTrace->vecPlaneNormal, NULL );
	}

	SetTouch( NULL );
	UTIL_Remove( this );
}

void CDmlHandGrenade::DmlHandGrenTumbleThink( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime - 1 < gpGlobals->time)
	{
		CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 400, 0.1 );
	}

	if (pev->dmgtime <= gpGlobals->time)
	{
		SetThink( &CDmlHandGrenade::DmlHandGrenDetonate );
	}
	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}
}

void CDmlHandGrenade::DmlHandGrenDetonate( void )
{
	TraceResult tr;
	Vector		vecSpot;// trace starts here!

	vecSpot = pev->origin + Vector ( 0 , 0 , 8 );
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -40 ),  ignore_monsters, ENT(pev), & tr);

	DmlGrenExplode( &tr, DMG_BLAST );
}

//=========================================================
// ShootClusters - the cluster payload: four bombs, thrown
// up off the floor, or away from the wall that was hit
//=========================================================
void CDmlHandGrenade::ShootClusters( void )
{
	Vector vecForward;
	TraceResult tr;
	Vector vecEnd;
	int i;

	if ( m_iDetonate == 2 )
	{
		pev->angles.x = RANDOM_FLOAT( -90, -70 );
		pev->angles.y = RANDOM_FLOAT( -180, 180 );
		pev->angles.z = 0;
		UTIL_MakeVectors( pev->angles );
		vecForward = gpGlobals->v_forward;

		vecEnd = pev->origin + vecForward * 2048;	// sic: unused
		UTIL_TraceLine( pev->origin, pev->origin + vecForward * 128, dont_ignore_monsters, pev->owner, &tr );

		if ( tr.flFraction < 1.0 )
		{
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( pHit && !( pHit->pev->flags & FL_CONVEYOR ) )
			{
				Vector angles = UTIL_VecToAngles( tr.vecPlaneNormal );
				UTIL_MakeVectors( angles );
				vecForward = gpGlobals->v_forward;
			}
		}

		for ( i = 0; i < 4; i++ )
		{
			CClusterGrenade *pCluster = CClusterGrenade::CreateCluster( );
			pCluster->pev->owner = pev->owner;
			pCluster->pev->velocity = vecForward * RANDOM_LONG( 50, 250 ) + gpGlobals->v_right * RANDOM_LONG( -200, 200 );
			pCluster->pev->origin = pev->origin + vecForward * RANDOM_LONG( 5, 20 ) + gpGlobals->v_right * RANDOM_LONG( -15, 15 );
		}
	}
	else
	{
		UTIL_MakeVectors( pev->angles );
		vecForward = gpGlobals->v_forward;

		vecEnd = pev->origin + vecForward * 2048;	// sic: unused
		UTIL_TraceLine( pev->origin, pev->origin + vecForward * 128, dont_ignore_monsters, pev->owner, &tr );

		if ( tr.vecPlaneNormal.z > 0 )
		{
			// on the floor: go off as if timed
			m_iDetonate = 2;
			ShootClusters( );
			return;
		}

		if ( tr.flFraction < 1.0 )
		{
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( pHit && !( pHit->pev->flags & FL_CONVEYOR ) )
			{
				Vector angles( UTIL_VecToAngles( tr.vecPlaneNormal ) );
				UTIL_MakeVectors( angles );
				vecForward = gpGlobals->v_forward;
			}
		}

		for ( i = 0; i < 4; i++ )
		{
			CClusterGrenade *pCluster = CClusterGrenade::CreateCluster( );
			pCluster->pev->owner = pev->owner;
			pCluster->pev->velocity = vecForward * RANDOM_LONG( 50, 250 ) + gpGlobals->v_right * RANDOM_LONG( -200, 200 );
			pCluster->pev->origin = pev->origin + vecForward * RANDOM_LONG( 5, 20 ) + gpGlobals->v_right * RANDOM_LONG( -15, 15 );
		}
	}

	EMIT_SOUND( ENT(pev), CHAN_ITEM, "weapons/dml_fragment.wav", 1, ATTN_NORM );
	SetTouch( NULL );
	SetThink( NULL );
	UTIL_Remove( this );
}


void CDmlGrenade::Spawn( )
{
	Precache( );
	m_iId = WEAPON_DMLGRENADE;
	SET_MODEL(ENT(pev), "models/w_grenade.mdl");

	pev->dmg = gSkillData.plrDmgDmlGrenade;

	m_iDefaultAmmo = 0;
	pev->body = 1;

	FallInit();// get ready to fall down.

	m_flEjectShell1 = 0;
	m_flEjectShell2 = 0;
	m_flTimePlacedTripmine = 0;
	m_flTimeNextCustomAnim = 0;
	m_bRetired = FALSE;
	m_Config.payload = 1;
	m_Config.detonate = 2;
	m_fDeployTakeout = TRUE;
}


void CDmlGrenade::Precache( void )
{
	PRECACHE_MODEL("models/w_grenade.mdl");
	PRECACHE_MODEL("models/v_grenade.mdl");
	PRECACHE_MODEL("models/p_grenade.mdl");

	m_iShell1 = PRECACHE_MODEL("models/shelltip.mdl");
	m_iShell2 = PRECACHE_MODEL("models/shellengine.mdl");
	m_iModelIndexRocketExpl = PRECACHE_MODEL("sprites/part1.spr");
	m_iModelIndexSmoke = PRECACHE_MODEL("sprites/part2.spr");
	PRECACHE_MODEL("models/grenadecore.mdl");

	UTIL_PrecacheOther( "dml_cluster" );
	UTIL_PrecacheOther( "DmlHandGrenade" );

	PRECACHE_SOUND("weapons/rocket1.wav");
	PRECACHE_SOUND("weapons/kabam1.wav");
	PRECACHE_SOUND("weapons/kabam2.wav");
	PRECACHE_SOUND("weapons/kabam3.wav");
	PRECACHE_SOUND("weapons/kaboom1.wav");
	PRECACHE_SOUND("weapons/kaboom2.wav");
	PRECACHE_SOUND("weapons/kaboom3.wav");
	PRECACHE_SOUND("weapons/dml_fragment.wav");

	UTIL_PrecacheOther( "monster_tripmine" );
}

int CDmlGrenade::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "missiles";
	p->iMaxAmmo1 = 6;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 6;
	p->iPosition = 0;
	p->iId = WEAPON_DMLGRENADE;
	p->iFlags = 0;
	p->iWeight = -1;

	return 1;
}


BOOL CDmlGrenade::Deploy( )
{
	if ( m_flStartThrow == 0 )
		m_flReleaseThrow = -1;

	if ( m_fDeployTakeout )
	{
		// bring a grenade out right away
		m_flNextPrimaryAttack = gpGlobals->time + 1.43;
		m_flTakeoutAnim = gpGlobals->time - 0.1;
		BOOL fResult = DefaultDeploy( "models/v_grenade.mdl", "models/p_grenade.mdl", DMLGRENADE_DRAW, "crowbar" );
		m_flTimeWeaponIdle = gpGlobals->time - 0.1;
		WeaponIdle( );
		m_fDeployTakeout = FALSE;
		return fResult;
	}

	return DefaultDeploy( "models/v_grenade.mdl", "models/p_grenade.mdl", DMLGRENADE_DRAW, "crowbar" );
}

BOOL CDmlGrenade::CanHolster( void )
{
	// can only holster hand grenades when not primed!
	return ( m_flStartThrow == 0 );
}

void CDmlGrenade::Holster( int skiplocal /* = 0 */ )
{
	if ( m_flEjectShell1 != 0 || m_flEjectShell2 != 0 )
		m_fDeployTakeout = TRUE;

	if ( m_flStartThrow != 0 )
		m_fDeployTakeout = FALSE;

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	m_flEjectShell1 = 0;
	m_flEjectShell2 = 0;
	m_flStartThrow = 0;

	CBasePlayerWeapon::Holster( skiplocal );
}

void CDmlGrenade::PrimaryAttack()
{
	if ( m_flTakeoutAnim == 0 && m_flEjectShell1 == 0 && m_flEjectShell2 == 0 )
	{
		if ( m_Config.detonate == 3 )
		{
			PlaceTripmine( );
			return;
		}

		if ( !m_flStartThrow && m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] > 0 )
		{
			m_flStartThrow = gpGlobals->time;
			m_flReleaseThrow = 0;

			SendWeaponAnim( DMLGRENADE_ARMING );
			m_flTimeWeaponIdleThrow = gpGlobals->time + 0.5;
			m_fDeployTakeout = TRUE;
		}
	}
	else
	{
		WeaponIdle( );
	}
}


void CDmlGrenade::WeaponIdle( void )
{
	if ( gpGlobals->time - m_flTimePlacedTripmine > 1.0 && m_flTimePlacedTripmine != 0 )
	{
		PlaceTripmine( );
		m_flTimePlacedTripmine = 0;
		return;
	}

	if ( m_flTimePlacedTripmine != 0 )
		return;

	if ( gpGlobals->time >= m_flTakeoutAnim && m_flTakeoutAnim != 0 && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		// a new grenade: take it out and let the shells fall
		m_fDeployTakeout = FALSE;
		pev->body = 1;
		m_flTimeWeaponIdle = gpGlobals->time + 3.35;
		m_flNextPrimaryAttack = gpGlobals->time;
		SendWeaponAnim( DMLGRENADE_TAKEOUT );
		m_flTakeoutAnim = 0;
		m_flEjectShell1 = gpGlobals->time + 0.733;
		m_flEjectShell2 = gpGlobals->time + 1.43;
		m_flTimeNextCustomAnim = gpGlobals->time + 3.35;
		return;
	}

	if (m_flReleaseThrow == 0)
		m_flReleaseThrow = gpGlobals->time;

	if ( m_Config.detonate == 3 )
	{
		m_flReleaseThrow = 0;
		m_flStartThrow = 0;
	}

	if ( m_flTimeWeaponIdleThrow != 0 && m_flTimeWeaponIdleThrow > gpGlobals->time )
		return;

	if (m_flStartThrow)
	{
		Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

		if (angThrow.x < 0)
			angThrow.x = -10 + angThrow.x * ((90 - 10) / 90.0);
		else
			angThrow.x = -10 + angThrow.x * ((90 + 10) / 90.0);

		float flVel = (120 - angThrow.x) * 4;
		if (flVel > 600)
			flVel = 600;

		UTIL_MakeVectors( angThrow );

		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;

		Vector vecThrow = gpGlobals->v_forward * flVel + m_pPlayer->pev->velocity;

		// alway explode 3.5 seconds after the pin was pulled
		float time = m_flStartThrow - gpGlobals->time + 3.5;
		if (time < 0)
			time = 0;

		CDmlHandGrenade::ShootTimed( m_pPlayer->pev, vecSrc, vecThrow, time, m_iModelIndexRocketExpl, m_Config.payload, m_Config.detonate );

		pev->body = 1;
		SendWeaponAnim( DMLGRENADE_THROW );

		// sic: never runs. Retail inlines more of the second shell's vector
		// math than the live code leaves budget for, so there was dead code
		// in the function; the SDK's choice of throw animation is a guess
		if ( 0 )
		{
			if ( flVel < 500 )
				SendWeaponAnim( DMLGRENADE_THROW );
			else if ( flVel < 1000 )
				SendWeaponAnim( DMLGRENADE_THROW );
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		m_flStartThrow = 0;
		m_flNextPrimaryAttack = gpGlobals->time;
		m_flTakeoutAnim = gpGlobals->time + 100;
		m_flTimeWeaponIdleThrow = gpGlobals->time + 0.3;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		if ( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		{
			// just threw last grenade
			// set attack times in the future, and weapon idle in the future so we can see the whole throw
			// animation, weapon idle will automatically retire the weapon for us.
			m_flTimeWeaponIdleThrow = m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + 0.5;// ensure that the animation can finish playing
		}
		return;
	}
	else if (m_flReleaseThrow > 0)
	{
		// we've finished the throw, restart.
		m_flStartThrow = 0;

		if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		{
			if ( m_bRetired )
			{
				m_flNextPrimaryAttack = gpGlobals->time;
				m_flTakeoutAnim = gpGlobals->time;
				Deploy( );
				m_bRetired = FALSE;
				m_flReleaseThrow = -1;
				return;
			}
			else
			{
				m_flNextPrimaryAttack = gpGlobals->time;
				m_flTakeoutAnim = gpGlobals->time;
				m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
				m_flReleaseThrow = -1;
				return;
			}
		}
		else
		{
			RetireWeapon();
			m_bRetired = TRUE;
			return;
		}

		// sic: never reached (every branch above returns); the SDK's tail,
		// part of the dead code noted at the throw
		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
		m_flReleaseThrow = -1;
		return;
	}
	else
	{
		if ( gpGlobals->time > m_flEjectShell1 && m_flEjectShell1 != 0 )
		{
			// the tip of the shell
			UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

			Vector vecShellVelocity = m_pPlayer->pev->velocity + gpGlobals->v_right * RANDOM_FLOAT(50,70) + gpGlobals->v_up * RANDOM_FLOAT(50,75) + gpGlobals->v_forward * 25;

			EjectBrass ( m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_up * -8 + gpGlobals->v_forward * 20 + gpGlobals->v_right * 9 , vecShellVelocity, pev->angles.y, m_iShell1, TE_BOUNCE_SHOTSHELL );
			m_flEjectShell1 = 0;
			return;
		}

		if ( gpGlobals->time > m_flEjectShell2 && m_flEjectShell2 != 0 )
		{
			// the engine
			UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

			Vector vecShellVelocity = m_pPlayer->pev->velocity + gpGlobals->v_right * RANDOM_FLOAT(-50,-70) + gpGlobals->v_up * RANDOM_FLOAT(25,50) + gpGlobals->v_forward * 25;

			EjectBrass ( m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_up * -13 + gpGlobals->v_forward * 20 + gpGlobals->v_right * -4 , vecShellVelocity, pev->angles.y, m_iShell2, TE_BOUNCE_SHOTSHELL );
			m_flEjectShell2 = 0;
			return;
		}

		pev->body = 0;

		if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] && m_bRetired )
		{
			// it was retired: bring a grenade out again
			m_fDeployTakeout = TRUE;
			m_flNextPrimaryAttack = gpGlobals->time;
			m_flTakeoutAnim = gpGlobals->time;
			Deploy( );
			m_bRetired = FALSE;
			m_flReleaseThrow = -1;
			return;
		}
	}

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = RANDOM_FLOAT(0, 1);
		if (flRand <= 0.75)
		{
			iAnim = DMLGRENADE_IDLE;
			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );// how long till we do this again.
		}
		else
		{
			iAnim = DMLGRENADE_IDLEFIDGET;
			m_flTimeWeaponIdle = gpGlobals->time + 75.0 / 30.0;
		}

		SendWeaponAnim( iAnim );
	}
}


int CDmlGrenade::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();

		pPlayer->GiveNamedItem( "cust_1DMLGrenDetonate" );
		pPlayer->GiveNamedItem( "cust_2DMLGrenPayload" );

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_GRENDETONATE );
			WRITE_BYTE( m_Config.detonate );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_GRENPAYLOAD );
			WRITE_BYTE( m_Config.payload );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}


void CDmlGrenade::Customize( char cOption, int iValue )
{
	if ( m_flTimeNextCustomAnim < gpGlobals->time )
		m_flTimeNextCustomAnim = gpGlobals->time + 2.0;

	int iOldDetonate = m_Config.detonate;
	m_flTimeWeaponIdle = gpGlobals->time + 1.4;

	switch ( cOption )
	{
	case '1':
		m_Config.detonate = iValue;
		break;
	case '2':
		m_Config.payload = iValue;
		break;
	}

	// switched from tripmine to throwing: start over
	if ( m_flStartThrow == 0 && m_flReleaseThrow == 0 && iOldDetonate == 3 && m_Config.detonate != 3 )
		m_flReleaseThrow = -1;

	m_flTakeoutAnim = 0;
}


//=========================================================
// PlaceTripmine - stick a grenade to the wall in front, as
// a tripmine
//=========================================================
void CDmlGrenade::PlaceTripmine( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
	{
		m_bRetired = TRUE;
		RetireWeapon( );
		return;
	}

	pev->body = 0;

	if ( m_flTimePlacedTripmine == 0 )
	{
		m_flTimePlacedTripmine = gpGlobals->time;
		SendWeaponAnim( DMLGRENADE_TRIPMINE );
		return;
	}

	if ( gpGlobals->time - m_flTimePlacedTripmine < 1.0 )
		return;

	m_flTimePlacedTripmine = 0;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = gpGlobals->v_forward;

	TraceResult tr;

	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 128, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	if (tr.flFraction < 1.0)
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		if (pEntity && !(pEntity->pev->flags & FL_CONVEYOR) && FClassnameIs( ENT( pEntity->pev ), "worldspawn" ))
		{
			Vector angles = UTIL_VecToAngles( tr.vecPlaneNormal );

			CBaseEntity *pEnt = CBaseEntity::Create( "monster_tripmine", tr.vecEndPos + tr.vecPlaneNormal * 8, angles, m_pPlayer->edict() );
			((CTripmineGrenade *)pEnt)->m_iPayload = m_Config.payload;
			((CTripmineGrenade *)pEnt)->m_iExplosion = m_iModelIndexRocketExpl;

			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

			m_flTimeWeaponIdle = gpGlobals->time + 1.4;
			m_fDeployTakeout = TRUE;
			m_flTakeoutAnim = gpGlobals->time + 0.1;

			if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
			{
				pev->body = 1;
				m_flTakeoutAnim = gpGlobals->time;
				SendWeaponAnim( DMLGRENADE_DRAW );
				m_flNextPrimaryAttack = gpGlobals->time;
				m_flTimeWeaponIdle = gpGlobals->time;
				return;
			}

			m_bRetired = TRUE;
			RetireWeapon( );
			return;
		}
	}

	m_flNextPrimaryAttack = gpGlobals->time + 1.4;
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 1, 5 );
}


#ifndef CLIENT_DLL
void CDmlGrenade::UpdateOwner( void )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_GRENDETONATE );
		WRITE_BYTE( m_Config.detonate );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_GRENPAYLOAD );
		WRITE_BYTE( m_Config.payload );
	MESSAGE_END();
}
#endif


//=========================================================
// the tripmine
//=========================================================
void CTripmineGrenade :: Spawn( void )
{
	pev->classname = MAKE_STRING( "monster_tripmine" );
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_NOT;

	SET_MODEL(ENT(pev), "models/grenadecore.mdl");
	pev->frame = 0;
	pev->body = 0;
	pev->sequence = 0;
	ResetSequenceInfo( );
	pev->framerate = 0;

	UTIL_SetSize(pev, Vector( -8, -8, -8), Vector(8, 8, 8));
	UTIL_SetOrigin( pev, pev->origin );

	if (pev->spawnflags & 1)
	{
		// power up quickly
		m_flPowerUp = gpGlobals->time + 1.0;
	}
	else
	{
		// power up in 2.5 seconds
		m_flPowerUp = gpGlobals->time + 2.5;
	}

	SetThink( &CTripmineGrenade::PowerupThink );
	pev->nextthink = gpGlobals->time + 0.2;

	pev->takedamage = DAMAGE_YES;
	pev->dmg = gSkillData.plrDmgDmlGrenade * 1.5;
	pev->health = 1; // don't let die normally

	if (pev->owner != NULL)
	{
		// play deploy sound
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/mine_deploy.wav", 1.0, ATTN_NORM );
		EMIT_SOUND( ENT(pev), CHAN_BODY, "weapons/mine_charge.wav", 0.2, ATTN_NORM ); // chargeup

		m_pRealOwner = pev->owner;// see CTripmineGrenade for why.
	}

	UTIL_MakeAimVectors( pev->angles );

	m_vecDir = gpGlobals->v_forward;
	m_vecEnd = pev->origin + m_vecDir * 2048;

	SetTouch( &CTripmineGrenade::TripmineTouch );
}


void CTripmineGrenade :: Precache( void )
{
	PRECACHE_MODEL("models/grenadecore.mdl");
	PRECACHE_SOUND("weapons/mine_deploy.wav");
	PRECACHE_SOUND("weapons/mine_activate.wav");
	PRECACHE_SOUND("weapons/mine_charge.wav");
	PRECACHE_MODEL("sprites/dustyred.spr");
}


void CTripmineGrenade :: TripmineTouch( CBaseEntity *pOther )
{
	// let a player that walks into it pass for a moment
	if ( pOther && ( pOther->pev->flags & FL_CLIENT ) )
	{
		pev->solid = SOLID_NOT;
		m_flMakeSolidTime = gpGlobals->time + 0.5;
	}
}


void CTripmineGrenade :: WarningThink( void  )
{
	// play warning sound
	// EMIT_SOUND( ENT(pev), CHAN_VOICE, "buttons/Blip2.wav", 1.0, ATTN_NORM );

	// set to power up
	SetThink( &CTripmineGrenade::PowerupThink );
	pev->nextthink = gpGlobals->time + 1.0;
}


void CTripmineGrenade :: PowerupThink( void  )
{
	TraceResult tr;

	if (m_hOwner == NULL)
	{
		// find an owner
		edict_t *oldowner = pev->owner;
		pev->owner = NULL;
		UTIL_TraceLine( pev->origin + m_vecDir * 8, pev->origin - m_vecDir * 32, dont_ignore_monsters, ENT( pev ), &tr );
		if (tr.fStartSolid || (oldowner && tr.pHit == oldowner))
		{
			pev->owner = oldowner;
			m_flPowerUp += 0.1;
			pev->nextthink = gpGlobals->time + 0.1;
			return;
		}
		if (tr.flFraction < 1.0)
		{
			pev->owner = tr.pHit;
			m_hOwner = CBaseEntity::Instance( pev->owner );
			m_posOwner = m_hOwner->pev->origin;
			m_angleOwner = m_hOwner->pev->angles;
		}
		else
		{
			STOP_SOUND( ENT(pev), CHAN_VOICE, "weapons/mine_deploy.wav" );
			STOP_SOUND( ENT(pev), CHAN_BODY, "weapons/mine_charge.wav" );
			SetThink( &CBaseEntity::SUB_Remove );
			pev->nextthink = gpGlobals->time + 0.1;
			ALERT( at_console, "WARNING:Tripmine at %.0f, %.0f, %.0f removed\n", pev->origin.x, pev->origin.y, pev->origin.z );
			KillBeam();
			return;
		}
	}
	else if (m_posOwner != m_hOwner->pev->origin || m_angleOwner != m_hOwner->pev->angles)
	{
		// disable
		STOP_SOUND( ENT(pev), CHAN_VOICE, "weapons/mine_deploy.wav" );
		STOP_SOUND( ENT(pev), CHAN_BODY, "weapons/mine_charge.wav" );
		CBaseEntity *pMine = Create( "weapon_tripmine", pev->origin + m_vecDir * 24, pev->angles );	// no such weapon in Gunman (sic)
		if ( pMine )
			pMine->pev->spawnflags |= SF_NORESPAWN;

		SetThink( &CBaseEntity::SUB_Remove );
		KillBeam();
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}
	// ALERT( at_console, "%d %.0f %.0f %0.f\n", pev->owner, m_pOwner->pev->origin.x, m_pOwner->pev->origin.y, m_pOwner->pev->origin.z );

	if (gpGlobals->time > m_flPowerUp)
	{
		// make solid
		pev->solid = SOLID_BBOX;
		UTIL_SetOrigin( pev, pev->origin );

		MakeBeam( );

		// play enabled sound
        EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/mine_activate.wav", 0.5, ATTN_NORM, 1.0, 75 );
	}
	pev->nextthink = gpGlobals->time + 0.1;
}


void CTripmineGrenade :: KillBeam( void )
{
	if ( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}
}


void CTripmineGrenade :: MakeBeam( void )
{
	TraceResult tr;

	// the beam starts a little off the mine
	UTIL_MakeVectors( pev->angles );
	m_vOrigin = pev->origin + gpGlobals->v_right * -2 + gpGlobals->v_up * 2;

	UTIL_TraceLine( m_vOrigin, m_vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	m_flBeamLength = tr.flFraction;

	// set to follow laser spot
	SetThink( &CTripmineGrenade::BeamBreakThink );
	pev->nextthink = gpGlobals->time + 0.1;

	Vector vecTmpEnd = m_vOrigin + m_vecDir * 2048 * m_flBeamLength;

	m_pBeam = CBeam::BeamCreate( "sprites/dustyred.spr", 10 );
	m_pBeam->PointEntInit( vecTmpEnd, entindex() );
	m_pBeam->SetColor( 255, 0, 0 );
	m_pBeam->SetScrollRate( 150 );
	m_pBeam->SetBrightness( 255 );
}


void CTripmineGrenade :: BeamBreakThink( void  )
{
	BOOL bBlowup = 0;

	TraceResult tr;

	pev->solid = SOLID_BBOX;
	if ( m_flMakeSolidTime < gpGlobals->time )
		SetTouch( NULL );

	// HACKHACK Set simple box using this really nice global!
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine( m_vOrigin, m_vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	// ALERT( at_console, "%f : %f\n", tr.flFraction, m_flBeamLength );

	// respawn detect.
	if ( !m_pBeam )
	{
		MakeBeam( );
		if ( tr.pHit )
			m_hOwner = CBaseEntity::Instance( tr.pHit );	// reset owner too
	}

	if (fabs( m_flBeamLength - tr.flFraction ) > 0.001)
	{
		bBlowup = 1;
	}
	else
	{
		if (m_hOwner == NULL)
			bBlowup = 1;
		else if (m_posOwner != m_hOwner->pev->origin)
			bBlowup = 1;
		else if (m_angleOwner != m_hOwner->pev->angles)
			bBlowup = 1;
	}

	if (bBlowup)
	{
		// a bit of a hack, but all CGrenade code passes pev->owner along to make sure the proper player gets credit for the kill
		// so we have to restore pev->owner from pRealOwner, because an entity's tracelines don't strike it's pev->owner which meant
		// that a player couldn't trigger his own tripmine. Now that the mine is exploding, it's safe the restore the owner so the
		// CGrenade code knows who the explosive really belongs to.
		pev->owner = m_pRealOwner;
		pev->health = 0;
		Killed( VARS( pev->owner ), GIB_NORMAL );
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

int CTripmineGrenade :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if (gpGlobals->time < m_flPowerUp && flDamage < pev->health)
	{
		// disable
		// Create( "weapon_tripmine", pev->origin + m_vecDir * 24, pev->angles );
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1;
		KillBeam();
		return FALSE;
	}
	return CGrenade::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CTripmineGrenade::Killed( entvars_t *pevAttacker, int iGib )
{
	pev->takedamage = DAMAGE_NO;

	if ( pevAttacker && ( pevAttacker->flags & FL_CLIENT ) )
	{
		// some client has destroyed this mine, he'll get credit for any kills
		pev->owner = ENT( pevAttacker );
	}

	SetThink( &CTripmineGrenade::DelayDeathThink );
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1, 0.3 );

	EMIT_SOUND( ENT(pev), CHAN_BODY, "common/null.wav", 0.5, ATTN_NORM ); // shut off chargeup
}


void CTripmineGrenade::DelayDeathThink( void )
{
	KillBeam();
	TraceResult tr;
	UTIL_TraceLine ( pev->origin + m_vecDir * 8, pev->origin - m_vecDir * 64,  dont_ignore_monsters, ENT(pev), & tr);

	TripExplode( &tr, DMG_BLAST );
}


void CTripmineGrenade::TripExplode( TraceResult *pTrace, int bitsDamageType )
{
	float		flRndSound;// sound randomizer

	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible

	pev->takedamage = DAMAGE_NO;

	if ( m_iPayload == 2 )
	{
		ShootClusters( );
		return;
	}

	// Pull out of the wall a bit
	if ( pTrace->flFraction != 1.0 )
	{
		pev->origin = pTrace->vecEndPos + (pTrace->vecPlaneNormal * (pev->dmg - 24) * 0.6);
	}

	int iContents = UTIL_PointContents ( pev->origin );

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( pev->origin.x );	// Send to PAS because of the sound
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( m_iExplosion );
		WRITE_BYTE( 20  ); // scale * 10
		WRITE_BYTE( 15  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END();

	CSprite *pSprite = CSprite::SpriteCreate( "sprites/part2.spr", pev->origin, TRUE );
	pSprite->SetScale( 4 );
	pSprite->AnimateAndDie( 10 );
	pSprite->SetTransparency( kRenderTransAlpha, 255, 255, 255, 255, kRenderFxNoDissipation );
	pSprite->pev->frame = 0;
	UTIL_SetOrigin( pSprite->pev, pev->origin );
	pSprite->pev->origin.z += 16;

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	RadiusDamage ( pev, pevOwner, pev->dmg, CLASS_NONE, bitsDamageType );

	flRndSound = RANDOM_FLOAT( 0 , 1 );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
		case 0:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/kabam1.wav", 1, ATTN_NORM);	break;
		case 1:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/kabam2.wav", 1, ATTN_NORM);	break;
		case 2:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/kabam3.wav", 1, ATTN_NORM);	break;
	}

	pev->effects |= EF_NODRAW;
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 0.3;

	if (iContents != CONTENTS_WATER)
	{
		int sparkCount = RANDOM_LONG(0,3);
		for ( int i = 0; i < sparkCount; i++ )
			Create( "spark_shower", pev->origin, pTrace->vecPlaneNormal, NULL );
	}

	SetTouch( NULL );
	UTIL_Remove( this );
}


//=========================================================
// ShootClusters - as the thrown grenade's
//=========================================================
void CTripmineGrenade::ShootClusters( void )
{
	Vector vecForward;
	TraceResult tr;

	if ( pev->angles.x == 90.0 )
	{
		// on the floor
		pev->angles.x = RANDOM_FLOAT( -90, -70 );
		pev->angles.y = RANDOM_FLOAT( -90, 90 );
		pev->angles.z = 0;
	}

	UTIL_MakeVectors( pev->angles );
	vecForward = gpGlobals->v_forward;

	Vector vecEnd = pev->origin + vecForward * 2048;
	UTIL_TraceLine( pev->origin, pev->origin + vecForward * 128, dont_ignore_monsters, pev->owner, &tr );

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
		if ( pHit && !( pHit->pev->flags & FL_CONVEYOR ) )
		{
			Vector angles( UTIL_VecToAngles( tr.vecPlaneNormal ) );
			UTIL_MakeVectors( angles );
			vecForward = gpGlobals->v_forward;
		}
	}

	for ( int i = 0; i < 4; i++ )
	{
		CClusterGrenade *pCluster = CClusterGrenade::CreateCluster( );
		pCluster->pev->owner = pev->owner;
		pCluster->pev->velocity = vecForward * RANDOM_LONG( 50, 250 ) + gpGlobals->v_right * RANDOM_LONG( -200, 200 );
		pCluster->pev->origin = pev->origin + vecForward * RANDOM_LONG( 5, 20 ) + gpGlobals->v_right * RANDOM_LONG( -15, 15 );
	}

	EMIT_SOUND( ENT(pev), CHAN_ITEM, "weapons/dml_fragment.wav", 1, ATTN_NORM );
	SetTouch( NULL );
	SetThink( NULL );
	UTIL_Remove( this );
}


//=========================================================
// the DML grenade's two customization items: one class
//=========================================================
class CCustDmlGrenade : public CBaseCustomization
{
public:
	void Spawn( void );
	int GetItemInfo(ItemInfo *p);
};
LINK_ENTITY_TO_CLASS( cust_1DMLGrenDetonate, CCustDmlGrenade );
LINK_ENTITY_TO_CLASS( cust_2DMLGrenPayload, CCustDmlGrenade );

void CCustDmlGrenade::Spawn( )
{
	Precache( );

	if ( FClassnameIs( pev, "cust_1DMLGrenDetonate" ) )
		m_iId = WEAPON_CUST_GRENDETONATE;
	else if ( FClassnameIs( pev, "cust_2DMLGrenPayload" ) )
		m_iId = WEAPON_CUST_GRENPAYLOAD;

	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
}

int CCustDmlGrenade::GetItemInfo(ItemInfo *p)
{
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iWeight = CUSTOMIZATION_WEIGHT;
	p->iSlot = 9;
	p->iPosition = 0;
	p->iIsCustomization = 1;

	p->pszName = STRING(pev->classname);

	if ( FClassnameIs( pev, "cust_1DMLGrenDetonate" ) )
	{
		p->iId = WEAPON_CUST_GRENDETONATE;
		p->iPosition = 11;
		p->iCustomSettings = 3;
		p->iCustomDefault = 1;
		p->pszCustomizes = "weapon_dmlGrenade";
	}
	else if ( FClassnameIs( pev, "cust_2DMLGrenPayload" ) )
	{
		p->iId = WEAPON_CUST_GRENPAYLOAD;
		p->iPosition = 12;
		p->iCustomSettings = 2;
		p->iIsCustomization = 1;
		p->iCustomDefault = 1;
		p->pszCustomizes = "weapon_dmlGrenade";
	}

	p->iWeight = CUSTOMIZATION_WEIGHT;
	return 1;
}
