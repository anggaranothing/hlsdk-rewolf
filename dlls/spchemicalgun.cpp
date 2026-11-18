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
// the chemical gun: its sliders mix acid, base and
// neutralizer, and the mixture decides what the blob does
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "effects.h"
#include "soundent.h"
#include "gamerules.h"
#include "chemgrenade.h"
#include "spchemicalgun.h"

extern int gmsgWeapPickup;
extern int gmsgSliderX;

enum chemgun_e {
	CHEMGUN_DRAW = 0,
	CHEMGUN_HOLSTER,
	CHEMGUN_IDLE,
	CHEMGUN_IDLEFIDGET,
	CHEMGUN_CHANGEMIXTURE,
	CHEMGUN_SHOOT
};


LINK_ENTITY_TO_CLASS( weapon_chemgun, CSPChemicalGun );
LINK_ENTITY_TO_CLASS( weapon_SPchemicalgun, CSPChemicalGun );

#ifndef CLIENT_DLL
TYPEDESCRIPTION	CSPChemicalGun::m_SaveData[] =
{
	DEFINE_FIELD( CSPChemicalGun, m_iVelocity, FIELD_FLOAT ),
	DEFINE_FIELD( CSPChemicalGun, ChemicalMixture.acid, FIELD_INTEGER ),
	DEFINE_FIELD( CSPChemicalGun, ChemicalMixture.base, FIELD_INTEGER ),
	DEFINE_FIELD( CSPChemicalGun, ChemicalMixture.nuet, FIELD_INTEGER ),
	DEFINE_FIELD( CSPChemicalGun, m_iPressure, FIELD_INTEGER ),
	DEFINE_FIELD( CSPChemicalGun, m_flTimeNextIdle, FIELD_TIME ),
	DEFINE_FIELD( CSPChemicalGun, m_vSpread, FIELD_VECTOR ),
};
IMPLEMENT_SAVERESTORE( CSPChemicalGun, CBasePlayerWeapon );
#endif

int CSPChemicalGun::PrimaryAmmoIndex( void )
{
	return m_iPrimaryAmmoType;
}

void CSPChemicalGun::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_SPchemicalgun" );
	Precache( );
	SET_MODEL(ENT(pev), "models/w_chemgun.mdl");

	m_iId = WEAPON_SPCHEMICALGUN;
	m_iVelocity = 850;
	m_iPressure = 3;
	m_vSpread = VECTOR_CONE_5DEGREES;

	ChemicalMixture.acid = 4;
	ChemicalMixture.base = 3;
	ChemicalMixture.nuet = 2;
	ChemicalMixture.chemical_type = 0;
	ChemicalMixture.color = 0;
	ChemicalMixture.explosion = 0;
	ChemicalMixture.stability = 0;
	ChemicalMixture.viscosity = 0;
	ChemicalMixture.volatility = 0;
	m_flLastMixture = 0;

	m_iDefaultAmmo = 10;

	FallInit();// get ready to fall down.

	m_flTimeNextIdle = 0;
}

void CSPChemicalGun::Precache( void )
{
	PRECACHE_MODEL("models/v_chemgun.mdl");
	PRECACHE_MODEL("models/w_chemgun.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("weapons/cg-fire.wav");
	PRECACHE_SOUND("weapons/empty.wav");

	UTIL_PrecacheOther( "chem_grenade" );

	PRECACHE_MODEL("models/p_hgun.mdl");
	m_iSplash = PRECACHE_MODEL( "sprites/greensplash.spr" );
}

int CSPChemicalGun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "ChemicalAcid";
	p->iMaxAmmo1 = gSkillData.chemMaxAmmo;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 7;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SPCHEMICALGUN;
	p->iWeight = -10;
	p->pszCustomizes = NULL;
	p->iIsCustomization = 0;
	p->iCustomSettings = 0;
	p->iCustomDefault = 0;

	return 1;
}

void CSPChemicalGun::UpdateOwner( void )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_PRESSURE );
		WRITE_BYTE( (m_iVelocity - 400) / 150 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_ACID );
		WRITE_BYTE( ChemicalMixture.acid + 1 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_BASE );
		WRITE_BYTE( ChemicalMixture.base + 1 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_NUET );
		WRITE_BYTE( ChemicalMixture.nuet + 1 );
	MESSAGE_END();
}

int CSPChemicalGun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();

		m_pPlayer = pPlayer;

		pPlayer->GiveNamedItem( "cust_1SPChemicalGunPressure" );
		pPlayer->GiveNamedItem( "cust_2ChemAcid" );
		pPlayer->GiveNamedItem( "cust_3ChemBase" );
		pPlayer->GiveNamedItem( "cust_4ChemNuet" );

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_PRESSURE );
			WRITE_BYTE( (m_iVelocity - 400) / 150 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_ACID );
			WRITE_BYTE( ChemicalMixture.acid + 1 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_BASE );
			WRITE_BYTE( ChemicalMixture.base + 1 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_NUET );
			WRITE_BYTE( ChemicalMixture.nuet + 1 );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

