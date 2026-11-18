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
// the DML (dual missile launcher, "mule"): a customizable
// rocket launcher, its laser spot and its cluster bombs
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "effects.h"
#include "decals.h"
#include "soundent.h"
#include "dmlrocket.h"

#include "dml.h"

extern int gmsgWeapPickup;
extern int gmsgSliderX;
extern int gmsgDmlCross;

enum dml_e {
	DML_IDLE = 0,
	DML_IDLEFIDGET,
	DML_RELOADBOTH,
	DML_RELOADLEFT,
	DML_RELOADRIGHT,
	DML_FIRE,
	DML_CUSTOMIZE,
	DML_DRAW
};

LINK_ENTITY_TO_CLASS( laser_spot, CLaserSpot );
LINK_ENTITY_TO_CLASS( dml_cluster, CClusterGrenade );

//=========================================================
//=========================================================
CLaserSpot *CLaserSpot::CreateSpot( void )
{
	CLaserSpot *pSpot = GetClassPtr( (CLaserSpot *)NULL );
	pSpot->Spawn();

	pSpot->pev->classname = MAKE_STRING("laser_spot");

	return pSpot;
}

//=========================================================
//=========================================================
void CLaserSpot::Spawn( void )
{
	Precache( );
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	pev->rendermode = kRenderGlow;
	pev->renderfx = kRenderFxNoDissipation;
	pev->renderamt = 255;

	SET_MODEL(ENT(pev), "sprites/laserdot.spr");
	UTIL_SetOrigin( pev, pev->origin );
};

//=========================================================
// Suspend- make the laser sight invisible.
//=========================================================
void CLaserSpot::Suspend( float flSuspendTime )
{
	pev->effects |= EF_NODRAW;

	SetThink( &CLaserSpot::Revive );
	pev->nextthink = gpGlobals->time + flSuspendTime;
}

//=========================================================
// Revive - bring a suspended laser sight back.
//=========================================================
void CLaserSpot::Revive( void )
{
	pev->effects &= ~EF_NODRAW;

	SetThink( NULL );
}

void CLaserSpot::Precache( void )
{
	PRECACHE_MODEL("sprites/laserdot.spr");
};


//=========================================================
// the cluster bomb
//=========================================================
CClusterGrenade *CClusterGrenade::CreateCluster( void )
{
	CClusterGrenade *pCluster = GetClassPtr( (CClusterGrenade *)NULL );

	pCluster->pev->classname = MAKE_STRING( "dml_cluster" );
	pCluster->Spawn();

	return pCluster;
}

void CClusterGrenade::Spawn( void )
{
	float flDetonate = RANDOM_FLOAT( 0.8, 4.0 );

	pev->dmg = gSkillData.plrDmgDmlRocket / 2.1;

	Precache( );
	pev->movetype = MOVETYPE_BOUNCE;
	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_BBOX;
	pev->gravity = 0.5;
	pev->friction = 0.6;

	SET_MODEL( ENT(pev), "models/dmlcluster.mdl" );
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	SetTouch( &CGrenade::BounceTouch );
	SetThink( &CClusterGrenade::TumbleClusterThink );
	pev->dmgtime = gpGlobals->time + flDetonate;
	pev->nextthink = gpGlobals->time + 0.1;
	pev->framerate = 1.0;
	m_fTrailOn = FALSE;
}

void CClusterGrenade::Precache( void )
{
	PRECACHE_MODEL( "models/dmlcluster.mdl" );
	m_iSmoke = PRECACHE_MODEL( "sprites/smoke.spr" );
	m_sKaboom = PRECACHE_MODEL( "sprites/kaboom.spr" );
	m_usClusterExplosion = PRECACHE_EVENT( 1, "events/ClusterExplosion.sc" );
	m_iClusterShock = PRECACHE_MODEL( "sprites/clustershock.spr" );
	PRECACHE_MODEL( "sprites/smokering.spr" );

	PRECACHE_SOUND( "manta/carpetbomb1.wav" );
	PRECACHE_SOUND( "manta/carpetbomb2.wav" );
	PRECACHE_SOUND( "manta/carpetbomb3.wav" );
}

void CClusterGrenade::ClusterDetonate( void )
{
	TraceResult tr;
	Vector		vecSpot;// trace starts here!

	vecSpot = pev->origin + Vector ( 0 , 0 , 8 );
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -40 ),  ignore_monsters, ENT(pev), & tr);

	ClusterExplode( &tr, DMG_BLAST );
}

