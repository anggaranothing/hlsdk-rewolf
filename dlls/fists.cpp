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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "fists.h"


#define	CROWBAR_BODYHIT_VOLUME 128
#define	CROWBAR_WALLHIT_VOLUME 300

extern int gmsgWeapPickup;

LINK_ENTITY_TO_CLASS( weapon_fists, CFists );

#ifndef CLIENT_DLL
TYPEDESCRIPTION	CFists::m_SaveData[] =
{
	DEFINE_FIELD( CFists, m_flDrawAnimTime, FIELD_TIME ),
	DEFINE_FIELD( CFists, m_iWeapon, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CFists, CBasePlayerWeapon );
#endif


enum fists_e {
	FISTS_IDLE = 0,
	FISTS_IDLEJUDO,
	FISTS_IDLEKICKASS,
	FISTS_RIGHTPUNCH,
	FISTS_LEFTPUNCH,
	FISTS_DOUBLEPUNCH,
	FISTS_READY,
	FISTS_HOLSTER,
	FISTS_KNIFEDRAW,
	FISTS_KNIFEHOLSTER,
	FISTS_IDLEKNIFE,
	FISTS_IDLEKNIFEINSPECT,
	FISTS_KNIFEATTACK1,
	FISTS_KNIFEATTACK2,
	FISTS_PUSHBUTTON
};


void CFists::Spawn( )
{
	Precache( );
	m_iId = WEAPON_FISTS;
	SET_MODEL(ENT(pev), "models/w_knife.mdl");
	m_iClip = -1;
	m_iWeapon = 1;
	pev->body = 1;
	m_vecLastRipplePos = g_vecZero;

	FallInit();// get ready to fall down.
}


void CFists::Precache( void )
{
	PRECACHE_MODEL("models/v_hands.mdl");
	PRECACHE_MODEL("models/w_crowbar.mdl");
	PRECACHE_MODEL("models/p_crowbar.mdl");
	PRECACHE_SOUND("weapons/Hands_IdleKickAss_F0.wav");
	PRECACHE_SOUND("weapons/LeftPunch.wav");
	PRECACHE_SOUND("weapons/LeftPunch2.wav");
	PRECACHE_SOUND("weapons/LeftPunch3.wav");
	PRECACHE_SOUND("weapons/RightPunch.wav");
	PRECACHE_SOUND("weapons/RightPunch2.wav");
	PRECACHE_SOUND("weapons/RightPunch3.wav");
	PRECACHE_SOUND("weapons/KnifeAttack1.wav");
	PRECACHE_SOUND("weapons/KnifeAttack1b.wav");
	PRECACHE_SOUND("weapons/KnifeAttack2.wav");
	PRECACHE_SOUND("weapons/KnifeAttack2b.wav");
	PRECACHE_SOUND("weapons/KnifeDraw.wav");
	PRECACHE_SOUND("weapons/KnifeHolster.wav");
	PRECACHE_SOUND("weapons/cbar_miss1.wav");
	PRECACHE_MODEL("models/w_knife.mdl");
	PRECACHE_MODEL("sprites/waterripple.spr");

	m_usWaterRipple = PRECACHE_EVENT( 1, "events/waterripple.sc" );
}

int CFists::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 0;
	p->iId = WEAPON_FISTS;
	p->iWeight = FISTS_WEIGHT;
	return 1;
}



BOOL CFists::Deploy( )
{
	int iAnim;

	m_flNextIdle = RANDOM_FLOAT( 3, 5 ) + gpGlobals->time;

	switch ( m_iWeapon )
	{
	case 0:
		iAnim = FISTS_READY;
		break;
	case 1:
		iAnim = FISTS_KNIFEDRAW;
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/KnifeDraw.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF));
		break;
	}

	return DefaultDeploy( "models/v_hands.mdl", "models/p_crowbar.mdl", iAnim, "crowbar" );
}


void FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
{
	int			i, j, k;
	float		distance;
	float		*minmaxs[2] = {mins, maxs};
	TraceResult tmpTrace;
	Vector		vecHullEnd = tr.vecEndPos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if ( tmpTrace.flFraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
				if ( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}


void CFists::PrimaryAttack()
{
	if (! Swing( 1 ))
	{
		SetThink( &CFists::SwingAgain );
		pev->nextthink = gpGlobals->time + 0.1;
	}

	m_flNextIdle = RANDOM_FLOAT( 8, 13 ) + gpGlobals->time;
}


void CFists::Smack( )
{
}


void CFists::SwingAgain( void )
{
	Swing( 0 );
}


int CFists::Swing( int fFirst )
{
	int iDamage = 0;
	int fDidHit = FALSE;

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
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction >= 1.0 )
	{
		if (fFirst)
		{
			// miss
			switch ( m_iWeapon )
			{
			case 0:
				m_flNextPrimaryAttack = gpGlobals->time + 0.4;
				switch( (m_iSwing++) % 2 )
				{
				case 0:
					SendWeaponAnim( FISTS_RIGHTPUNCH );
					// play wiff or swish sound
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF));
					break;
				case 1:
					SendWeaponAnim( FISTS_LEFTPUNCH );
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF));
					break;
				}
				break;

			case 1:
				switch( (m_iSwing++) % 2 )
				{
				case 0:
					SendWeaponAnim( FISTS_KNIFEATTACK1 );
					m_flNextPrimaryAttack = gpGlobals->time + 0.65;
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/KnifeAttack1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF));
					break;
				case 1:
					SendWeaponAnim( FISTS_KNIFEATTACK2 );
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/KnifeAttack1b.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF));
					m_flNextPrimaryAttack = gpGlobals->time + 0.5;
					break;
				}
				break;
			}

			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		// hit
		fDidHit = TRUE;

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		switch ( m_iWeapon )
		{
		case 0:
			m_flNextPrimaryAttack = gpGlobals->time + 0.2;
			switch( (m_iSwing++) % 2 )
			{
			case 0:
				SendWeaponAnim( FISTS_RIGHTPUNCH );
				iDamage = gSkillData.plrDmgOneHandPunch;
				switch( RANDOM_LONG(0,2) )
				{
				case 0:
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/RightPunch.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF)); break;
				case 1:
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/RightPunch2.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF)); break;
				case 2:
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/RightPunch3.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF)); break;
				}
				break;
			case 1:
				SendWeaponAnim( FISTS_LEFTPUNCH );
				iDamage = gSkillData.plrDmgOneHandPunch;
				switch( RANDOM_LONG(0,2) )
				{
				case 0:
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/RightPunch.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF)); break;
				case 1:
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/RightPunch2.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF)); break;
				case 2:
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/RightPunch3.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF)); break;
				}
				break;
			}
			break;

		case 1:
			switch( (m_iSwing++) % 2 )
			{
			case 0:
				SendWeaponAnim( FISTS_KNIFEATTACK1 );
				m_flNextPrimaryAttack = gpGlobals->time + 0.5;
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/KnifeAttack2.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF));
				iDamage = gSkillData.plrDmgKnife;
				break;
			case 1:
				SendWeaponAnim( FISTS_KNIFEATTACK2 );
				m_flNextPrimaryAttack = gpGlobals->time + 0.45;
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/KnifeAttack2b.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF));
				iDamage = gSkillData.plrDmgKnife;
				break;
			}
			break;
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		ClearMultiDamage( );
		if ( (m_flNextPrimaryAttack + 1 < gpGlobals->time) || g_pGameRules->IsMultiplayer() )
		{
			// first swing does full damage
			pEntity->TraceAttack(m_pPlayer->pev, iDamage, gpGlobals->v_forward, &tr, DMG_CLUB );
		}
		else
		{
			// subsequent swings do half
			pEntity->TraceAttack(m_pPlayer->pev, iDamage / 2, gpGlobals->v_forward, &tr, DMG_CLUB );
		}
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if (pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE)
			{
				m_pPlayer->m_iWeaponVolume = CROWBAR_BODYHIT_VOLUME;
				if (!pEntity->IsAlive() )
					return TRUE;
				else
					flVol = 0.1;

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_CROWBAR);
		}

		// delay the decal a bit
		m_trHit = tr;
		SetThink( &CFists::Smack );
		pev->nextthink = gpGlobals->time + 0.2;

		m_pPlayer->m_iWeaponVolume = flVol * CROWBAR_WALLHIT_VOLUME;
	}
	return fDidHit;
}