BOOL CSPChemicalGun::Deploy( )
{
	m_flTimeNextIdle = gpGlobals->time + RANDOM_FLOAT( 2, 5 );
	return DefaultDeploy( "models/v_chemgun.mdl", "models/p_hgun.mdl", CHEMGUN_DRAW, "hive" );
}

void CSPChemicalGun::PrimaryAttack( void )
{
	if ( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	CalcVolatility( );
	CalcExplosion( );

	// the more volatile, the more it costs
	int iAmmo;
	if ( ChemicalMixture.volatility < 50 )
		iAmmo = 1;
	if ( ChemicalMixture.volatility >= 50 )
		iAmmo = 2;
	if ( ChemicalMixture.volatility > 85 )
		iAmmo = 3;
	if ( ChemicalMixture.explosion == 3 )
		iAmmo++;
	iAmmo--;
	if ( iAmmo < 1 )
		iAmmo = 1;
	if ( !ChemicalMixture.base && !ChemicalMixture.acid && !ChemicalMixture.nuet )
		iAmmo = 0;

	if ( iAmmo > m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_DANGER;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;

	SendWeaponAnim( CHEMGUN_SHOOT );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	if ( !ChemicalMixture.acid && !ChemicalMixture.base && !ChemicalMixture.nuet )
		EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/empty.wav", 0.8, ATTN_NORM, 0, PITCH_NORM );
	else
		EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cg-fire.wav", 0.8, ATTN_NORM, 0, PITCH_NORM );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecDir = gpGlobals->v_forward;

	if ( m_vSpread != g_vecZero )
	{
		float x, y, z;

		// get circular gaussian spread
		do {
			x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
			y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
			z = x*x+y*y;
		} while (z > 1);

		vecDir = vecDir + x * m_vSpread.x * gpGlobals->v_right + y * m_vSpread.y * gpGlobals->v_up;
	}

	// the gun kicks back with the pressure
	float flZ = m_pPlayer->pev->velocity.z;
	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * (m_iPressure * 100 - 100);
	m_pPlayer->pev->velocity.z = flZ;

	LaunchChemical( m_pPlayer->pev, m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16, vecDir * m_iVelocity + gpGlobals->v_up * 100 );

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= iAmmo;

	m_flNextPrimaryAttack = gpGlobals->time + 1;
	m_flNextSecondaryAttack = gpGlobals->time + 0.25;
	m_flTimeNextIdle = gpGlobals->time + 5;

	m_pPlayer->pev->punchangle.x -= 10;
}

void CSPChemicalGun::Customize( char cOption, int iValue )
{
	switch ( cOption )
	{
	case '1':
		// pressure: the speed and the spread
		m_iVelocity = iValue * 150 + 400;
		switch ( iValue )
		{
		case 1:
			m_vSpread = g_vecZero;
			break;
		case 2:
			m_vSpread = VECTOR_CONE_3DEGREES;
			break;
		case 3:
			m_vSpread = VECTOR_CONE_7DEGREES;
			break;
		case 4:
			m_vSpread = VECTOR_CONE_10DEGREES;
			break;
		case 5:
			m_vSpread = VECTOR_CONE_15DEGREES;
			break;
		}
		m_iPressure = iValue;
		break;
	case '2':
		ChemicalMixture.acid = iValue - 1;
		break;
	case '3':
		ChemicalMixture.base = iValue - 1;
		break;
	case '4':
		ChemicalMixture.nuet = iValue - 1;
		break;
	}

	if ( gpGlobals->time - m_flLastMixture > 3 )
	{
		m_flLastMixture = gpGlobals->time;
		SendWeaponAnim( CHEMGUN_CHANGEMIXTURE );
		m_flTimeNextIdle = gpGlobals->time + 4;
	}
}

BOOL CSPChemicalGun::IsUseable( void )
{
	if ( m_iClip <= 0 )
	{
		ItemInfo II;
		GetItemInfo( &II );

		if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] <= 0 )
			return FALSE;
	}

	return TRUE;
}

void CSPChemicalGun::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeNextIdle <= gpGlobals->time )
	{
		int iAnim = RANDOM_LONG( 0, 4 ) ? CHEMGUN_IDLE : CHEMGUN_IDLEFIDGET;
		SendWeaponAnim( iAnim );
		m_flTimeNextIdle = gpGlobals->time + RANDOM_FLOAT( 3, 8 );
	}
}