void CClusterGrenade::TumbleClusterThink( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	if ( !m_fTrailOn )
	{
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMFOLLOW );
			WRITE_SHORT( entindex() );	// entity
			WRITE_SHORT( m_iSmoke );	// model
			WRITE_BYTE( 20 ); // life
			WRITE_BYTE( 5 );  // width
			WRITE_BYTE( 224 );   // r, g, b
			WRITE_BYTE( 224 );
			WRITE_BYTE( 255 );
			WRITE_BYTE( 255 );	// brightness
		MESSAGE_END();

		m_fTrailOn = TRUE;
	}

	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime - 1 < gpGlobals->time)
	{
		CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 400, 0.1 );
	}

	if (pev->dmgtime <= gpGlobals->time)
	{
		SetThink( &CClusterGrenade::ClusterDetonate );
	}
	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}
}

void CClusterGrenade::ClusterExplode( TraceResult *pTrace, int bitsDamageType )
{
	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible

	pev->takedamage = DAMAGE_NO;

	PLAYBACK_EVENT_FULL( 0, edict(), m_usClusterExplosion, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, m_iExplosion, 0 );

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 8 );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 8 + 200.0 );	// reach damage radius over .2 seconds
		WRITE_SHORT( m_iClusterShock );
		WRITE_BYTE( 0 );	// startframe
		WRITE_BYTE( 0 );	// framerate
		WRITE_BYTE( 6 );	// life
		WRITE_BYTE( 16 );	// width
		WRITE_BYTE( 0 );	// noise
		WRITE_BYTE( 255 );	// r, g, b
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );	// brightness
		WRITE_BYTE( 0 );	// speed
	MESSAGE_END();

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	RadiusDamage ( pev, pevOwner, pev->dmg, CLASS_NONE, bitsDamageType );

	if ( UTIL_PercentChance( 50 ) )
		UTIL_DecalTrace( pTrace, DECAL_SCORCH1 );
	else
		UTIL_DecalTrace( pTrace, DECAL_SCORCH2 );

	pev->effects |= EF_NODRAW;
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time;
	SetThink( &CClusterGrenade::ClusterSmokeThink );
}

void CClusterGrenade::ClusterSmokeThink( void )
{
	if ( !m_pSmokeRing )
	{
		m_pSmokeRing = CSprite::SpriteCreate( "sprites/smokering.spr", pev->origin, FALSE );
		m_pSmokeRing->SetScale( 0.2 );
		m_pSmokeRing->SetTransparency( kRenderTransAlpha, 255, 255, 255, 255, kRenderFxNoDissipation );
		m_pSmokeRing->pev->frame = 0;
		UTIL_SetOrigin( m_pSmokeRing->pev, pev->origin );
	}
	else
	{
		m_pSmokeRing->pev->scale += 0.16;
	}

	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_pSmokeRing->pev->scale > 1.25 )
	{
		UTIL_Remove( m_pSmokeRing );
		m_pSmokeRing = NULL;
		UTIL_Remove( this );
	}
}


//=========================================================
// the launcher
//=========================================================
LINK_ENTITY_TO_CLASS( weapon_mule, CDml );
LINK_ENTITY_TO_CLASS( weapon_dml, CDml );

#ifndef CLIENT_DLL
TYPEDESCRIPTION	CDml::m_SaveData[] =
{
	DEFINE_FIELD( CDml, m_iSide, FIELD_INTEGER ),
	DEFINE_FIELD( CDml, m_iFirstSide, FIELD_INTEGER ),
	DEFINE_FIELD( CDml, m_iStyle, FIELD_INTEGER ),
	DEFINE_FIELD( CDml, m_iSpiralScalar_In, FIELD_FLOAT ),
	DEFINE_FIELD( CDml, m_iSpiralScalar_Up, FIELD_FLOAT ),
	DEFINE_FIELD( CDml, m_fSpotActive, FIELD_INTEGER ),
	DEFINE_FIELD( CDml, m_Config.flightpath, FIELD_INTEGER ),
	DEFINE_FIELD( CDml, m_Config.launch, FIELD_INTEGER ),
	DEFINE_FIELD( CDml, m_Config.detonate, FIELD_INTEGER ),
	DEFINE_FIELD( CDml, m_Config.payload, FIELD_INTEGER ),
	DEFINE_FIELD( CDml, m_flNextConfigTime, FIELD_TIME ),
	DEFINE_FIELD( CDml, m_flLastAttack, FIELD_TIME ),
	DEFINE_FIELD( CDml, m_flNextSkinChange, FIELD_TIME ),
	DEFINE_FIELD( CDml, m_iHomingState, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CDml, CBasePlayerWeapon );
#endif


void CDml::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_dml");
	Precache( );
	m_iId = WEAPON_DML;

	SET_MODEL(ENT(pev), "models/w_dml.mdl");
	m_iDefaultAmmo = gSkillData.dmlDefaultAmmo;

	FallInit();// get ready to fall down.

	m_iSpiralScalar_In = 1.6;
	m_iSide = -1;
	m_iFirstSide = -1;
	m_iSpiralScalar_Up = 75;
	m_Config.launch = 1;
	m_Config.detonate = 1;
	m_Config.payload = 1;
	m_Config.flightpath = 1;
	m_fSpotActive = 1;
	m_fCrosshairOn = 0;
	m_iHomingState = 1;
}


