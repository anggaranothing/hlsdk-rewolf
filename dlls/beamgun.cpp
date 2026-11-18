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
// the beam gun (polaris blade): a customizable lightning
// gun, and the ball lightning, tracers and lightning bugs
// it throws off
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "effects.h"
#include "customentity.h"
#include "gamerules.h"
#include "soundent.h"
#include "beamgun.h"

extern int gmsgWeapPickup;
extern int gmsgSliderX;
extern int gmsgTempX;

#define	EGON_PRIMARY_VOLUME		450
#define EGON_BEAM_SPRITE		"sprites/xbeam1.spr"
#define EGON_FLARE_SPRITE		"sprites/XSpark1.spr"
#define EGON_SOUND_OFF			"weapons/egon_off1.wav"
#define EGON_SOUND_RUN			"weapons/egon_run3.wav"
#define EGON_SOUND_STARTUP		"weapons/egon_windup2.wav"

int g_iBeamSparks;	// sparks where the beam hits the world

enum beamgun_e {
	BEAMGUN_ARM = 0,
	BEAMGUN_IDLESUBDUEL,
	BEAMGUN_IDLEBLADE,
	BEAMGUN_IDLEINSPECT,
	BEAMGUN_FIRE,
	BEAMGUN_SHOOTSINGLE,
	BEAMGUN_CONFIG,
	BEAMGUN_CHARGE
};


LINK_ENTITY_TO_CLASS( weapon_polarisblade, CBeamGun );
LINK_ENTITY_TO_CLASS( weapon_beamgun, CBeamGun );


#ifndef CLIENT_DLL
TYPEDESCRIPTION	CBeamGun::m_SaveData[] =
{
	DEFINE_FIELD( CBeamGun, m_pBeam, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBeamGun, m_pLightningBeam, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBeamGun, m_pNoise, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBeamGun, m_pSprite, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBeamGun, m_shootTime, FIELD_TIME ),
	DEFINE_FIELD( CBeamGun, m_fireState, FIELD_INTEGER ),
	DEFINE_FIELD( CBeamGun, m_shakeTime, FIELD_TIME ),
	DEFINE_FIELD( CBeamGun, m_flAmmoUseTime, FIELD_TIME ),
	DEFINE_FIELD( CBeamGun, m_iTemp, FIELD_INTEGER ),
	DEFINE_FIELD( CBeamGun, m_flNextTempChangeTime, FIELD_TIME ),
	DEFINE_FIELD( CBeamGun, m_fOverheating, FIELD_INTEGER ),
	DEFINE_FIELD( CBeamGun, m_flTimeStartOverheating, FIELD_TIME ),
	DEFINE_FIELD( CBeamGun, m_flNextTempDecreaseTime, FIELD_TIME ),
	DEFINE_FIELD( CBeamGun, m_flNextIdleAnimTime, FIELD_TIME ),
	DEFINE_FIELD( CBeamGun, m_flTimeAttackAnim, FIELD_TIME ),
	DEFINE_FIELD( CBeamGun, m_flShootLightningBugTime, FIELD_TIME ),
	DEFINE_FIELD( CBeamGun, m_flLastCustomTime, FIELD_TIME ),
	DEFINE_FIELD( CBeamGun, m_flChainLightningTime, FIELD_TIME ),
	DEFINE_FIELD( CBeamGun, Configuration.current, FIELD_FLOAT ),
	DEFINE_FIELD( CBeamGun, Configuration.voltage, FIELD_FLOAT ),
	DEFINE_FIELD( CBeamGun, Configuration.frequency, FIELD_FLOAT ),	// an int (sic)
	DEFINE_FIELD( CBeamGun, m_flDrawShotgunLightning, FIELD_FLOAT ),
	DEFINE_FIELD( CBeamGun, m_iFireType, FIELD_INTEGER ),
	DEFINE_FIELD( CBeamGun, m_flStartAttack, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CBeamGun, CBasePlayerWeapon );
#endif


//=========================================================
// ball lightning: thrown off at random by a high voltage
// beam; it spits lightning bugs as it flies
//=========================================================
class CBallLightning : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void );
	void EXPORT LightningTouch( CBaseEntity *pOther );
	void EXPORT PopThink( void );
	void EXPORT MoveThink( void );

	static CBallLightning *Shoot( Vector vecStart, Vector vecAngles, CBaseEntity *pOwner, int iSpeed );

	float	m_flSpawnTime;
	int		m_iSmoke;
	int		m_iUnused;
	float	m_flNextBugTime;
	unsigned short m_usBallLightning;
};
LINK_ENTITY_TO_CLASS( ball_lightning, CBallLightning );


//=========================================================
// CanArc - the beam only throws off lightning with the
// player on the ground, at high voltage
//=========================================================
BOOL CBeamGun::CanArc( void )
{
	if ( FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) && Configuration.voltage > 2 )
		return TRUE;

	return FALSE;
}

BOOL CBeamGun::HasAmmo( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return FALSE;

	return TRUE;
}

void CBeamGun::UseAmmo( int count )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= count )
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= count;
	else
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = 0;
}


void CBeamGun::Spawn( )
{
	Configuration.current = 2;
	Configuration.voltage = 2;
	Configuration.frequency = 3;
	m_iTemp = 1;

	pev->classname = MAKE_STRING("weapon_beamgun");
	Precache( );
	m_iId = WEAPON_BEAMGUN;
	SET_MODEL(ENT(pev), "models/w_beam.mdl");

	m_iDefaultAmmo = gSkillData.beamgunDefaultAmmo;

	FallInit();// get ready to fall down.
}


void CBeamGun::Precache( void )
{
	m_iBeamLight = PRECACHE_MODEL("sprites/beamlight2.spr");
	PRECACHE_MODEL("models/w_beam.mdl");
	PRECACHE_MODEL("models/v_beam.mdl");
	PRECACHE_MODEL("models/p_egon.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND( EGON_SOUND_OFF );
	PRECACHE_SOUND( EGON_SOUND_RUN );
	PRECACHE_SOUND( EGON_SOUND_STARTUP );
	PRECACHE_SOUND( "weapons/electro4.wav" );
	PRECACHE_SOUND( "weapons/overheat.wav" );

	PRECACHE_MODEL( EGON_BEAM_SPRITE );
	PRECACHE_MODEL( EGON_FLARE_SPRITE );

	UTIL_PrecacheOther( "ball_lightning" );
	UTIL_PrecacheOther( "tracer" );
	UTIL_PrecacheOther( "lightning_bug" );

	m_usMinigunHeat = PRECACHE_EVENT( 1, "events/gw_minigunHeat.sc" );
	m_usLightning = PRECACHE_EVENT( 1, "events/lightning.sc" );

	g_iBeamSparks = PRECACHE_MODEL( "sprites/gorebot.spr" );
}


int CBeamGun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();

		pPlayer->GiveNamedItem( "cust_1BeamFrequency" );
		pPlayer->GiveNamedItem( "cust_2BeamCurrent" );
		pPlayer->GiveNamedItem( "cust_3BeamVoltage" );

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_FREQUENCY );
			WRITE_BYTE( Configuration.frequency );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_CURRENT );
			WRITE_BYTE( Configuration.current );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_VOLTAGE );
			WRITE_BYTE( Configuration.voltage - 1 );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}


BOOL CBeamGun::Deploy( void )
{
	return DefaultDeploy( "models/v_beam.mdl", "models/p_egon.mdl", BEAMGUN_ARM, "egon" );
}


void CBeamGun::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	PLAYBACK_EVENT_FULL( 0, m_pPlayer->edict(), m_usMinigunHeat, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 1 );

	if ( m_fireState != FIRE_OFF )
		EndAttack();

	m_fireState = FIRE_OFF;
	m_flDrawShotgunLightning = 0;

	CBasePlayerWeapon::Holster( skiplocal );
}