//=========================================================
// LaunchChemical - mix and fire a blob
//=========================================================
void CSPChemicalGun::LaunchChemical( entvars_t *pevOwner, Vector vecSrc, Vector vecVelocity )
{
	CalcViscosity( );
	CalcVolatility( );
	CalcStability( );
	CalcExplosion( );

	if ( !ChemicalMixture.base && !ChemicalMixture.acid && !ChemicalMixture.nuet )
		return;

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		ChemicalMixture.volatility = ChemicalMixture.volatility * 0.85;
	}
	else if ( ChemicalMixture.stability < 3 )
	{
		ChemicalMixture.stability = 3;
	}

	if ( ChemicalMixture.chemical_type == 6 )
		CChemGrenade::ShootSticky( pevOwner, vecSrc, vecVelocity, ChemicalMixture );

	if ( ChemicalMixture.chemical_type == 7 )
		CChemGrenade::ShootBouncy( pevOwner, vecSrc, vecVelocity, ChemicalMixture );

	if ( ChemicalMixture.chemical_type == 8 )
		CChemGrenade::ShootSplatter( pevOwner, vecSrc, vecVelocity, ChemicalMixture );
}

//=========================================================
// CalcViscosity - acid and base thicken each other, the
// neutralizer thins; the thickness picks the blob
//=========================================================
int CSPChemicalGun::CalcViscosity( void )
{
	if ( !ChemicalMixture.base && !ChemicalMixture.acid && !ChemicalMixture.nuet )
	{
		ChemicalMixture.viscosity = 0;
		ChemicalMixture.chemical_type = 7;
		return 0;
	}

	if ( ( !ChemicalMixture.acid && !ChemicalMixture.base ) || ( !ChemicalMixture.acid && !ChemicalMixture.nuet ) || ( !ChemicalMixture.base && !ChemicalMixture.nuet ) )
	{
		// a single chemical splatters
		ChemicalMixture.chemical_type = 8;
		ChemicalMixture.viscosity = 100;
		return 100;
	}

	if ( ChemicalMixture.acid && ChemicalMixture.base )
		ChemicalMixture.viscosity = 69 - fabs( ChemicalMixture.acid - ChemicalMixture.base ) * 13;
	else
		ChemicalMixture.viscosity = 100;

	ChemicalMixture.viscosity -= ChemicalMixture.nuet * 13;

	if ( ChemicalMixture.viscosity < 45 )
	{
		ChemicalMixture.chemical_type = 7;	// thin: it bounces
		return ChemicalMixture.viscosity;
	}

	if ( ChemicalMixture.viscosity > 70 )
	{
		ChemicalMixture.chemical_type = 8;	// thick: it splatters
		return ChemicalMixture.viscosity;
	}

	ChemicalMixture.chemical_type = 6;		// in between: it sticks
	return ChemicalMixture.viscosity;
}

int CSPChemicalGun::CalcVolatility( void )
{
	ChemicalMixture.volatility = 0;

	if ( !ChemicalMixture.base && !ChemicalMixture.acid && !ChemicalMixture.nuet )
	{
		ChemicalMixture.volatility = 0;
		return 0;
	}

	if ( ( !ChemicalMixture.acid && !ChemicalMixture.base ) || ( !ChemicalMixture.acid && !ChemicalMixture.nuet ) || ( !ChemicalMixture.base && !ChemicalMixture.nuet ) )
		ChemicalMixture.volatility = ( ChemicalMixture.nuet + ChemicalMixture.acid + ChemicalMixture.base ) * 23;

	if ( ChemicalMixture.acid && ChemicalMixture.base )
	{
		ChemicalMixture.volatility = ( ChemicalMixture.acid + ChemicalMixture.base ) * 10;
		if ( ChemicalMixture.acid == ChemicalMixture.base )
			ChemicalMixture.volatility = ChemicalMixture.volatility * 1.5;
		ChemicalMixture.volatility = ( ChemicalMixture.volatility - ChemicalMixture.nuet * 2 ) * 1.15;
		return ChemicalMixture.volatility;
	}

	if ( ChemicalMixture.acid && ChemicalMixture.nuet )
		ChemicalMixture.volatility = ChemicalMixture.acid * 25 - ChemicalMixture.nuet * 2;

	if ( ChemicalMixture.base && ChemicalMixture.nuet )
		ChemicalMixture.volatility = ChemicalMixture.base * 25 - ChemicalMixture.nuet * 2;

	if ( ChemicalMixture.viscosity < 69 )
		ChemicalMixture.volatility = m_iVelocity / 25 + ChemicalMixture.volatility;

	if ( ChemicalMixture.volatility < 10 )
		ChemicalMixture.volatility = 25;

	ChemicalMixture.volatility = ChemicalMixture.volatility * 1.1;
	return ChemicalMixture.volatility;
}