void CDml::Precache( void )
{
	PRECACHE_MODEL("models/w_dml.mdl");
	PRECACHE_MODEL("models/v_dml.mdl");
	PRECACHE_MODEL("models/p_crossbow.mdl");
	PRECACHE_MODEL("sprites/dmllock.spr");

	PRECACHE_SOUND("weapons/dml_fire.wav");
	PRECACHE_SOUND("weapons/dml_reload.wav");
	PRECACHE_SOUND("weapons/dml_dualreload.wav");
	PRECACHE_SOUND("weapons/dml_customize.wav");
	PRECACHE_SOUND("weapons/dryfire.wav");
	PRECACHE_SOUND("weapons/dml_lock.wav");

	UTIL_PrecacheOther( "dmlRocket" );
}


#ifndef CLIENT_DLL
void CDml::UpdateOwner( void )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_LAUNCH );
		WRITE_BYTE( m_Config.launch );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_FLIGHTPATH );
		WRITE_BYTE( m_Config.flightpath );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_DETONATE );
		WRITE_BYTE( m_Config.detonate );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_PAYLOAD );
		WRITE_BYTE( m_Config.payload );
	MESSAGE_END();
}
#endif


int CDml::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();

		pPlayer->GiveNamedItem( "cust_1DMLLaunch" );
		pPlayer->GiveNamedItem( "cust_2DMLFlightpath" );
		pPlayer->GiveNamedItem( "cust_3DMLDetonate" );
		pPlayer->GiveNamedItem( "cust_4DMLPayload" );

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_LAUNCH );
			WRITE_BYTE( m_Config.launch );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_FLIGHTPATH );
			WRITE_BYTE( m_Config.flightpath );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_DETONATE );
			WRITE_BYTE( m_Config.detonate );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_PAYLOAD );
			WRITE_BYTE( m_Config.payload );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}


int CDml::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "missiles";
	p->iMaxAmmo1 = 6;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 2;
	p->iSlot = 5;
	p->iPosition = 0;
	p->iId = WEAPON_DML;
	p->iFlags = 0;
	p->iWeight = -1;
	return 1;
}


BOOL CDml::Deploy( )
{
	if ( m_Config.flightpath == 1 )
		m_fSpotActive = TRUE;

	return DefaultDeploy( "models/v_dml.mdl", "models/p_crossbow.mdl", DML_DRAW, "bow" );
}


void CDml::Holster( int skiplocal /* = 0 */ )
{
	m_fInReload = FALSE;// cancel any reload in progress.
	m_fSpotActive = FALSE;

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	if (m_pSpot)
	{
		m_pSpot->Killed( NULL, GIB_NEVER );
		m_pSpot = NULL;
	}

	if ( m_fCrosshairOn )
	{
		m_fCrosshairOn = 0;
		MESSAGE_BEGIN( MSG_ONE, gmsgDmlCross, NULL, m_pPlayer->pev );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	CBasePlayerWeapon::Holster( skiplocal );
}


void CDml::Customize( char cOption, int iValue )
{
	switch ( RANDOM_LONG( 0, 3 ) )
	{
	case 0: SetBody( 0 ); break;
	case 1: SetBody( 2 ); break;
	case 2: SetBody( 6 ); break;
	case 3: SetBody( 8 ); break;
	}

	m_pLockTarget = NULL;

	if ( m_fCrosshairOn )
	{
		m_pPlayer->GetAutoaimVector( 0 );
		m_fCrosshairOn = 0;
		MESSAGE_BEGIN( MSG_ONE, gmsgDmlCross, NULL, m_pPlayer->pev );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	switch ( cOption )
	{
	case '1':
		m_Config.launch = iValue;
		break;
	case '2':
		m_Config.flightpath = iValue;
		if ( iValue == 1 )
		{
			m_fSpotActive = TRUE;
		}
		else
		{
			m_fSpotActive = FALSE;
			if (m_pSpot)
			{
				m_pSpot->Killed( NULL, GIB_NEVER );
				m_pSpot = NULL;
			}
		}
		break;
	case '3':
		m_Config.detonate = iValue;
		break;
	case '4':
		m_Config.payload = iValue;
		break;
	}

	if ( m_flNextConfigTime < gpGlobals->time )
	{
		SendWeaponAnim( DML_CUSTOMIZE );
		m_flNextConfigTime = gpGlobals->time + 4.0;
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/dml_customize.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));
	}
}