void CFists::WeaponIdle( void )
{
	ResetEmptySound( );

	// ripples when standing in water
	Vector vecFeet = m_pPlayer->pev->origin;
	vecFeet.z -= 36;
	float flDepth = UTIL_WaterLevel( vecFeet, vecFeet.z, vecFeet.z + 74 ) - vecFeet.z;

	if ( flDepth != 0 )
	{
		if ( m_vecLastRipplePos == m_pPlayer->pev->origin )
		{
			if ( gpGlobals->time - m_flLastRipple < 0.5 )
				goto noripple;
		}
		else if ( gpGlobals->time - m_flLastRipple < 0.18 )
			goto noripple;

		m_vecLastRipplePos = m_pPlayer->pev->origin;
		PLAYBACK_EVENT_FULL( 0, m_pPlayer->edict(), m_usWaterRipple, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, flDepth, 0.0, 0, 0, 0, 0 );
		m_flLastRipple = gpGlobals->time;
	}
noripple:

	if ( m_flDrawAnimTime != 0 && m_flDrawAnimTime < gpGlobals->time )
	{
		m_flDrawAnimTime = 0;
		switch ( m_iWeapon )
		{
		case 0:
			SendWeaponAnim( FISTS_READY );
			break;
		case 1:
			SendWeaponAnim( FISTS_KNIFEDRAW );
			break;
		}
	}

	if ( m_flNextIdle > gpGlobals->time )
		return;

	int iAnim;
	switch ( m_iWeapon )
	{
	case 0:
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			iAnim = FISTS_IDLE;
			break;
		case 1:
			iAnim = FISTS_IDLEJUDO;
			break;
		default:
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/Hands_IdleKickAss_F0.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));
			iAnim = FISTS_IDLEKICKASS;
			break;
		}
		break;
	case 1:
		iAnim = RANDOM_LONG( 0, 1 ) ? FISTS_IDLEKNIFEINSPECT : FISTS_IDLEKNIFE;
		break;
	}

	SendWeaponAnim( iAnim );
	m_flNextIdle = RANDOM_FLOAT( 8, 13 ) + gpGlobals->time;
}


int CFists::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();

		pPlayer->GiveNamedItem( "cust_1fistsKnife" );
		return TRUE;
	}
	return FALSE;
}


void CFists::Customize( char cOption, int iValue )
{
	switch ( cOption )
	{
	case '1':
		if ( m_iWeapon == 1 )
		{
			SendWeaponAnim( FISTS_KNIFEHOLSTER );
			m_iWeapon = 0;
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/KnifeHolster.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF));
			m_flDrawAnimTime = gpGlobals->time + 1.0;
		}
		else
		{
			if ( m_iWeapon == 0 )
			{
				SendWeaponAnim( FISTS_HOLSTER );
				m_flDrawAnimTime = gpGlobals->time + 1.0;
			}
			m_iWeapon = 1;
		}
		break;
	default:
		m_iWeapon = 0;
		break;
	}

	pev->body = m_iWeapon;
	m_flNextIdle = gpGlobals->time + 3;
	m_flNextPrimaryAttack = gpGlobals->time + 0.5;
}


//=========================================================
// the knife customization item
//=========================================================
class CCust1fistsKnife : public CBaseCustomization
{
public:
	void Spawn( void );
	int GetItemInfo(ItemInfo *p);
};
LINK_ENTITY_TO_CLASS( cust_1fistsKnife, CCust1fistsKnife );
LINK_ENTITY_TO_CLASS( customization_knife, CCust1fistsKnife );

void CCust1fistsKnife::Spawn( )
{
	Precache( );
	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
	m_iId = WEAPON_CUST_KNIFE;
}

int CCust1fistsKnife::GetItemInfo(ItemInfo *p)
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
	p->iId = WEAPON_CUST_KNIFE;
	p->iPosition = 14;
	p->iCustomSettings = 0;
	p->iCustomDefault = 0;
	p->pszCustomizes = "weapon_fists";
	return 1;
}