int CBeamGun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "battery";
	p->iMaxAmmo1 = gSkillData.beamgunMaxAmmo;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_BEAMGUN;
	p->iFlags = 0;
	p->iWeight = 10;

	return 1;
}


#define EGON_PULSE_INTERVAL			0.1
#define EGON_DISCHARGE_INTERVAL		0.1

float CBeamGun::GetPulseInterval( void )
{
	return EGON_PULSE_INTERVAL;
}

float CBeamGun::GetDischargeInterval( void )
{
	return EGON_DISCHARGE_INTERVAL;
}


void CBeamGun::SecondaryAttack( void )
{
	WeaponIdle( );
}


void CBeamGun::PrimaryAttack( void )
{
	if ( gpGlobals->time > m_flDrawShotgunLightning && m_flDrawShotgunLightning != 0 )
	{
		m_fireState = FIRE_OFF;
		m_flDrawShotgunLightning = 0;
		m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0;
		return;
	}

	if ( m_flTimeStartOverheating != 0 && gpGlobals->time - m_flTimeStartOverheating > 0.5 )
	{
		PLAYBACK_EVENT_FULL( 0, m_pPlayer->edict(), m_usMinigunHeat, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 1 );
		SendWeaponAnim( BEAMGUN_IDLESUBDUEL );
		m_flTimeStartOverheating = 0;
	}

	if ( m_fOverheating )
		return;

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_pBeam )
		{
			EndAttack();
		}
		else
		{
			PlayEmptySound( );
		}
		return;
	}

	if ( m_flStartAttack > gpGlobals->time )
		return;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecSrc	 = m_pPlayer->GetGunPosition( );

	switch( m_fireState )
	{
		case FIRE_OFF:
		{
			if ( !HasAmmo() )
			{
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 0.25;
				PlayEmptySound( );
				return;
			}

			m_flAmmoUseTime = gpGlobals->time;// start using ammo ASAP.

			SendWeaponAnim( BEAMGUN_CHARGE );
			m_shakeTime = 0;

			m_pPlayer->m_iWeaponVolume = EGON_PRIMARY_VOLUME;
			m_flTimeWeaponIdle = gpGlobals->time + 0.1;
			m_shootTime = gpGlobals->time + 1.34;

			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, EGON_SOUND_STARTUP, 0.98, ATTN_NORM, 0, 125 );

			pev->dmgtime = gpGlobals->time + GetPulseInterval();
			m_fireState = FIRE_CHARGE;
			m_flNextBeamRing = gpGlobals->time + 0.5;
			m_flStartAttack = gpGlobals->time + 0.73;
		}
		break;

		case FIRE_CHARGE:
		{
			Discharge( vecSrc, vecAiming );
			m_pPlayer->m_iWeaponVolume = EGON_PRIMARY_VOLUME;

			if ( m_shootTime != 0 && gpGlobals->time > m_shootTime )
			{
				if ( gpGlobals->time > m_flDrawShotgunLightning )
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_STATIC, EGON_SOUND_RUN, 0.98, ATTN_NORM, 0, 125 );

				m_shootTime = 0;
			}
			if ( !HasAmmo() )
			{
				EndAttack();
				m_fireState = FIRE_OFF;
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 1.0;
			}

		}
		break;
	}
}

void CBeamGun::Fire( const Vector &vecOrigSrc, const Vector &vecDir )
{
	edict_t		*pentIgnore;
	TraceResult tr;

	if ( m_flTimeAttackAnim < gpGlobals->time )
	{
		SendWeaponAnim( BEAMGUN_FIRE );
		m_flTimeAttackAnim = gpGlobals->time + 0.25;
	}

	pentIgnore = m_pPlayer->edict();
	Vector tmpSrc = vecOrigSrc + gpGlobals->v_up * -8 + gpGlobals->v_right * 3;

	float x, y, z;
	do {
		x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		z = x*x+y*y;
	} while (z > 1);

	Vector vecSpreadDir = vecDir +
					x * Configuration.spread.x * gpGlobals->v_right +
					y * Configuration.spread.y * gpGlobals->v_up;
	Vector vecDest;

	vecDest = vecOrigSrc + vecSpreadDir * Configuration.range;

	UTIL_TraceLine( vecOrigSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr );
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1000, 0.1 );

	if (tr.fAllSolid)
		return;

	if ( FClassnameIs( VARS( tr.pHit ), "worldspawn" ) && tr.flFraction < 1 && !g_pGameRules->IsMultiplayer() )
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
			WRITE_BYTE( TE_SPRITE_SPRAY );
			WRITE_COORD( tr.vecEndPos.x );	// pos
			WRITE_COORD( tr.vecEndPos.y );
			WRITE_COORD( tr.vecEndPos.z );
			WRITE_COORD( 0 );	// dir
			WRITE_COORD( 0 );
			WRITE_COORD( 1.0 );
			WRITE_SHORT( g_iBeamSparks );
			WRITE_BYTE( 1 );	// count
			WRITE_BYTE( 50 );	// speed
			WRITE_BYTE( 200 );	// noise
		MESSAGE_END();
	}

	CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

	if (pEntity == NULL)
		return;

	if ( g_pGameRules->IsMultiplayer() )
	{
		if ( m_pSprite && pEntity->pev->takedamage )
		{
			m_pSprite->pev->effects &= ~EF_NODRAW;
		}
		else if ( m_pSprite )
		{
			m_pSprite->pev->effects |= EF_NODRAW;
		}
	}

	if ( pev->dmgtime < gpGlobals->time )
	{
		ClearMultiDamage();

		if ( pEntity->pev->takedamage && m_iFireType == 1 )
		{
			// short range: one big jolt, then nothing until the shot is over
			if ( m_flDrawShotgunLightning == 0 )
			{
				pEntity->TraceAttack( m_pPlayer->pev, Configuration.shockdamage, vecDir, &tr, DMG_ENERGYBEAM );
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/overheat.wav", 0.98, ATTN_NORM, 0, 100 );
				m_flDrawShotgunLightning = gpGlobals->time + 0.8;
			}
			STOP_SOUND( ENT(m_pPlayer->pev), CHAN_STATIC, EGON_SOUND_RUN );
		}
		else if ( m_iFireType != 1 || m_flDrawShotgunLightning == 0 )
		{
			pEntity->TraceAttack( m_pPlayer->pev, Configuration.damage, vecDir, &tr, DMG_ENERGYBEAM );
			pEntity->TraceAttack( m_pPlayer->pev, Configuration.shockdamage / 2, vecDir, &tr, DMG_SHOCK );
			pev->dmgtime = gpGlobals->time + GetDischargeInterval();
		}

		ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);

		if ( !m_pPlayer->IsAlive() )
			return;

		if ( gpGlobals->time >= m_flAmmoUseTime )
		{
			UseAmmo( 1 );

			if ( m_iFireType == 1 )
			{
				if ( m_flDrawShotgunLightning != 0 )
					m_flAmmoUseTime = gpGlobals->time + 0.1;
				else
					m_flAmmoUseTime = gpGlobals->time + 0.2;
			}
			else
			{
				switch ( (int)Configuration.voltage )
				{
				case 1:
					m_flAmmoUseTime = gpGlobals->time + 0.15;
					break;
				case 2:
					m_flAmmoUseTime = gpGlobals->time + 0.12;
					break;
				default:
					m_flAmmoUseTime = gpGlobals->time + 0.1;
					break;
				}
			}
		}
	}

	if ( m_flChainLightningTime < gpGlobals->time && CanArc() && Configuration.voltage > 2 )
	{
		m_flChainLightningTime = gpGlobals->time + RANDOM_FLOAT( 0.5, 1 );
		if ( g_pGameRules->IsMultiplayer() )
			m_flChainLightningTime = gpGlobals->time + RANDOM_FLOAT( 1.5, 2 );

		ChainLightning( tr.vecEndPos );
	}

	if ( m_iFireType == 1 && gpGlobals->time < m_flDrawShotgunLightning )
	{
		ShotgunLightning( vecOrigSrc, vecDir, tr );
		return;
	}

	UpdateEffect( vecOrigSrc + gpGlobals->v_right * 4, tr.vecEndPos, 0 );
}