void CDml::PrimaryAttack()
{
	m_pLockTarget = NULL;

	switch ( RANDOM_LONG( 0, 3 ) )
	{
	case 0: SetBody( 0 ); break;
	case 1: SetBody( 2 ); break;
	case 2: SetBody( 6 ); break;
	case 3: SetBody( 8 ); break;
	}

	if ( !m_iClip )
	{
		if ( m_Config.launch == 2 && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] && gpGlobals->time - m_flLastAttack > 2.0 )
		{
			Reload( );
			return;
		}

		PlayEmptySound( );
		return;
	}

	m_flLastAttack = gpGlobals->time;
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;

	if ( m_Config.launch == 2 )
	{
		// locked launch
		if ( gpGlobals->time - m_flLastAttack > 2.0 && m_iClip < 2 )
		{
			Reload( );
			return;
		}

		CBaseEntity *pTarget;
		if ( m_Config.flightpath == 2 )
		{
			pTarget = FindTarget( true );
			if ( pTarget )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgDmlCross, NULL, m_pPlayer->pev );
					WRITE_SHORT( 2 );
				MESSAGE_END();
				m_iHomingState = 2;
			}
		}
		else
		{
			pTarget = FindTarget( false );
		}

		m_flNextPrimaryAttack = gpGlobals->time + 0.1;

		if ( !pTarget )
			return;

		if ( m_Config.flightpath == 3 )
		{
			FireSpiral( );
			m_flNextPrimaryAttack = gpGlobals->time + 0.75;
		}
		else
		{
			FireSingle( );
		}
	}
	else if ( m_Config.flightpath == 3 )
	{
		FireSpiral( );
	}
	else if ( m_Config.launch == 1 )
	{
		FireSingle( );
	}
}


//=========================================================
// FireSpiral - two rockets that corkscrew around each other
//=========================================================
void CDml::FireSpiral( void )
{
	if ( m_iClip < 2 )
	{
		if ( m_iClip )
		{
			// only one left: it flies straight
			SendWeaponAnim( DML_FIRE );
			m_iClip--;

			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/dml_fire.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));

			CreateRocket( );
		}
		else
		{
			PlayEmptySound( );
		}
		return;
	}

	m_iClip -= 2;

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/dml_fire.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));

	if ( m_iClip )
		SendWeaponAnim( DML_FIRE );
	else if ( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		SendWeaponAnim( DML_FIRE );	// sic: the same animation

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	CDmlRocket *pRocket1 = CreateRocket( );
	OffsetRocket( pRocket1, -1 );
	CDmlRocket *pRocket2 = CreateRocket( );
	OffsetRocket( pRocket2, 1 );

	pRocket1->m_BrotherRocket = pRocket2;
	pRocket2->m_BrotherRocket = pRocket1;
	pRocket2->m_iIsBrother = 1;
	pRocket1->m_iIsBrother = 2;
	pRocket2->m_iSpiralScalar_In = m_iSpiralScalar_In;
	pRocket2->m_iSpiralScalar_Up = m_iSpiralScalar_Up;
	pRocket1->m_iSpiralScalar_In = m_iSpiralScalar_In;
	pRocket1->m_iSpiralScalar_Up = m_iSpiralScalar_Up;

	m_pPlayer->pev->punchangle.x -= 2;
}


void CDml::InventoryFrame( void )
{
	if ( this != m_pPlayer->m_pActiveItem )
	{
		if (m_pSpot)
		{
			m_pSpot->Killed( NULL, GIB_NEVER );
			m_pSpot = NULL;
		}
	}
	else if ( m_Config.flightpath == 1 )
		UpdateSpot( );
}


//=========================================================
// FireSingle - one rocket, from alternate barrels
//=========================================================
void CDml::FireSingle( void )
{
	if ( m_iClip )
	{
		m_iClip--;
		SendWeaponAnim( DML_FIRE );

		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/dml_fire.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));

		if ( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/dryfire.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));

		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		CDmlRocket *pRocket = CreateRocket( );
		OffsetRocket( pRocket, m_iSide );
		m_iSide = ( m_iSide == -1 ) ? 1 : -1;

		if ( m_Config.flightpath == 2 )
			pRocket->m_hEnemy = FindTarget( true );

		m_pPlayer->pev->punchangle.x -= 2;
	}
}