int CSPChemicalGun::CalcStability( void )
{
	ChemicalMixture.stability = 55;

	if ( ChemicalMixture.acid && ChemicalMixture.base )
		ChemicalMixture.stability = fabs( ChemicalMixture.acid - ChemicalMixture.base ) * 10;

	ChemicalMixture.stability += ChemicalMixture.nuet * 20;
	return ChemicalMixture.stability;
}

int CSPChemicalGun::CalcExplosion( void )
{
	if ( !ChemicalMixture.base && !ChemicalMixture.acid && !ChemicalMixture.nuet )
		return 0;

	if ( ChemicalMixture.base && ChemicalMixture.acid )
	{
		// acid and base: explosive, more so when they are 3 apart
		if ( ChemicalMixture.acid - ChemicalMixture.base == 3 || ChemicalMixture.base - ChemicalMixture.acid == 3 )
			ChemicalMixture.explosion = 4;
		else
			ChemicalMixture.explosion = 3;
		return ChemicalMixture.explosion;
	}

	if ( !ChemicalMixture.base && !ChemicalMixture.nuet )
		ChemicalMixture.explosion = 1;

	if ( !ChemicalMixture.acid && !ChemicalMixture.nuet )
		ChemicalMixture.explosion = 2;

	if ( !ChemicalMixture.acid && !ChemicalMixture.base )
		ChemicalMixture.explosion = 5;

	if ( ChemicalMixture.acid && ChemicalMixture.nuet )
	{
		if ( ChemicalMixture.nuet > ChemicalMixture.acid )
			ChemicalMixture.explosion = 5;
		else
			ChemicalMixture.explosion = 1;
	}

	if ( ChemicalMixture.base && ChemicalMixture.nuet )
	{
		if ( ChemicalMixture.nuet > ChemicalMixture.base )
			ChemicalMixture.explosion = 2;
		else
			ChemicalMixture.explosion = 1;
	}

	return ChemicalMixture.explosion;
}


//=========================================================
// the chemical gun's four customization items: one class
//=========================================================
class CCustChemGun : public CBaseCustomization
{
public:
	void Spawn( void );
	int GetItemInfo(ItemInfo *p);
};
LINK_ENTITY_TO_CLASS( cust_1SPChemicalGunPressure, CCustChemGun );
LINK_ENTITY_TO_CLASS( cust_2ChemAcid, CCustChemGun );
LINK_ENTITY_TO_CLASS( cust_3ChemBase, CCustChemGun );
LINK_ENTITY_TO_CLASS( cust_4ChemNuet, CCustChemGun );

void CCustChemGun::Spawn( )
{
	Precache( );

	if ( FClassnameIs( pev, "cust_1SPChemicalGunPressure" ) )
		m_iId = WEAPON_CUST_PRESSURE;
	else if ( FClassnameIs( pev, "cust_2ChemAcid" ) )
		m_iId = WEAPON_CUST_ACID;
	else if ( FClassnameIs( pev, "cust_3ChemBase" ) )
		m_iId = WEAPON_CUST_BASE;
	else if ( FClassnameIs( pev, "cust_4ChemNuet" ) )
		m_iId = WEAPON_CUST_NUET;

	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
}

int CCustChemGun::GetItemInfo(ItemInfo *p)
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

	if ( FClassnameIs( pev, "cust_1SPChemicalGunPressure" ) )
	{
		p->iId = WEAPON_CUST_PRESSURE;
		p->iPosition = 6;
		p->iCustomSettings = 5;
		p->iCustomDefault = 3;
		p->pszCustomizes = "weapon_SPchemicalgun";
	}
	else if ( FClassnameIs( pev, "cust_2ChemAcid" ) )
	{
		p->iId = WEAPON_CUST_ACID;
		p->iPosition = 3;
		p->iIsCustomization = 1;
		p->iCustomSettings = 5;
		p->iCustomDefault = 4;
		p->pszCustomizes = "weapon_SPchemicalgun";
	}
	else if ( FClassnameIs( pev, "cust_3ChemBase" ) )
	{
		p->iId = WEAPON_CUST_BASE;
		p->iPosition = 5;
		p->iCustomSettings = 5;
		p->iCustomDefault = 3;
		p->pszCustomizes = "weapon_SPchemicalgun";
	}
	else if ( FClassnameIs( pev, "cust_4ChemNuet" ) )
	{
		p->iId = WEAPON_CUST_NUET;
		p->iPosition = 4;
		p->iCustomSettings = 5;
		p->iCustomDefault = 2;
		p->pszCustomizes = "weapon_SPchemicalgun";
	}

	p->iWeight = CUSTOMIZATION_WEIGHT;
	return 1;
}


class CChemicalAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL(ENT(pev), "models/chem_ammo.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/chem_ammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if (pOther->GiveAmmo( gSkillData.chemClipAmmo, "ChemicalAcid", gSkillData.chemMaxAmmo ) != -1)
		{
			if ( PlayPickupSound() )
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_chemical, CChemicalAmmo );