void CBeamGun::UpdateEffect( Vector &startPoint, const Vector &endPoint, float timeBlend )
{
	if ( !m_pBeam )
	{
		CreateEffect();
	}

	m_pBeam->RelinkBeam();
	m_pBeam->SetStartPos( endPoint );
	m_pBeam->SetBrightness( 230 );
	m_pBeam->SetColor( Configuration.red, Configuration.green, Configuration.blue );

	startPoint = m_pPlayer->GetGunPosition( ) + gpGlobals->v_right * 2 + gpGlobals->v_up * -2 + gpGlobals->v_forward * 2;

	if ( m_flNextBeamRing <= gpGlobals->time && FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) )
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, endPoint );
			WRITE_BYTE( TE_BEAMENTPOINT );
			WRITE_SHORT( m_pPlayer->entindex() + 0x1000 );
			WRITE_COORD( endPoint.x );
			WRITE_COORD( endPoint.y );
			WRITE_COORD( endPoint.z );
			WRITE_SHORT( m_iBeamLight );
			WRITE_BYTE( 0 );	// framestart
			WRITE_BYTE( 10 );	// framerate
			WRITE_BYTE( 1 );	// life
			WRITE_BYTE( 25 );	// width
			WRITE_BYTE( 20 );	// noise
			WRITE_BYTE( 255 );	// r, g, b
			WRITE_BYTE( 255 );
			WRITE_BYTE( 128 );
			WRITE_BYTE( 200 );	// brightness
			WRITE_BYTE( 0 );	// speed
		MESSAGE_END();

		m_flNextBeamRing = gpGlobals->time + 0.085;
	}

	if ( m_flShootLightningBugTime < gpGlobals->time && !g_pGameRules->IsMultiplayer() && CanArc() )
	{
		m_flShootLightningBugTime = gpGlobals->time + 1;
		for ( int i = 0; i < 3; i++ )
			ShootTracer();
	}
}


void CBeamGun::CreateEffect( void )
{
	DestroyEffect();

	m_pBeam = CBeam::BeamCreate( EGON_BEAM_SPRITE, 40 );
	m_pBeam->PointEntInit( pev->origin, m_pPlayer->entindex() );
	m_pBeam->SetEndAttachment( 1 );
	m_pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;	// Flag these to be destroyed on save/restore or level transition
	m_pBeam->SetScrollRate( 10 );
	m_pBeam->SetNoise( Configuration.noise );
	m_pBeam->SetWidth( Configuration.width + 10 );
}


void CBeamGun::DestroyEffect( void )
{
	if ( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}
	if ( m_pNoise )
	{
		UTIL_Remove( m_pNoise );
		m_pNoise = NULL;
	}
	if ( m_pLightningBeam )
	{
		UTIL_Remove( m_pLightningBeam );
		m_pNoise = NULL;	// (sic)
	}
	if ( m_pSprite )
	{
		m_pSprite->Expand( 10, 500 );
		m_pSprite = NULL;
	}
}


//=========================================================
// ShotgunLightning - the short range beam's burst: seven
// bolts and a white flash
//=========================================================
void CBeamGun::ShotgunLightning( Vector vecSrc, Vector vecDir, TraceResult tr )
{
	DestroyEffect();

	if ( gpGlobals->time > m_flDrawShotgunLightning && m_flDrawShotgunLightning != 0 )
	{
		m_fireState = FIRE_OFF;
		m_flDrawShotgunLightning = 0;
		m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0;
		return;
	}

	if ( !g_pGameRules->IsMultiplayer() )
		Create( "spark_shower", tr.vecEndPos, tr.vecPlaneNormal, NULL );

	int i = 0;
	while ( 1 )
	{
		float x, y, z;
		float flSpread = 0.2;
		do {
			x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
			y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
			z = x*x+y*y;
		} while (z > 1);

		UTIL_ScreenFade( m_pPlayer, Vector( 255, 255, 255 ), 1.0, 0.1, 150, 0 );
		m_flTimeStartOverheating = gpGlobals->time;

		Vector vecSpreadDir = vecDir +
						x * flSpread * gpGlobals->v_right +
						y * flSpread * gpGlobals->v_up;
		Vector vecStart = vecSrc + gpGlobals->v_right * 4;
		float flRange = Configuration.range;
		Vector vecEnd = vecSpreadDir * flRange + vecSrc;
		TraceResult tr2;

		UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr2 );

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr2.vecEndPos );
			WRITE_BYTE( TE_BEAMENTPOINT );
			WRITE_SHORT( m_pPlayer->entindex() + 0x1000 );
			WRITE_COORD( tr2.vecEndPos.x );
			WRITE_COORD( tr2.vecEndPos.y );
			WRITE_COORD( tr2.vecEndPos.z );
			WRITE_SHORT( m_iBeamLight );
			WRITE_BYTE( 0 );	// framestart
			WRITE_BYTE( 0 );	// framerate
			WRITE_BYTE( 1 );	// life
			WRITE_BYTE( 35 );	// width
			WRITE_BYTE( 15 );	// noise
			WRITE_BYTE( 255 );	// r, g, b
			WRITE_BYTE( 255 );
			WRITE_BYTE( 128 );
			WRITE_BYTE( 200 );	// brightness
			WRITE_BYTE( 0 );	// speed
		MESSAGE_END();

		if ( ++i >= 7 )
			break;
	}
}


#ifndef CLIENT_DLL
void CBeamGun::UpdateOwner( void )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_FREQUENCY );
		WRITE_BYTE( Configuration.frequency );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_CURRENT );
		WRITE_BYTE( Configuration.current );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_VOLTAGE );
		WRITE_BYTE( Configuration.voltage - 1 );
	MESSAGE_END();
}
#endif


void CBeamGun::InventoryFrame( void )
{
	bool bSendTemp = false;

	if ( m_fOverheating )
	{
		if ( gpGlobals->time - m_flTimeStartOverheating >= 5.0 )
		{
			m_fOverheating = FALSE;
			m_iTemp = 70;
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgTempX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_BEAMGUN );
			WRITE_BYTE( m_iTemp );
		MESSAGE_END();
	}
	else if ( m_flNextTempDecreaseTime < gpGlobals->time )
	{
		// cools down
		m_iTemp--;
		if ( m_iTemp < 1 )
			m_iTemp = 1;

		if ( this == m_pPlayer->m_pActiveItem )
			bSendTemp = true;

		m_flNextTempDecreaseTime = gpGlobals->time + 0.2;

#ifndef CLIENT_DLL
		if ( bSendTemp )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgTempX, NULL, m_pPlayer->pev );
				WRITE_BYTE( WEAPON_BEAMGUN );
				WRITE_BYTE( m_iTemp );
			MESSAGE_END();
		}
#endif
	}
}