//=========================================================
// OffsetRocket - move a rocket out to its barrel
//=========================================================
void CDml::OffsetRocket( CBaseEntity *pRocket, int iSide )
{
	Vector vecRight;

	UTIL_MakeVectorsPrivate( pRocket->pev->angles, NULL, vecRight, NULL );
	vecRight.x *= iSide * 5;
	vecRight.y *= iSide * 5;
	vecRight.z *= iSide * 5;
	pRocket->pev->origin = pRocket->pev->origin + vecRight;
}


CDmlRocket *CDml::CreateRocket( void )
{
	Vector vecAngles = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors( vecAngles );
	vecAngles.x = -vecAngles.x;

	Vector vecSrc = m_pPlayer->GetGunPosition( ) - gpGlobals->v_up * 2;
	Vector vecForward = gpGlobals->v_forward;

	CDmlRocket *pRocket = ( m_Config.flightpath == 3 ) ?
		CDmlRocket::CreateSpiralRocket( m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12, m_pPlayer->pev->v_angle, m_pPlayer->edict(), m_Config.flightpath, m_Config.payload, m_Config.detonate ) :
		CDmlRocket::CreateDmlRocket( m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12, m_pPlayer->pev->v_angle, m_pPlayer->edict(), m_Config.flightpath, m_Config.payload, m_Config.detonate );

	pRocket->m_iFlightpath = m_Config.flightpath;
	pRocket->m_iPayload = m_Config.payload;
	pRocket->m_iDetonate = m_Config.detonate;
	pRocket->pev->origin = vecSrc;
	pRocket->pev->angles = vecAngles;
	pRocket->pev->owner = m_pPlayer->edict();
	pRocket->m_angleOwner = m_pPlayer->pev->angles;

	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		pRocket->pev->velocity = vecForward * 600;
		pRocket->pev->speed = 600;
	}
	else
	{
		pRocket->pev->velocity = vecForward * 900;
		pRocket->pev->speed = 900;
	}
	pRocket->pev->avelocity.z = 10;

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = gpGlobals->time + 0.75;
	if ( m_iClip )
		m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	else
		m_flTimeWeaponIdle = 0.75;	// not a time (sic)

	if ( m_Config.detonate == 3 )
		pRocket->pev->dmgtime = gpGlobals->time + 2;

	return pRocket;
}


void CDml::Reload( void )
{
	if ( gpGlobals->time - m_flLastAttack < 2.0 && m_Config.flightpath == 1 )
		UpdateSpot( );

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] - 2 + m_iClip < 1 )
		pev->body = 1;

	if ( m_iClip == 1 || m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 2 )
	{
		// one rocket
		BOOL fReloaded;
		if ( m_iSide == -1 )
			fReloaded = DefaultReload( 2, DML_RELOADRIGHT, 1.4 );
		else
			fReloaded = DefaultReload( 2, DML_RELOADLEFT, 1.4 );
		if ( !fReloaded )
			return;
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/dml_reload.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));
	}
	else
	{
		if ( !DefaultReload( 2, DML_RELOADBOTH, 1.4 ) )
			return;

		if ( m_iFirstSide == 1 )
		{
			m_iSide = -1;
			m_iFirstSide = -1;
		}
		else
		{
			m_iSide = 1;
			m_iFirstSide = 1;
		}
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/dml_reload.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));
	}
}


void CDml::WeaponIdle( void )
{
	if ( m_Config.flightpath == 2 )
	{
		// homing: show the lock-on crosshair
		if ( !m_fCrosshairOn )
		{
			m_fCrosshairOn = 1;
			MESSAGE_BEGIN( MSG_ONE, gmsgDmlCross, NULL, m_pPlayer->pev );
				WRITE_SHORT( 1 );
			MESSAGE_END();
			m_fLocked = 0;
			m_iHomingState = 1;
		}

		if ( FindTarget( true ) )
		{
			if ( !m_fLocked )
			{
				m_fLocked = 1;
				m_iHomingState = 2;
				MESSAGE_BEGIN( MSG_ONE, gmsgDmlCross, NULL, m_pPlayer->pev );
					WRITE_SHORT( 2 );
				MESSAGE_END();
				SetBody( 4 );
				EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/dml_lock.wav", 1.0, ATTN_NORM, 0, 100 );
				SendWeaponAnim( DML_IDLE );
			}
		}
		else if ( m_fLocked )
		{
			m_fLocked = 0;
			m_iHomingState = 1;
			MESSAGE_BEGIN( MSG_ONE, gmsgDmlCross, NULL, m_pPlayer->pev );
				WRITE_SHORT( 1 );
			MESSAGE_END();
			SetBody( 0 );
			SendWeaponAnim( DML_IDLE );
		}

		CBaseEntity *pTarget = FindTarget( true );
		if ( !pTarget && m_pLockTarget )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgDmlCross, NULL, m_pPlayer->pev );
				WRITE_SHORT( 1 );
			MESSAGE_END();
			m_iHomingState = 1;
			SetBody( 0 );
			SendWeaponAnim( DML_IDLE );
		}
		m_pLockTarget = pTarget;
	}
	else if ( m_fCrosshairOn )
	{
		m_fCrosshairOn = 0;
		MESSAGE_BEGIN( MSG_ONE, gmsgDmlCross, NULL, m_pPlayer->pev );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	if ( m_flNextSkinChange < gpGlobals->time && m_Config.flightpath != 2 )
	{
		switch ( RANDOM_LONG( 0, 3 ) )
		{
		case 0: SetBody( 0 ); break;
		case 1: SetBody( 2 ); break;
		case 2: SetBody( 6 ); break;
		case 3: SetBody( 8 ); break;
		}
		m_flNextSkinChange = gpGlobals->time + 1.0;
	}

	if ( gpGlobals->time - m_flLastAttack > 2.0 && m_iClip < 2 && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		Reload( );
		return;
	}

	if ( m_Config.flightpath == 1 )
		UpdateSpot( );

	ResetEmptySound( );

	if ( m_flNextConfigTime <= gpGlobals->time && m_flTimeWeaponIdle < gpGlobals->time )
	{
		if ( RANDOM_FLOAT( 0, 1 ) < 0.5 )
		{
			SendWeaponAnim( DML_IDLEFIDGET );
			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 5, 12 );
		}
		else
		{
			SendWeaponAnim( DML_IDLE );
			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 3, 10 );
		}
	}
}


void CDml::UpdateSpot( void )
{
	if (m_fSpotActive)
	{
		if (!m_pSpot)
		{
			m_pSpot = CLaserSpot::CreateSpot();
		}

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc = m_pPlayer->GetGunPosition( );;
		Vector vecAiming = gpGlobals->v_forward;

		TraceResult tr;
		UTIL_TraceLine ( vecSrc, vecSrc + vecAiming * 8192, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr );

		UTIL_SetOrigin( m_pSpot->pev, tr.vecEndPos );
	}
}


//=========================================================
// FindTarget - what the player has in his sights, if it's
// worth a rocket
//=========================================================
CBaseEntity *CDml::FindTarget( bool fAutoaim )
{
	float flMaxDeviation = 0.05;	// past this, autoaim is ignored

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecForward = gpGlobals->v_forward;
	Vector vecDir = vecForward;	// sic: both branches below set it; the dead
								// copy gives retail's operand order in the x
								// sum of v_angle + punchangle

	if ( fAutoaim )
		vecDir = GetAutoaimVector( AUTOAIM_10DEGREES );
	else
		vecDir = vecForward;

	if ( ( vecForward - vecDir ).Length() > flMaxDeviation )
		vecDir = vecForward;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc = m_pPlayer->GetGunPosition( );

	TraceResult tr;
	UTIL_TraceLine ( vecSrc, vecSrc + vecDir * 8192, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr );

	if ( !tr.fAllSolid )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		if ( pEntity && pEntity->pev->takedamage && pEntity->IsAlive() )
		{
			if ( g_pGameRules->IsMultiplayer() )
				return pEntity;

			if ( m_pPlayer->IRelationship( pEntity ) > R_NO )
				return pEntity;
		}
	}

	return NULL;
}


//=========================================================
// GetAutoaimVector - the player's autoaim, always on and
// always slow
//=========================================================
Vector CDml::GetAutoaimVector( float flDelta )
{
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	float flDist = 8192;

	BOOL m_fOldTargeting = m_pPlayer->m_fOnTarget;
	Vector angles = AutoaimDeflection( vecSrc, flDist, flDelta );

	// update ontarget if changed
	if ( m_fOldTargeting != m_pPlayer->m_fOnTarget )
	{
		m_pPlayer->m_pActiveItem->UpdateItemInfo( );
	}

	if (angles.x > 180)
		angles.x -= 360;
	if (angles.x < -180)
		angles.x += 360;
	if (angles.y > 180)
		angles.y -= 360;
	if (angles.y < -180)
		angles.y += 360;

	if (angles.x > 25)
		angles.x = 25;
	if (angles.x < -25)
		angles.x = -25;
	if (angles.y > 12)
		angles.y = 12;
	if (angles.y < -12)
		angles.y = -12;

	m_pPlayer->m_vecAutoAim = m_pPlayer->m_vecAutoAim * 0.67 + angles * 0.33;

	if ( m_pPlayer->m_vecAutoAim.x != m_pPlayer->m_lastx ||
		 m_pPlayer->m_vecAutoAim.y != m_pPlayer->m_lasty )
	{
		SET_CROSSHAIRANGLE( m_pPlayer->edict(), -m_pPlayer->m_vecAutoAim.x, m_pPlayer->m_vecAutoAim.y );

		m_pPlayer->m_lastx = m_pPlayer->m_vecAutoAim.x;
		m_pPlayer->m_lasty = m_pPlayer->m_vecAutoAim.y;
	}

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle + m_pPlayer->m_vecAutoAim );
	return gpGlobals->v_forward;
}