void CBeamGun::WeaponIdle( void )
{
	TraceResult tr;

	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	m_flStartAttack = 0;

	if ( gpGlobals->time > m_flDrawShotgunLightning && m_flDrawShotgunLightning != 0 )
	{
		m_fireState = FIRE_OFF;
		m_flDrawShotgunLightning = 0;
		m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0;
		return;
	}

	if ( ( m_flTimeStartOverheating != 0 && gpGlobals->time - m_flTimeStartOverheating > 0.5 ) ||
		( m_flTimeStartOverheating != 0 && !m_fOverheating ) )
	{
		PLAYBACK_EVENT_FULL( 0, m_pPlayer->edict(), m_usMinigunHeat, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 1 );
		SendWeaponAnim( BEAMGUN_IDLESUBDUEL );
		m_flTimeStartOverheating = 0;
	}

	if ( gpGlobals->time < m_flDrawShotgunLightning )
	{
		// the short range burst goes on while the trigger is up
		UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
		Vector vecAiming = gpGlobals->v_forward;
		Vector vecSrc	 = m_pPlayer->GetGunPosition( );
		Vector vecEnd;
		vecEnd	 = vecSrc + vecAiming * Configuration.range;

		UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );
		ShotgunLightning( vecSrc, vecAiming, tr );

		if ( m_flAmmoUseTime <= gpGlobals->time )
		{
			UseAmmo( 1 );
			m_flAmmoUseTime = gpGlobals->time + 0.1;
		}
	}
	else if ( m_flTimeWeaponIdle <= gpGlobals->time )
	{
		if ( m_fireState != FIRE_OFF )
			EndAttack();

		if ( m_flNextIdleAnimTime < gpGlobals->time )
		{
			switch ( RANDOM_LONG( 0, 16 ) )
			{
			case 0:
				SendWeaponAnim( BEAMGUN_IDLEBLADE );
				m_flTimeWeaponIdle = gpGlobals->time;
				break;
			case 1:
				SendWeaponAnim( BEAMGUN_IDLEINSPECT );
				m_flTimeWeaponIdle = gpGlobals->time;
				break;
			default:
				SendWeaponAnim( BEAMGUN_IDLESUBDUEL );
				m_flTimeWeaponIdle = gpGlobals->time;
				break;
			}
			m_flNextIdleAnimTime = gpGlobals->time + 2.5;
		}
	}
}



void CBeamGun::EndAttack( void )
{
	STOP_SOUND( ENT(m_pPlayer->pev), CHAN_STATIC, EGON_SOUND_RUN );
	if ( gpGlobals->time > m_flDrawShotgunLightning )
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, EGON_SOUND_OFF, 0.98, ATTN_NORM, 0, 100);
	m_fireState = FIRE_OFF;
	m_flTimeWeaponIdle = gpGlobals->time + 2.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 0.5;
	DestroyEffect();
}


//=========================================================
// ShootTracer - a seeking spark off a high voltage beam
//=========================================================
void CBeamGun::ShootTracer( void )
{
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecAiming = gpGlobals->v_forward;

	m_iTracerSpread = m_iTracerSpread - 0.01;
	if ( m_iTracerSpread < 0 || m_iTracerSpread > 0.11 )
		m_iTracerSpread = 0;

	float x, y, z;
	do {
		x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		z = x*x+y*y;
	} while (z > 1);

	vecAiming = vecAiming +
				x * m_iTracerSpread * gpGlobals->v_right +
				y * m_iTracerSpread * gpGlobals->v_up;

	Vector vecSrc = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12;

	CBaseEntity *pTracer = CBaseEntity::Create( "tracer", vecSrc, m_pPlayer->pev->angles, m_pPlayer->edict() );
	pTracer->pev->velocity = vecAiming * 600;
}


void CBeamGun::Customize( char cOption, int iValue )
{
	switch ( cOption )
	{
	case '1':
		Configuration.frequency = iValue;
		break;
	case '2':
		Configuration.current = iValue;
		break;
	case '3':
		Configuration.voltage = iValue + 1;
		break;
	}

	if ( gpGlobals->time - m_flLastCustomTime > 5 )
	{
		m_flLastCustomTime = gpGlobals->time;
		SendWeaponAnim( BEAMGUN_CONFIG );
	}
}


//=========================================================
// Discharge - works the beam out from the sliders, then fires
// it, or now and then throws ball lightning instead; heats
// the gun up
//=========================================================
void CBeamGun::Discharge( const Vector &vecOrigSrc, const Vector &vecDir )
{
	Configuration.red = ( Configuration.voltage * 120 ) - ( Configuration.current * 5 );
	Configuration.green = ( Configuration.current * 114 ) - ( Configuration.voltage * 13 ) - Configuration.frequency;
	Configuration.blue = ( Configuration.frequency * 100 ) - ( Configuration.current * 10 );

	if ( Configuration.red > 255 )
		Configuration.red = 255;
	if ( Configuration.red < 0 )
		Configuration.red = 0;
	if ( Configuration.green > 255 )
		Configuration.green = 255;
	if ( Configuration.green < 0 )
		Configuration.green = 0;
	if ( Configuration.blue > 255 )
		Configuration.blue = 255;
	if ( Configuration.blue < 0 )
		Configuration.blue = 0;

	Configuration.damage = Configuration.current * 2.5;
	Configuration.shockdamage = Configuration.voltage * 4;

	switch ( Configuration.frequency )
	{
	case 1:
		Configuration.shockdamage += 55;
	case 2:
		if ( g_pGameRules->IsMultiplayer() )
			Configuration.shockdamage += 70;
		else
			Configuration.shockdamage += 100;
	case 3:
		Configuration.damage += 1;
	}

	if ( g_pGameRules->IsMultiplayer() )
		Configuration.shockdamage += 2;

	if ( Configuration.damage < 0 )
		Configuration.damage = 1;
	if ( Configuration.shockdamage < 0 )
		Configuration.shockdamage = 1;
	if ( Configuration.damage + Configuration.shockdamage < 8 )
		Configuration.damage = 8;

	Configuration.width = 45 - (int)( Configuration.current * -6 );
	if ( Configuration.frequency < 3 )
		Configuration.width -= 20;

	Configuration.noise = (int)( ( Configuration.current - Configuration.voltage ) * 1.3 ) + ( Configuration.frequency - Configuration.voltage ) * 2;
	if ( Configuration.noise < 0 )
		Configuration.noise = 0;
	if ( Configuration.frequency < 3 )
		Configuration.noise += 50;

	switch ( (int)Configuration.current )
	{
	case 1:
		Configuration.spread = VECTOR_CONE_1DEGREES;
		break;
	case 2:
		Configuration.spread = VECTOR_CONE_4DEGREES;
		break;
	case 3:
		Configuration.spread = VECTOR_CONE_7DEGREES;
		break;
	case 4:
		Configuration.spread = VECTOR_CONE_10DEGREES;
		break;
	}

	if ( Configuration.frequency < 3 )
	{
		if ( Configuration.frequency == 2 )
			Configuration.spread = VECTOR_CONE_10DEGREES;
		else
			Configuration.spread = VECTOR_CONE_15DEGREES;
	}

	switch ( Configuration.frequency )
	{
	case 1:
		Configuration.range = 100;
		m_iFireType = 1;
		break;
	case 2:
		Configuration.range = 200;
		m_iFireType = 1;
		break;
	case 3:
		Configuration.range = 880;
		m_iFireType = 0;
		break;
	case 4:
		Configuration.range = 8192;
		m_iFireType = 0;
		break;
	default:
		Configuration.range = ( 18 - Configuration.frequency ) * 100;
		m_iFireType = 0;
		break;
	}

	if ( Configuration.voltage > 2 )
		Configuration.flUnused = 0.85;
	else
		Configuration.flUnused = 0;

	if ( Configuration.voltage > 3 )
		Configuration.ballchance = 0.1;
	else
		Configuration.ballchance = 0;

	if ( RANDOM_FLOAT( 0, 1 ) < Configuration.ballchance && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= 10 )
	{
		if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 20 )
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = 0;
		else
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 20;

		m_iTemp = ( m_iTemp <= 75 ) ? 90 : 100;

		CBallLightning::Shoot( vecOrigSrc, m_pPlayer->pev->v_angle, m_pPlayer, 400 );
		m_pPlayer->m_flNextAttack = gpGlobals->time + 2.0;
		EndAttack();

		if ( m_iTemp >= 100 )
		{
			m_iTemp = 100;
			m_fOverheating = TRUE;
			m_flTimeStartOverheating = gpGlobals->time;
			EndAttack();
			EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/electro4.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f) );
			m_flTimeWeaponIdle = gpGlobals->time + 0.2;
			m_pPlayer->m_flNextAttack = gpGlobals->time + 5.0;
			m_pPlayer->TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), 40, DMG_SHOCK );
			PLAYBACK_EVENT_FULL( 0, m_pPlayer->edict(), m_usMinigunHeat, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 1, 1 );
			return;
		}
	}
	else
	{
		if ( m_iTemp >= 80 )
		{
			m_flTimeStartOverheating = gpGlobals->time;
			PLAYBACK_EVENT_FULL( 0, m_pPlayer->edict(), m_usMinigunHeat, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 1, 1 );
		}

		Fire( vecOrigSrc, vecDir );
	}

	if ( m_flNextTempChangeTime < gpGlobals->time )
	{
		switch ( (int)Configuration.current )
		{
		case 4:
			m_iTemp++;
		case 3:
			m_iTemp++;
		case 2:
			m_iTemp++;
		}

		m_iTemp++;
		if ( m_iTemp >= 100 )
		{
			m_iTemp = 100;
			m_fOverheating = TRUE;
			m_flTimeStartOverheating = gpGlobals->time;
			EndAttack();
			EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/electro4.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f) );
			m_flTimeWeaponIdle = gpGlobals->time + 0.2;
			m_pPlayer->m_flNextAttack = gpGlobals->time + 5.0;
			m_pPlayer->TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), 40, DMG_SHOCK );
			PLAYBACK_EVENT_FULL( 0, m_pPlayer->edict(), m_usMinigunHeat, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 1, 1 );
			return;
		}
		else if ( g_pGameRules->IsMultiplayer() )
		{
			m_flNextTempChangeTime = gpGlobals->time + 0.16;
		}
		else
		{
			m_flNextTempChangeTime = gpGlobals->time + 0.1;
		}
	}
}