//=========================================================
// AutoaimDeflection - the player's, telling the crosshair
// when something is in the sights
//=========================================================
Vector CDml::AutoaimDeflection( Vector &vecSrc, float flDist, float flDelta )
{
	edict_t		*pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
	CBaseEntity	*pEntity;
	float		bestdot;
	Vector		bestdir;
	edict_t		*bestent;
	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle + m_pPlayer->m_vecAutoAim );

	// try all possible entities
	bestdir = gpGlobals->v_forward;
	bestdot = flDelta; // +- 10 degrees
	bestent = NULL;

	m_pPlayer->m_fOnTarget = FALSE;

	UTIL_TraceLine( vecSrc, vecSrc + bestdir * flDist, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );


	if ( tr.pHit && tr.pHit->v.takedamage != DAMAGE_NO)
	{
		// don't look through water
		if (!((m_pPlayer->pev->waterlevel != 3 && tr.pHit->v.waterlevel == 3)
			|| (m_pPlayer->pev->waterlevel == 3 && tr.pHit->v.waterlevel == 0)))
		{
			if (tr.pHit->v.takedamage == DAMAGE_AIM)
				m_pPlayer->m_fOnTarget = TRUE;

			if ( m_iHomingState != 3 && !m_fLocked )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgDmlCross, NULL, m_pPlayer->pev );
					WRITE_SHORT( 3 );
				MESSAGE_END();
				m_iHomingState = 3;
			}

			return m_pPlayer->m_vecAutoAim;
		}
	}

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		Vector center;
		Vector dir;
		float dot;

		if ( pEdict->free )	// Not in use
			continue;

		if (pEdict->v.takedamage != DAMAGE_AIM)
			continue;
		if (pEdict == m_pPlayer->edict())
			continue;

		pEntity = Instance( pEdict );
		if (pEntity == NULL)
			continue;

		if (!pEntity->IsAlive())
			continue;

		// don't look through water
		if ((m_pPlayer->pev->waterlevel != 3 && pEntity->pev->waterlevel == 3)
			|| (m_pPlayer->pev->waterlevel == 3 && pEntity->pev->waterlevel == 0))
			continue;

		center = pEntity->BodyTarget( vecSrc );

		dir = (center - vecSrc).Normalize( );

		// make sure it's in front of the player
		if (DotProduct (dir, gpGlobals->v_forward ) < 0)
			continue;

		dot = fabs( DotProduct (dir, gpGlobals->v_right ) )
			+ fabs( DotProduct (dir, gpGlobals->v_up ) ) * 0.5;

		// tweek for distance
		dot *= 1.0 + 0.2 * ((center - vecSrc).Length() / flDist);

		if (dot > bestdot)
			continue;	// to far to turn

		UTIL_TraceLine( vecSrc, center, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );
		if (tr.flFraction != 1.0 && tr.pHit != pEdict)
		{
			continue;
		}

		// don't shoot at friends
		if (m_pPlayer->IRelationship( pEntity ) < 0)
		{
			if ( !pEntity->IsPlayer() && !g_pGameRules->IsDeathmatch())
				continue;
		}

		// can shoot at this one
		bestdot = dot;
		bestent = pEdict;
		bestdir = dir;
	}

	if (bestent)
	{
		bestdir = UTIL_VecToAngles (bestdir);
		bestdir.x = -bestdir.x;
		bestdir = bestdir - m_pPlayer->pev->v_angle - m_pPlayer->pev->punchangle;

		if (bestent->v.takedamage == DAMAGE_AIM)
			m_pPlayer->m_fOnTarget = TRUE;

		if ( m_iHomingState != 3 && !m_fLocked )
		{
			m_iHomingState = 3;
			MESSAGE_BEGIN( MSG_ONE, gmsgDmlCross, NULL, m_pPlayer->pev );
				WRITE_SHORT( 3 );
			MESSAGE_END();
		}

		return bestdir;
	}

	if ( !m_fLocked && m_iHomingState )
	{
		m_iHomingState = 0;
		MESSAGE_BEGIN( MSG_ONE, gmsgDmlCross, NULL, m_pPlayer->pev );
			WRITE_SHORT( 1 );
		MESSAGE_END();
	}

	return Vector( 0, 0, 0 );
}