//=========================================================
// ChainLightning - arcs from where the beam hits to the
// nearest living thing, and on from there, up to four times
//=========================================================
void CBeamGun::ChainLightning( const Vector &vecSrc )
{
	CBaseEntity *pList[100];
	CBaseEntity *pHit[100];
	Vector vecPos = vecSrc;
	CBaseEntity *pLast = NULL;
	int iHits = 0;

	for ( int iLink = 1; iLink < 5; iLink++ )
	{
		CBaseEntity *pTarget = NULL;
		int count = UTIL_EntitiesInBox( pList, 100, vecPos - Vector( 512, 512, 512 ), vecPos + Vector( 512, 512, 512 ), FL_CLIENT|FL_MONSTER );

		if ( count < 1 )
			return;

		int iMinDist = 8120;
		for ( int i = 0; i < count; i++ )
		{
			if ( pList[i] && pList[i]->IsAlive() && pList[i]->pev->takedamage && m_pPlayer->IRelationship( pList[i] ) > R_NO )
			{
				int iDist = ( pList[i]->BodyTarget( vecPos ) - vecPos ).Length();
				if ( iDist <= iMinDist )
				{
					BOOL fHit = FALSE;
					for ( int j = 0; j < iHits; j++ )
					{
						if ( pHit[j] == pList[i] )
						{
							fHit = TRUE;
							break;
						}
					}
					if ( !fHit )
					{
						pTarget = pList[i];
						iMinDist = iDist;
						pHit[iHits++] = pTarget;
					}
				}
			}
		}

		if ( !pTarget )
			return;

		if ( pTarget->BodyTarget( vecPos ) == vecPos )
			return;

		TraceResult tr;
		UTIL_TraceLine( vecPos, pTarget->BodyTarget( vecPos ), dont_ignore_monsters, pLast ? pLast->edict() : m_pPlayer->edict(), &tr );
		if ( tr.pHit != pTarget->edict() )
			return;

		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecPos );
			WRITE_BYTE( TE_BEAMPOINTS );
			WRITE_COORD( vecPos.x );
			WRITE_COORD( vecPos.y );
			WRITE_COORD( vecPos.z );
			WRITE_COORD( pTarget->BodyTarget( vecPos ).x );
			WRITE_COORD( pTarget->BodyTarget( vecPos ).y );
			WRITE_COORD( pTarget->BodyTarget( vecPos ).z );
			WRITE_SHORT( m_iBeamLight );
			WRITE_BYTE( 0 );	// framestart
			WRITE_BYTE( 0 );	// framerate
			WRITE_BYTE( 7 );	// life
			WRITE_BYTE( 25 );	// width
			WRITE_BYTE( 50 );	// noise
			WRITE_BYTE( 255 );	// r, g, b
			WRITE_BYTE( 255 );
			WRITE_BYTE( 128 );
			WRITE_BYTE( 200 );	// brightness
			WRITE_BYTE( 0 );	// speed
		MESSAGE_END();

		float flDamage;
		if ( g_pGameRules->IsMultiplayer() )
			flDamage = Configuration.shockdamage;
		else
			flDamage = Configuration.shockdamage * 2.0;
		if ( flDamage > 8 )
			flDamage = 8;

		if ( pTarget != m_pPlayer )
			pTarget->TakeDamage( m_pPlayer->pev, m_pPlayer->pev, flDamage, DMG_SHOCK );

		vecPos = pTarget->BodyTarget( vecPos );
		pLast = pTarget;
	}
}


//=========================================================
// tracer: a spark that wanders off the beam
//=========================================================
class CLightningTracer : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify ( void );
	int  IRelationship ( CBaseEntity *pTarget );
#ifndef CLIENT_DLL
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
#endif

	void EXPORT TrackTarget ( void );
	void EXPORT TrackTouch ( CBaseEntity *pOther );
	void EXPORT DieTouch ( CBaseEntity *pOther );
	void EXPORT LightningTracerDie ( void );
	void DrawTrail( void );

	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	float			m_flStopAttack;
	float			m_flFlySpeed;
	Vector			m_vecTrailStart;
	BOOL			m_fBuzz;
	float			m_flNextTrail;
	int				m_iTrail;
	float			m_flNextBuzz;
};

LINK_ENTITY_TO_CLASS( tracer, CLightningTracer );

#ifndef CLIENT_DLL
TYPEDESCRIPTION	CLightningTracer::m_SaveData[] =
{
	DEFINE_FIELD( CLightningTracer, m_flStopAttack, FIELD_TIME ),
	DEFINE_FIELD( CLightningTracer, m_flFlySpeed, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CLightningTracer, CBaseMonster );
#endif

int CLightningTracer :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// filter these bits a little.
	bitsDamageType &= ~ ( DMG_ALWAYSGIB );
	bitsDamageType |= DMG_NEVERGIB;

	return CBaseMonster :: TakeDamage ( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CLightningTracer :: Spawn( void )
{
	Precache();

	pev->movetype	= MOVETYPE_FLY;
	pev->solid		= SOLID_NOT;
	pev->takedamage = DAMAGE_YES;
	pev->flags		|= FL_MONSTER;
	pev->health		= 1;// weak!
	pev->gravity	= 0;

	m_flNextBuzz = gpGlobals->time + RANDOM_FLOAT( 0.4, 3 );
	m_flStopAttack	= gpGlobals->time + 3.5;
	m_flFieldOfView = 0.9; // +- 25 degrees
	m_flFlySpeed = 300;

	SET_MODEL(ENT( pev ), "models/tubeball.mdl");
	UTIL_SetSize( pev, Vector( -4, -4, -4 ), Vector( 4, 4, 4 ) );

	SetTouch( &CLightningTracer::TrackTouch );
	SetThink( &CLightningTracer::TrackTarget );

	pev->nextthink = gpGlobals->time + 0.1;
	ResetSequenceInfo( );

	m_fBuzz = TRUE;
	m_vecTrailStart.x = 0;
	pev->effects |= EF_NODRAW;
}


void CLightningTracer :: Precache()
{
	PRECACHE_MODEL("models/tubeball.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/zbeam5.spr");

	PRECACHE_SOUND( "weapons/residual1.wav" );
	PRECACHE_SOUND( "weapons/residual2.wav" );
	PRECACHE_SOUND( "weapons/residual3.wav" );
	PRECACHE_SOUND( "weapons/residual4.wav" );
}

//=========================================================
// tracers will never get mad at each other, no matter who
// the owner is.
//=========================================================
int CLightningTracer::IRelationship ( CBaseEntity *pTarget )
{
	if ( pTarget->pev->owner == pev->owner )
	{
		return R_NO;
	}

	return CBaseMonster :: IRelationship( pTarget );
}

//=========================================================
// ID's tracer as their owner
//=========================================================
int CLightningTracer::Classify ( void )
{

	if ( pev->owner && pev->owner->v.flags & FL_CLIENT)
	{
		return CLASS_PLAYER_BIOWEAPON;
	}

	return	CLASS_ALIEN_BIOWEAPON;
}

//=========================================================
// Tracer is flying, gently tracking target
//=========================================================
void CLightningTracer :: TrackTarget ( void )
{
	Vector	vecFlightDir;
	Vector	vecDirToEnemy;
	float	flDelta;

	if (m_flStopAttack < gpGlobals->time)
	{
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}

	m_hEnemy = NULL;
	m_vecEnemyLKP = m_vecEnemyLKP + pev->velocity * m_flFlySpeed * 0.1;

	vecDirToEnemy = ( m_vecEnemyLKP - pev->origin ).Normalize();

	if (pev->velocity.Length() < 0.1)
		vecFlightDir = vecDirToEnemy;
	else
		vecFlightDir = pev->velocity.Normalize();

	// measure how far the turn is, the wider the turn, the slow we'll go this time.
	flDelta = DotProduct ( vecFlightDir, vecDirToEnemy );

	if ( flDelta <= 0 )
	{// no flying backwards, but we don't want to invert this, cause we'd go fast when we have to turn REAL far.
		flDelta = 0.25;
	}

	pev->velocity = ( vecFlightDir + vecDirToEnemy).Normalize();

	if ( pev->owner && (pev->owner->v.flags & FL_MONSTER) )
	{
		// random pattern only applies to tracers fired by monsters, not players.

		pev->velocity.x += RANDOM_FLOAT ( -0.10, 0.10 );// scramble the flight dir a bit.
		pev->velocity.y += RANDOM_FLOAT ( -0.10, 0.10 );
		pev->velocity.z += RANDOM_FLOAT ( -0.10, 0.10 );
	}

	pev->velocity = pev->velocity * ( m_flFlySpeed * flDelta );// scale the dir by the ( speed * width of turn )
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1, 0.3 );

	pev->angles = UTIL_VecToAngles (pev->velocity);

	// if tracer is close to the enemy, jet in a straight line for a second.
	if ( m_hEnemy != NULL )
	{
		if ( flDelta >= 0.4 && ( pev->origin - m_vecEnemyLKP ).Length() <= 300 )
		{
			pev->velocity = pev->velocity * 2;
			pev->nextthink = gpGlobals->time + 1.0;
		}
	}

	if ( !m_fBuzz )
		return;

	if ( m_flNextBuzz < gpGlobals->time )
	{
		switch (RANDOM_LONG(0,3))
		{
		case 0:	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/residual1.wav", 0.25, ATTN_NORM, 0, 125 );	break;
		case 1:	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/residual2.wav", 0.3, ATTN_NORM, 0, 125 );	break;
		case 2:	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/residual3.wav", 0.4, ATTN_NORM, 0, 125 );	break;
		case 3:	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/residual4.wav", 0.5, ATTN_NORM, 0, 125 );	break;
		}
	}
	m_flNextBuzz = gpGlobals->time + RANDOM_FLOAT( 1, 5 );	// every think (sic)

	DrawTrail();
}

//=========================================================
// Tracking tracer hit something
//=========================================================
void CLightningTracer :: TrackTouch ( CBaseEntity *pOther )
{
	if ( pOther->edict() == pev->owner || pOther->pev->owner == pev->owner )
	{// bumped into the guy that shot it.
		pev->solid = SOLID_NOT;
		return;
	}

	if ( IRelationship( pOther ) <= R_NO )
	{
		m_fBuzz = TRUE;
		m_flFlySpeed = 400;
		return;
	}

	DieTouch( pOther );
}

void CLightningTracer::DieTouch ( CBaseEntity *pOther )
{
	SetThink( &CLightningTracer::LightningTracerDie );
	SetTouch( NULL );
	pev->solid = SOLID_NOT;
	pev->nextthink = gpGlobals->time + 1;
}

void CLightningTracer::LightningTracerDie( void )
{
	UTIL_Remove( this );
}

//=========================================================
// DrawTrail - a bolt from where the tracer was half a
// second ago
//=========================================================
void CLightningTracer::DrawTrail( void )
{
	if ( m_flNextTrail < gpGlobals->time )
	{
		m_flNextTrail = gpGlobals->time + 0.5;

		if ( m_vecTrailStart.x == 0 )
		{
			m_vecTrailStart = pev->origin;
			return;
		}

		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, m_vecTrailStart );
			WRITE_BYTE( TE_BEAMPOINTS );
			WRITE_COORD( m_vecTrailStart.x );
			WRITE_COORD( m_vecTrailStart.y );
			WRITE_COORD( m_vecTrailStart.z );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_SHORT( m_iTrail );
			WRITE_BYTE( 0 );	// framestart
			WRITE_BYTE( 0 );	// framerate
			WRITE_BYTE( 3 );	// life
			WRITE_BYTE( 25 );	// width
			WRITE_BYTE( 50 );	// noise
			WRITE_BYTE( 255 );	// r, g, b
			WRITE_BYTE( 255 );
			WRITE_BYTE( 128 );
			WRITE_BYTE( 200 );	// brightness
			WRITE_BYTE( 0 );	// speed
		MESSAGE_END();

		m_vecTrailStart = pev->origin;
	}
}


CBallLightning *CBallLightning::Shoot( Vector vecStart, Vector vecAngles, CBaseEntity *pOwner, int iSpeed )
{
	CBallLightning *pBall = GetClassPtr( (CBallLightning *)NULL );

	pBall->pev->owner = pOwner->edict();
	pBall->Spawn();
	UTIL_SetOrigin( pBall->pev, vecStart );
	pBall->pev->angles = vecAngles;
	UTIL_MakeVectors( vecAngles );
	pBall->pev->velocity = gpGlobals->v_forward * iSpeed;

	return pBall;
}

void CBallLightning::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT(pev), "sprites/ballight.spr" );
	pev->rendermode = kRenderGlow;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->renderamt = 255;
	pev->renderfx = kRenderFxNoDissipation;

	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	pev->classname = MAKE_STRING( "ball_lightning" );

	SetTouch( &CBallLightning::LightningTouch );
	SetThink( &CBallLightning::MoveThink );
	pev->nextthink = gpGlobals->time + 0.1;

	pev->takedamage = DAMAGE_NO;
	pev->dmg = 0;
	pev->frame = 1;
	m_flSpawnTime = gpGlobals->time;

	if ( Classify() == CLASS_PLAYER )
	{
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/ball-fly.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f) );
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "flier/flyball_fly1.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f) );	break;
		case 1:	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "flier/flyball_fly2.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f) );	break;
		case 2:	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "flier/flyball_fly3.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f) );	break;
		}
	}
}