//=========================================================
// SetBody - the launcher's body shows the rockets; the next
// body up is the empty one
//=========================================================
void CDml::SetBody( int iBody )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		pev->body = iBody;
	else
		pev->body = iBody + 1;
}


class CDmlSingleAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL(ENT(pev), "models/singlerocket.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/singlerocket.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		// the rockets bring the grenade launcher along
		if ( pOther->IsPlayer() && !((CBasePlayer *)pOther)->HasNamedPlayerItem( "weapon_dmlGrenade" ) )
			((CBasePlayer *)pOther)->GiveNamedItem( "weapon_dmlGrenade" );

		if (pOther->GiveAmmo( 1, "missiles", gSkillData.dmlMaxAmmo ) != -1)
		{
			if ( PlayPickupSound() )
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_dmlsingle, CDmlSingleAmmo );


class CDmlAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL(ENT(pev), "models/dmlammo.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/dmlammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if ( pOther->IsPlayer() && !((CBasePlayer *)pOther)->HasNamedPlayerItem( "weapon_dmlGrenade" ) )
			((CBasePlayer *)pOther)->GiveNamedItem( "weapon_dmlGrenade" );

		if (pOther->GiveAmmo( gSkillData.dmlClipAmmo, "missiles", 6 ) != -1)
		{
			if ( PlayPickupSound() )
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_dmlclip, CDmlAmmo );


//=========================================================
// the DML's four customization items: one class
//=========================================================
class CCustDml : public CBaseCustomization
{
public:
	void Spawn( void );
	int GetItemInfo(ItemInfo *p);
};
LINK_ENTITY_TO_CLASS( cust_1DMLLaunch, CCustDml );
LINK_ENTITY_TO_CLASS( cust_2DMLFlightpath, CCustDml );
LINK_ENTITY_TO_CLASS( cust_3DMLDetonate, CCustDml );
LINK_ENTITY_TO_CLASS( cust_4DMLPayload, CCustDml );

void CCustDml::Spawn( )
{
	Precache( );

	if ( FClassnameIs( pev, "cust_1DMLLaunch" ) )
		m_iId = WEAPON_CUST_LAUNCH;
	else if ( FClassnameIs( pev, "cust_2DMLFlightpath" ) )
		m_iId = WEAPON_CUST_FLIGHTPATH;
	else if ( FClassnameIs( pev, "cust_3DMLDetonate" ) )
		m_iId = WEAPON_CUST_DETONATE;
	else if ( FClassnameIs( pev, "cust_4DMLPayload" ) )
		m_iId = WEAPON_CUST_PAYLOAD;

	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
}

int CCustDml::GetItemInfo(ItemInfo *p)
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

	if ( FClassnameIs( pev, "cust_1DMLLaunch" ) )
	{
		p->iId = WEAPON_CUST_LAUNCH;
		p->iPosition = 15;
		p->iCustomSettings = 2;
		p->iCustomDefault = 1;
		p->pszCustomizes = "weapon_dml";
	}
	else if ( FClassnameIs( pev, "cust_2DMLFlightpath" ) )
	{
		p->iId = WEAPON_CUST_FLIGHTPATH;
		p->iPosition = 16;
		p->iIsCustomization = 1;
		p->iCustomSettings = 3;
		p->iCustomDefault = 1;
		p->pszCustomizes = "weapon_dml";
	}
	else if ( FClassnameIs( pev, "cust_3DMLDetonate" ) )
	{
		p->iId = WEAPON_CUST_DETONATE;
		p->iPosition = 17;
		p->iCustomSettings = 4;
		p->iIsCustomization = 1;
		p->iCustomDefault = 1;
		p->pszCustomizes = "weapon_dml";
	}
	else if ( FClassnameIs( pev, "cust_4DMLPayload" ) )
	{
		p->iId = WEAPON_CUST_PAYLOAD;
		p->iPosition = 18;
		p->iCustomSettings = 2;
		p->iIsCustomization = 1;
		p->iCustomDefault = 1;
		p->pszCustomizes = "weapon_dml";
	}

	p->iWeight = CUSTOMIZATION_WEIGHT;
	return 1;
}