void CBallLightning::Precache( void )
{
	PRECACHE_MODEL( "sprites/ballpop.spr" );
	PRECACHE_MODEL( "sprites/ballight.spr" );
	m_iSmoke = PRECACHE_MODEL( "sprites/smoke.spr" );

	PRECACHE_SOUND( "weapons/ball-fly.wav" );
	PRECACHE_SOUND( "flier/flyball_fly1.wav" );
	PRECACHE_SOUND( "flier/flyball_fly2.wav" );
	PRECACHE_SOUND( "flier/flyball_fly3.wav" );
	PRECACHE_SOUND( "flier/flyball_hit1.wav" );
	PRECACHE_SOUND( "flier/flyball_hit2.wav" );
	PRECACHE_SOUND( "flier/flyball_hit3.wav" );
	PRECACHE_SOUND( "weapons/ball-die.wav" );

	m_usBallLightning = PRECACHE_EVENT( 1, "events/balllightning.sc" );

	UTIL_PrecacheOther( "lightning_bug" );
}

void CBallLightning::LightningTouch( CBaseEntity *pOther )
{
	if ( pOther->edict() == pev->owner )
		return;

	if ( pev->owner == pOther->pev->owner )
		return;

	SetTouch( NULL );

	if ( Classify() != CLASS_PLAYER )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "flier/flyball_hit1.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f) );	break;
		case 1:	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "flier/flyball_hit2.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f) );	break;
		case 2:	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "flier/flyball_hit3.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f) );	break;
		}
	}
	else
	{
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/ball-die.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f) );
	}

	PLAYBACK_EVENT_FULL( 0, edict(), m_usBallLightning, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 1 );

	if ( gpGlobals->time - m_flSpawnTime < 1 && FClassnameIs( pOther->pev, "worldspawn" ) )
	{
		// hit a wall right away: just go away
		pev->velocity = g_vecZero;
		pev->avelocity = g_vecZero;
		UTIL_Remove( this );
		return;
	}

	pev->movetype = MOVETYPE_NONE;
	pev->frame = 0;
	SetThink( &CBallLightning::PopThink );

	::RadiusDamage( pev->origin, pev, VARS( pev->owner ), 60, 40, CLASS_NONE, DMG_SHOCK );

	pev->effects = EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetTouch( NULL );
	pev->nextthink = gpGlobals->time + 0.3;
}

void CBallLightning::PopThink( void )
{
	UTIL_Remove( this );
}

CBaseEntity *ShootLightningBug( Vector vecStart, Vector vecAngles, edict_t *pentOwner, CBaseEntity *pBall );

void CBallLightning::MoveThink( void )
{
	pev->frame += 1;
	if ( pev->frame > 9 )
		pev->frame = 0;

	if ( pev->origin.x < -4096 || pev->origin.x > 4096 ||
		pev->origin.y < -4096 || pev->origin.y > 4096 ||
		pev->origin.z < -4096 || pev->origin.z > 4096 )
	{
		UTIL_Remove( this );
		return;
	}

	if ( gpGlobals->time - m_flSpawnTime > 0.25 )
		::RadiusDamage( pev->origin, pev, VARS( pev->owner ), 60, 50, CLASS_NONE, DMG_SHOCK );

	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_flNextBugTime < gpGlobals->time )
	{
		UTIL_MakeVectors( pev->angles );

		CBaseEntity *pBug = ShootLightningBug( pev->origin + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12, pev->v_angle, pev->owner, this );

		Vector vecDir;
		vecDir.x = RANDOM_FLOAT( -0.5, 0.5 );
		vecDir.y = RANDOM_FLOAT( -0.5, 0.5 );
		vecDir.z = RANDOM_FLOAT( 0.1, 0.5 );
		vecDir = vecDir.Normalize();

		pBug->pev->angles = UTIL_VecToAngles( vecDir );
		UTIL_MakeAimVectors( pBug->pev->angles );
		pBug->pev->velocity = gpGlobals->v_forward * 300;

		m_flNextBugTime = gpGlobals->time + 0.5;
	}
}

int CBallLightning::Classify( void )
{
	if ( pev->owner )
	{
		CBaseEntity *pOwner = (CBaseEntity *)GET_PRIVATE( pev->owner );
		if ( pOwner )
			return pOwner->Classify();
	}

	return CLASS_PLAYER_BIOWEAPON;
}


//=========================================================
// lightning bug: seeks out enemies around the ball
// lightning that spat it
//=========================================================
class CLightningBug : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify ( void );
	int  IRelationship ( CBaseEntity *pTarget );
#ifndef CLIENT_DLL
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
#endif

	void EXPORT TrackTargetBug ( void );
	void EXPORT DieTouchBug ( CBaseEntity *pOther );
	void EXPORT LightningBugDie ( void );

	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	CBaseEntity		*m_pBall;
	float			m_flStopAttack;
	float			m_flFlySpeed;
	int				m_iUnused;
	int				m_iTrail;
	float			m_flNextDamage;
	float			m_flEnergy;
	unsigned short	m_usBallLightning;
};

LINK_ENTITY_TO_CLASS( lightning_bug, CLightningBug );

#ifndef CLIENT_DLL
TYPEDESCRIPTION	CLightningBug::m_SaveData[] =
{
	DEFINE_FIELD( CLightningBug, m_flStopAttack, FIELD_TIME ),
	DEFINE_FIELD( CLightningBug, m_flFlySpeed, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CLightningBug, CBaseMonster );
#endif

int CLightningBug :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// filter these bits a little.
	bitsDamageType &= ~ ( DMG_ALWAYSGIB );
	bitsDamageType |= DMG_NEVERGIB;

	return CBaseMonster :: TakeDamage ( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CLightningBug :: Spawn( void )
{
	Precache();

	SET_MODEL( ENT(pev), "sprites/ballspark.spr" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	pev->solid		= SOLID_BBOX;
	pev->movetype	= MOVETYPE_FLY;
	pev->takedamage = DAMAGE_NO;
	pev->classname = MAKE_STRING( "lightning_bug" );

	pev->rendermode = kRenderGlow;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->renderamt = 255;
	pev->renderfx = kRenderFxNoDissipation;
	pev->frame = 1;

	m_flStopAttack = gpGlobals->time + 4;
	m_flFieldOfView = -1;
	m_flFlySpeed = 300;

	if ( !g_pGameRules->IsMultiplayer() )
	{
		switch ( g_iSkillLevel )
		{
		case SKILL_MEDIUM:
			m_flEnergy = RANDOM_FLOAT( 1.0, 1.3 );
			pev->dmg = 75;
			break;
		case SKILL_HARD:
			m_flEnergy = RANDOM_FLOAT( 1.3, 1.6 );
			m_flFlySpeed = 400;
			pev->dmg = 100;
			break;
		default:
			m_flEnergy = RANDOM_FLOAT( 0.7, 1.0 );
			pev->dmg = 25;
			break;
		}
	}
	else
	{
		m_flEnergy = RANDOM_FLOAT( 0.7, 1.6 );
		m_flFlySpeed = 300;
		m_flStopAttack = gpGlobals->time + 1.7;
		pev->dmg = 130;
	}

	SetTouch( &CLightningBug::DieTouchBug );
	SetThink( &CLightningBug::TrackTargetBug );

	m_flEnergy = m_flEnergy * m_flEnergy * m_flEnergy + 1;

	pev->nextthink = gpGlobals->time + 0.1;
}

void CLightningBug :: Precache()
{
	PRECACHE_MODEL( "sprites/ballspark.spr" );
	m_iTrail = PRECACHE_MODEL( "sprites/zbeam5.spr" );
	m_usBallLightning = PRECACHE_EVENT( 1, "events/balllightning.sc" );
}

//=========================================================
// bugs will never get mad at each other or at their owner
//=========================================================
int CLightningBug::IRelationship ( CBaseEntity *pTarget )
{
	if ( pTarget->pev->modelindex == pev->modelindex )
	{
		return R_NO;
	}

	if ( pTarget->edict() == pev->owner )
	{
		return R_NO;
	}

	return CBaseMonster :: IRelationship( pTarget );
}

int CLightningBug::Classify( void )
{
	if ( !g_pGameRules->IsMultiplayer() && pev->owner )
	{
		CBaseEntity *pOwner = (CBaseEntity *)GET_PRIVATE( pev->owner );
		if ( pOwner )
			return pOwner->Classify();
	}

	return CLASS_PLAYER_BIOWEAPON;
}

void CLightningBug :: TrackTargetBug ( void )
{
	Vector	vecFlightDir;
	Vector	vecDirToEnemy;
	float	flDelta;
	int		iDead;	// sic: never read. Five dead stores are the inline budget
					// retail has here (Length() inlined, operator* called);
					// four or six give other code
	iDead = 0;
	iDead = 1;
	iDead = 2;
	iDead = 3;
	iDead = 4;

	RANDOM_FLOAT( 0.1, 0.3 );

	pev->frame += 1;
	if ( pev->frame > 17 )
		pev->frame = 0;

	if ( gpGlobals->time > m_flStopAttack )
	{
		CSprite *pSprite = CSprite::SpriteCreate( "sprites/ballpop.spr", pev->origin, TRUE );
		pSprite->AnimateAndDie( 10 );
		pSprite->SetTransparency( kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation );
		pSprite->SetScale( 0.3 );
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_flNextDamage < gpGlobals->time )
	{
		CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin + pev->velocity * 0.5, pev->velocity.Length( ), 0.1 );
		if ( g_iSkillLevel != SKILL_EASY )
			::RadiusDamage( pev->origin, pev, VARS( pev->owner ), 40, 40, CLASS_NONE, DMG_SHOCK );
		m_flNextDamage = gpGlobals->time + 0.5;
	}

	CBaseEntity *pBall = UTIL_FindEntityByClassname( NULL, "ball_lightning" );
	if ( pBall && !g_pGameRules->IsMultiplayer() )
	{
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMPOINTS );
			WRITE_COORD( pBall->pev->origin.x );
			WRITE_COORD( pBall->pev->origin.y );
			WRITE_COORD( pBall->pev->origin.z );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_SHORT( m_iTrail );
			WRITE_BYTE( 0 );	// framestart
			WRITE_BYTE( 10 );	// framerate
			WRITE_BYTE( 1 );	// life
			WRITE_BYTE( 14 );	// width
			WRITE_BYTE( 50 );	// noise
			WRITE_BYTE( 255 );	// r, g, b
			WRITE_BYTE( 0 );
			WRITE_BYTE( 255 );
			WRITE_BYTE( 255 );	// brightness
			WRITE_BYTE( 0 );	// speed
		MESSAGE_END();
	}

	if ( pev->owner && !(pev->owner->v.flags & FL_CLIENT) && m_flEnergy <= 0 )
		return;

	if ( m_hEnemy == NULL )
	{// enemy is dead.
		Look( 512 );
		m_hEnemy = BestVisibleEnemy( );
	}

	if ( m_hEnemy != NULL && FVisible( m_hEnemy ))
	{
		m_vecEnemyLKP = m_hEnemy->BodyTarget( pev->origin );
	}
	else
	{
		m_vecEnemyLKP = m_vecEnemyLKP + pev->velocity * m_flFlySpeed * 0.1;
	}

	vecDirToEnemy = ( m_vecEnemyLKP - pev->origin ).Normalize();

	if (pev->velocity.Length() < 0.1)
		vecFlightDir = vecDirToEnemy;
	else
		vecFlightDir = pev->velocity.Normalize();

	// measure how far the turn is, the wider the turn, the slow we'll go this time.
	flDelta = DotProduct ( vecFlightDir, vecDirToEnemy );

	if ( flDelta <= 0 )
	{// no flying backwards, but we don't want to invert this, cause we'd go fast when we have to turn REAL far.
		flDelta = 0.25;
	}

	// wide turns wear it out
	m_flEnergy += flDelta - 1;

	pev->velocity = ( vecFlightDir + vecDirToEnemy).Normalize() * ( flDelta * m_flFlySpeed );

	pev->angles = UTIL_VecToAngles (pev->velocity);
}

void CLightningBug::DieTouchBug ( CBaseEntity *pOther )
{
	if ( pOther->edict() != pev->owner && pOther->pev->modelindex != pev->modelindex )
	{
		PLAYBACK_EVENT_FULL( 0, edict(), m_usBallLightning, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );

		if ( pOther && pOther->pev->takedamage )
		{
			if ( !pOther->IsPlayer() )
				pev->dmg = 130;

			pOther->TakeDamage( pev, VARS( pev->owner ), pev->dmg, DMG_SHOCK );
		}

		pev->effects = EF_NODRAW;
		SetThink( &CLightningBug::LightningBugDie );
		pev->nextthink = gpGlobals->time + 0.3;
		pev->solid = SOLID_NOT;
		SetTouch( NULL );
	}
}

void CLightningBug::LightningBugDie( void )
{
	UTIL_Remove( this );
}

CBaseEntity *ShootLightningBug( Vector vecStart, Vector vecAngles, edict_t *pentOwner, CBaseEntity *pBall )
{
	CLightningBug *pBug = GetClassPtr( (CLightningBug *)NULL );

	UTIL_SetOrigin( pBug->pev, vecStart );
	pBug->pev->angles = vecAngles;
	pBug->Spawn();
	pBug->pev->owner = pentOwner;
	pBug->m_pBall = pBall;

	return pBug;
}


class CBeamGunAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL(ENT(pev), "models/beamgunammo.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/beamgunammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if (pOther->GiveAmmo( gSkillData.beamgunClipAmmo, "battery", gSkillData.beamgunMaxAmmo ) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_beamgunclip, CBeamGunAmmo );


//=========================================================
// the beam gun's three customization items: one class
//=========================================================
class CCustBeam : public CBaseCustomization
{
public:
	void Spawn( void );
	int GetItemInfo(ItemInfo *p);
};
LINK_ENTITY_TO_CLASS( cust_1BeamFrequency, CCustBeam );
LINK_ENTITY_TO_CLASS( cust_2BeamCurrent, CCustBeam );
LINK_ENTITY_TO_CLASS( cust_3BeamVoltage, CCustBeam );

void CCustBeam::Spawn( )
{
	Precache( );

	if ( FClassnameIs( pev, "cust_1BeamFrequency" ) )
		m_iId = WEAPON_CUST_FREQUENCY;
	else if ( FClassnameIs( pev, "cust_2BeamCurrent" ) )
		m_iId = WEAPON_CUST_CURRENT;
	else if ( FClassnameIs( pev, "cust_3BeamVoltage" ) )
		m_iId = WEAPON_CUST_VOLTAGE;

	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
}

int CCustBeam::GetItemInfo(ItemInfo *p)
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

	if ( FClassnameIs( pev, "cust_1BeamFrequency" ) )
	{
		p->iId = WEAPON_CUST_FREQUENCY;
		p->iPosition = 8;
		p->iIsCustomization = 1;
		p->iCustomSettings = 4;
		p->iCustomDefault = 3;
		p->pszCustomizes = "weapon_beamgun";
	}
	else if ( FClassnameIs( pev, "cust_3BeamVoltage" ) )
	{
		p->iId = WEAPON_CUST_VOLTAGE;
		p->iPosition = 10;
		p->iIsCustomization = 1;
		p->iCustomSettings = 3;
		p->iCustomDefault = 2;
		p->pszCustomizes = "weapon_beamgun";
	}
	else if ( FClassnameIs( pev, "cust_2BeamCurrent" ) )
	{
		p->iId = WEAPON_CUST_CURRENT;
		p->iPosition = 9;
		p->iIsCustomization = 1;
		p->iCustomSettings = 4;
		p->iCustomDefault = 2;
		p->pszCustomizes = "weapon_beamgun";
	}

	p->iWeight = CUSTOMIZATION_WEIGHT;
	return 1;
}
