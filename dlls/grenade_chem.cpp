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
// the chemical gun's blobs: they stick, bounce or splatter,
// and burn by acid, base, fire or explosion
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "effects.h"
#include "gamerules.h"
#include "soundent.h"
#include "decals.h"
#include "chemgrenade.h"

extern DLL_GLOBAL Vector g_vecAttackDir;

static void ChemicalRadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int iChemical );


LINK_ENTITY_TO_CLASS( chem_grenade, CChemGrenade );

TYPEDESCRIPTION	CChemGrenade::m_SaveData[] =
{
	DEFINE_FIELD( CChemGrenade, m_flTimeExploded, FIELD_TIME ),
	DEFINE_FIELD( CChemGrenade, m_bDontSpray, FIELD_INTEGER ),
	DEFINE_FIELD( CChemGrenade, m_pSmokeSprite, FIELD_CLASSPTR ),
	DEFINE_FIELD( CChemGrenade, ChemicalMixture.acid, FIELD_INTEGER ),
	DEFINE_FIELD( CChemGrenade, ChemicalMixture.base, FIELD_INTEGER ),
	DEFINE_FIELD( CChemGrenade, ChemicalMixture.nuet, FIELD_INTEGER ),
	DEFINE_FIELD( CChemGrenade, ChemicalMixture.viscosity, FIELD_INTEGER ),
	DEFINE_FIELD( CChemGrenade, ChemicalMixture.volatility, FIELD_INTEGER ),
	DEFINE_FIELD( CChemGrenade, ChemicalMixture.burn_rate, FIELD_INTEGER ),
	DEFINE_FIELD( CChemGrenade, ChemicalMixture.explosion, FIELD_INTEGER ),
	DEFINE_FIELD( CChemGrenade, ChemicalMixture.chemical_type, FIELD_INTEGER ),
	DEFINE_FIELD( CChemGrenade, ChemicalMixture.color, FIELD_INTEGER ),
	DEFINE_FIELD( CChemGrenade, ChemicalMixture.stability, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CChemGrenade, CGrenade );

void CChemGrenade::Spawn( void )
{
	Precache( );
	CGrenade::Spawn( );
	pev->classname = MAKE_STRING( "chem_grenade" );
	m_flTimeExploded = 0;
}

void CChemGrenade::Precache( void )
{
	PRECACHE_MODEL( "models/w_grenade.mdl" );
	PRECACHE_MODEL( "models/tubeball.mdl" );
	PRECACHE_MODEL( "models/stickygib.mdl" );
	PRECACHE_MODEL( "sprites/smoker_gray.spr" );
	PRECACHE_MODEL( "sprites/orangesplash.spr" );
	PRECACHE_SOUND( "weapons/fizzy1.wav" );
	PRECACHE_SOUND( "weapons/sprog1.wav" );
	PRECACHE_SOUND( "weapons/sprog2.wav" );
	PRECACHE_SOUND( "weapons/cgbounce.wav" );
	m_iGreenSplash = PRECACHE_MODEL( "sprites/greensplash.spr" );
	m_iRedSplash = PRECACHE_MODEL( "sprites/redsplash.spr" );
	m_iYellowSplash = PRECACHE_MODEL( "sprites/yellowsplash.spr" );
	m_iFlame = PRECACHE_MODEL( "sprites/fthrower.spr" );
	m_usChemicalSpray = PRECACHE_EVENT( 1, "events/ChemicalSpray.sc" );
}

void CChemGrenade::SplatterTouch( CBaseEntity *pOther )
{
	if ( UTIL_PointContents( pev->origin ) == CONTENTS_SKY )
	{
		UTIL_Remove( this );
		return;
	}

	TraceResult tr;
	Vector		vecSpot;// trace starts here!

	pev->enemy = pOther->edict();

	vecSpot = pev->origin - pev->velocity.Normalize() * 32;
	UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, ENT(pev), &tr );

	SetTouch( NULL );
	Splatter( &tr );
	UTIL_Remove( this );
}

void CChemGrenade::Splatter( TraceResult *pTrace )
{
	Vector vecSpot;
	TraceResult tr, Bloodtr;

	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible
	pev->takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if ( pTrace->flFraction != 1.0 )
	{
		vecSpot = pTrace->vecEndPos + (pTrace->vecPlaneNormal * (pev->dmg - 24) * 0.6);
	}
	else
	{
		vecSpot = pev->origin;
	}

	int iContents = UTIL_PointContents ( pev->origin );

	UTIL_TraceLine( pev->origin, pev->origin + pev->velocity * 10, dont_ignore_monsters, ENT(pev), &tr );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
		WRITE_BYTE( TE_SPRITE_SPRAY );
		WRITE_COORD( tr.vecEndPos.x );
		WRITE_COORD( tr.vecEndPos.y );
		WRITE_COORD( tr.vecEndPos.z );
		WRITE_COORD( tr.vecPlaneNormal.x );
		WRITE_COORD( tr.vecPlaneNormal.y );
		WRITE_COORD( tr.vecPlaneNormal.z );
		if ( ChemicalMixture.explosion == 1 )
			WRITE_SHORT( m_iGreenSplash );
		else if ( ChemicalMixture.explosion == 2 )
			WRITE_SHORT( m_iRedSplash );
		else
			WRITE_SHORT( m_iYellowSplash );
		WRITE_BYTE( 25 );			// count
		WRITE_BYTE( 50 );			// speed
		WRITE_BYTE( 200 );			// noise
	MESSAGE_END();

	if ( !g_pGameRules->IsMultiplayer() )
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_BLOODSPRITE );
			WRITE_COORD( vecSpot.x );
			WRITE_COORD( vecSpot.y );
			WRITE_COORD( vecSpot.z );
			WRITE_SHORT( g_sModelIndexBloodSpray );				// initial sprite model
			WRITE_SHORT( g_sModelIndexBloodDrop );				// droplet sprite models
			if ( ChemicalMixture.explosion == 1 )
				WRITE_BYTE( 53 );
			if ( ChemicalMixture.explosion == 2 )
				WRITE_BYTE( 247 );
			if ( ChemicalMixture.explosion == 5 )
				WRITE_BYTE( 195 );
			WRITE_BYTE( 16 );	// size
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_BLOODSPRITE );
			WRITE_COORD( vecSpot.x + 5 );
			WRITE_COORD( vecSpot.y + 5 );
			WRITE_COORD( vecSpot.z + 5 );
			WRITE_SHORT( g_sModelIndexBloodSpray );				// initial sprite model
			WRITE_SHORT( g_sModelIndexBloodDrop );				// droplet sprite models
			if ( ChemicalMixture.explosion == 1 )
				WRITE_BYTE( 53 );
			if ( ChemicalMixture.explosion == 2 )
				WRITE_BYTE( 247 );
			if ( ChemicalMixture.explosion == 5 )
				WRITE_BYTE( 195 );
			WRITE_BYTE( 12 );	// size
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_BLOODSPRITE );
			WRITE_COORD( vecSpot.x + 9 );
			WRITE_COORD( vecSpot.y + 9 );
			WRITE_COORD( vecSpot.z + 9 );
			WRITE_SHORT( g_sModelIndexBloodSpray );				// initial sprite model
			WRITE_SHORT( g_sModelIndexBloodDrop );				// droplet sprite models
			if ( ChemicalMixture.explosion == 1 )
				WRITE_BYTE( 53 );
			if ( ChemicalMixture.explosion == 2 )
				WRITE_BYTE( 247 );
			if ( ChemicalMixture.explosion == 5 )
				WRITE_BYTE( 195 );
			WRITE_BYTE( 30 );	// size
		MESSAGE_END();
	}

	// splash the walls around
	for ( int i = 0 ; i < 12 ; i++ )
	{
		Vector vecTraceDir = g_vecAttackDir;

		vecTraceDir.x += RANDOM_FLOAT( -2, 2 );
		vecTraceDir.y += RANDOM_FLOAT( -2, 2 );
		vecTraceDir.z += RANDOM_FLOAT( -2, 2 );

		UTIL_TraceLine( pTrace->vecEndPos, pTrace->vecEndPos + vecTraceDir * 20, ignore_monsters, ENT(pev), &Bloodtr );

		if ( Bloodtr.flFraction != 1.0 )
		{
			if ( ChemicalMixture.explosion == 2 )
				UTIL_DecalTrace( &Bloodtr, DECAL_CG_RED );
			else if ( ChemicalMixture.explosion == 1 )
				UTIL_DecalTrace( &Bloodtr, DECAL_CG_GREEN );
			else
				UTIL_DecalTrace( &Bloodtr, DECAL_CG_YELLOW );
		}

		UTIL_TraceLine( pTrace->vecEndPos, pTrace->vecEndPos + vecTraceDir * 40, ignore_monsters, ENT(pev), &Bloodtr );

		if ( Bloodtr.flFraction != 1.0 )
		{
			if ( ChemicalMixture.explosion == 2 )
				UTIL_DecalTrace( &Bloodtr, DECAL_CG_RED );
			else if ( ChemicalMixture.explosion == 1 )
				UTIL_DecalTrace( &Bloodtr, DECAL_CG_GREEN );
			else
				UTIL_DecalTrace( &Bloodtr, DECAL_CG_YELLOW );
		}
	}

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	ChemicalRadiusDamage( pev->origin, pev, pevOwner, pev->dmg, pev->dmg * 2.5, CLASS_NONE, ChemicalMixture.explosion );

	if ( ChemicalMixture.explosion == 2 )
		UTIL_DecalTrace( pTrace, DECAL_CG_RED );
	else if ( ChemicalMixture.explosion == 1 )
		UTIL_DecalTrace( pTrace, DECAL_CG_GREEN );
	else
		UTIL_DecalTrace( pTrace, DECAL_CG_YELLOW );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
		case 0:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/fizzy1.wav", 0.25, ATTN_NORM);	break;
		case 1:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/sprog1.wav", 0.25, ATTN_NORM);	break;
		case 2:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/sprog2.wav", 0.25, ATTN_NORM);	break;
	}

	pev->effects |= EF_NODRAW;
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 1.0;
	m_flTimeExploded = gpGlobals->time;
}

void CChemGrenade::ChemicalSmoke( void )
{
	SetThink( &CChemGrenade::ChemicalSmokeThink );
	pev->nextthink = gpGlobals->time;
}

void CChemGrenade::ChemicalSmokeThink( void )
{
	if ( gpGlobals->time - m_flTimeExploded > 10 )
	{
		if ( m_pSmokeSprite )
		{
			UTIL_Remove( m_pSmokeSprite );
			m_pSmokeSprite = NULL;
		}

		UTIL_Remove( this );
		return;
	}

	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE_SPRAY );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_COORD( 1 );
		WRITE_SHORT( g_sModelIndexGibSmoke );
		WRITE_BYTE( 3 );			// count
		WRITE_BYTE( 50 );			// speed
		WRITE_BYTE( 50 );			// noise
	MESSAGE_END();

	::RadiusDamage( pev->origin, pev, pevOwner, 2, pev->dmg, CLASS_NONE, DMG_RADIATION );

	pev->nextthink = gpGlobals->time + 0.2;
}

//=========================================================
// ShootSticky - a blob that sticks where it lands, and goes
// off when its time is up
//=========================================================
CChemGrenade *CChemGrenade::ShootSticky( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, chemicalmixture_t mixture )
{
	float time = mixture.stability / 10;

	CChemGrenade *pGrenade = GetClassPtr( (CChemGrenade *)NULL );
	pGrenade->Spawn();
	pGrenade->ChemicalMixture = mixture;
	UTIL_SetOrigin( pGrenade->pev, vecStart );
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles( pGrenade->pev->velocity );
	pGrenade->pev->owner = ENT(pevOwner);
	pGrenade->pev->avelocity.x = RANDOM_FLOAT( -100, -500 );
	pGrenade->pev->dmg = pGrenade->ChemicalMixture.volatility;
	pGrenade->pev->gravity = 0.5;

	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->SetThink( &CChemGrenade::TimedChemicalThink );
	pGrenade->pev->nextthink = gpGlobals->time + 0.1;
	if (time < 0.1)
	{
		pGrenade->pev->nextthink = gpGlobals->time;
		pGrenade->pev->velocity = Vector( 0, 0, 0 );
	}

	pGrenade->SetTouch( &CChemGrenade::StickyChemicalTouch );

	SET_MODEL(ENT(pGrenade->pev), "models/tubeball.mdl");
	pGrenade->pev->dmg = pGrenade->ChemicalMixture.volatility;

	switch ( mixture.explosion )
	{
	case 1:
		pGrenade->pev->skin = 0;
		break;
	case 2:
		pGrenade->pev->skin = 3;
		break;
	case 5:
		pGrenade->pev->skin = 1;
		break;
	default:
		pGrenade->pev->skin = 2;
		break;
	}

	return pGrenade;
}

void CChemGrenade::StickyChemicalTouch( CBaseEntity *pOther )
{
	if ( UTIL_PointContents( pev->origin ) == CONTENTS_SKY )
	{
		UTIL_Remove( this );
		return;
	}

	TraceResult	tr;

	UTIL_TraceLine ( pev->origin, pev->origin + pev->velocity * 32,  ignore_monsters, ENT(pev), & tr);

	m_bDontSpray = TRUE;

	if ( ChemicalMixture.explosion == 2 )
		UTIL_DecalTrace( &tr, DECAL_CG_RED );
	else if ( ChemicalMixture.explosion == 1 )
		UTIL_DecalTrace( &tr, DECAL_CG_GREEN );
	else if ( ChemicalMixture.explosion == 5 )
		UTIL_DecalTrace( &tr, DECAL_CG_YELLOW );
	else
		UTIL_DecalTrace( &tr, DECAL_CG_RED );

	EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/cgbounce.wav", 0.55, ATTN_NORM );

	pev->velocity = tr.vecPlaneNormal * -1;
	pev->angles = UTIL_VecToAngles ( pev->velocity );
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;

	if ( FClassnameIs( pOther->pev, "worldspawn" ) )
	{
		pev->movetype = MOVETYPE_NONE;
	}
	else
	{
		// stuck to something else: fall off it
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_TOSS;
	}

	pev->nextthink = gpGlobals->time;

	if ( ChemicalMixture.explosion != 3 && ChemicalMixture.explosion != 4 && ChemicalMixture.explosion != 5 )
	{
		SetThink( &CChemGrenade::ChemicalSmoke );
		pev->velocity = g_vecZero;
		pev->nextthink = gpGlobals->time + 0.1;
		m_flTimeExploded = gpGlobals->time;
		SetTouch( NULL );
	}
}

void CChemGrenade::BouncyChemicalTouch( CBaseEntity *pOther )
{
	if ( UTIL_PointContents( pev->origin ) == CONTENTS_SKY )
	{
		UTIL_Remove( this );
		return;
	}

	// don't hit the guy that launched this grenade
	if ( pOther->edict() == pev->owner )
		return;

	ChemicalMixture.stability -= pev->velocity.Length() / 15;

	// only do damage if we're moving fairly fast
	if (m_flNextAttack < gpGlobals->time && pev->velocity.Length() > 100)
	{
		entvars_t *pevOwner = VARS( pev->owner );
		if (pevOwner)
		{
			TraceResult tr = UTIL_GetGlobalTrace( );
			ClearMultiDamage( );
			pOther->TraceAttack(pevOwner, 1, gpGlobals->v_forward, &tr, DMG_CLUB );
			ApplyMultiDamage( pev, pevOwner);
		}
		m_flNextAttack = gpGlobals->time + 1.0; // debounce
	}

	Vector vecTestVelocity;

	vecTestVelocity = pev->velocity;
	vecTestVelocity.z *= 0.45;

	if ( ChemicalMixture.stability < 0 || ( !m_fRegisteredSound && vecTestVelocity.Length() <= 100 ) )
	{
		// it has come to rest
		CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin, 200, 0.3 );
		m_fRegisteredSound = TRUE;

		if ( ChemicalMixture.explosion == 3 || ChemicalMixture.explosion == 4 )
		{
			ChemicalExplode( );
		}
		else
		{
			TraceResult tr;
			Vector		vecSpot;// trace starts here!

			vecSpot = pev->origin - pev->velocity.Normalize() * 32;
			UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, ENT(pev), &tr );

			Splatter( &tr );
		}
		UTIL_Remove( this );
		return;
	}

	if (pev->flags & FL_ONGROUND)
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.8;

		pev->sequence = RANDOM_LONG( 1, 1 );
	}
	else
	{
		// the thicker, the less it bounces
		int iDamp = (30 - ChemicalMixture.viscosity) / 15;
		if ( iDamp < 1 )
			iDamp = 1;

		// sic: the * 1 folds away, but it leaves retail its extra Vector
		// temporary (built and copied out of line) before the division
		pev->velocity = pev->velocity * 1 / iDamp;

		EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/cgbounce.wav", 0.55, ATTN_NORM );
	}

	pev->framerate = pev->velocity.Length() / 200.0;
	if (pev->framerate > 1.0)
		pev->framerate = 1;
	else if (pev->framerate < 0.5)
		pev->framerate = 0;
}

void CChemGrenade::TimedChemicalThink( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	if ( !m_bDontSpray )
		PLAYBACK_EVENT_FULL( 0, edict(), m_usChemicalSpray, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, pev->skin, 0, 0, 0 );

	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime - 1 < gpGlobals->time)
	{
		CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 400, 0.1 );
	}

	if (pev->dmgtime <= gpGlobals->time)
	{
		if ( ChemicalMixture.explosion != 3 && ChemicalMixture.explosion != 4 )
		{
			TraceResult tr;
			Vector		vecSpot;// trace starts here!

			vecSpot = pev->origin - pev->velocity.Normalize() * 32;
			UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, ENT(pev), &tr );

			Splatter( &tr );
			UTIL_Remove( this );
			return;
		}

		ChemicalExplode( );
		UTIL_Remove( this );
		return;
	}

	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}
}

//=========================================================
// ShootBouncy - a blob that bounces until it settles or its
// time is up
//=========================================================
CChemGrenade *CChemGrenade::ShootBouncy( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, chemicalmixture_t mixture )
{
	CChemGrenade *pGrenade = GetClassPtr( (CChemGrenade *)NULL );
	pGrenade->Spawn();
	pGrenade->ChemicalMixture = mixture;

	int time = pGrenade->ChemicalMixture.stability / 10;

	pGrenade->pev->gravity = 0.5;
	UTIL_SetOrigin( pGrenade->pev, vecStart );
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles( pGrenade->pev->velocity );
	pGrenade->pev->owner = ENT(pevOwner);

	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->SetThink( &CChemGrenade::TimedChemicalThink );
	pGrenade->pev->nextthink = gpGlobals->time + 0.1;
	if (time < 0.1)
	{
		pGrenade->pev->nextthink = gpGlobals->time;
		pGrenade->pev->velocity = Vector( 0, 0, 0 );
	}

	pGrenade->pev->avelocity.x = RANDOM_FLOAT( -100, -500 );
	pGrenade->pev->dmg = pGrenade->ChemicalMixture.volatility;
	pGrenade->SetTouch( &CChemGrenade::BouncyChemicalTouch );

	SET_MODEL(ENT(pGrenade->pev), "models/tubeball.mdl");

	switch ( mixture.explosion )
	{
	case 1:
		pGrenade->pev->skin = 0;
		break;
	case 2:
		pGrenade->pev->skin = 3;
		break;
	case 5:
		pGrenade->pev->skin = 1;
		break;
	default:
		pGrenade->pev->skin = 2;
		break;
	}

	return pGrenade;
}

//=========================================================
// ShootSplatter - a blob that splatters where it hits
//=========================================================
CChemGrenade *CChemGrenade::ShootSplatter( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, chemicalmixture_t mixture )
{
	CChemGrenade *pGrenade = GetClassPtr( (CChemGrenade *)NULL );
	pGrenade->Spawn();
	pGrenade->ChemicalMixture = mixture;
	pGrenade->pev->gravity = 0.5;
	UTIL_SetOrigin( pGrenade->pev, vecStart );
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles( pGrenade->pev->velocity );
	pGrenade->pev->owner = ENT(pevOwner);

	pGrenade->SetThink( &CChemGrenade::ChemicalTrail );
	pGrenade->pev->nextthink = gpGlobals->time + 0.2;
	pGrenade->pev->avelocity.x = RANDOM_FLOAT( -100, -500 );
	pGrenade->SetTouch( &CChemGrenade::SplatterTouch );
	pGrenade->pev->dmg = pGrenade->ChemicalMixture.volatility;

	SET_MODEL(ENT(pGrenade->pev), "models/tubeball.mdl");

	switch ( mixture.explosion )
	{
	case 1:
		pGrenade->pev->skin = 0;
		break;
	case 2:
		pGrenade->pev->skin = 3;
		break;
	case 5:
		pGrenade->pev->skin = 1;
		break;
	default:
		pGrenade->pev->skin = 2;
		break;
	}

	return pGrenade;
}

void CChemGrenade::ChemicalExplode( void )
{
	TraceResult tr;
	Vector		vecSpot;// trace starts here!

	vecSpot = pev->origin + Vector ( 0 , 0 , 8 );
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -40 ),  ignore_monsters, ENT(pev), & tr);

	pev->dmg = ChemicalMixture.volatility;
	Explode( &tr, DMG_BLAST );

	if ( ChemicalMixture.explosion == 4 )
	{
		// both acid and base: burn by whichever there is more of
		entvars_t *pevOwner;
		if ( pev->owner )
			pevOwner = VARS( pev->owner );
		else
			pevOwner = NULL;

		pev->owner = NULL;

		ChemicalMixture.explosion = ( ChemicalMixture.base > ChemicalMixture.acid ) + 1;

		for ( int i = 0; i < 2; i++ )
		{
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
				WRITE_BYTE( TE_BLOODSPRITE );
				WRITE_COORD( pev->origin.x );
				WRITE_COORD( pev->origin.y );
				WRITE_COORD( pev->origin.z );
				WRITE_SHORT( g_sModelIndexBloodSpray );				// initial sprite model
				WRITE_SHORT( g_sModelIndexBloodDrop );				// droplet sprite models
				if ( ChemicalMixture.explosion == 1 )
					WRITE_BYTE( 53 );
				else
					WRITE_BYTE( 247 );
				WRITE_BYTE( 16 );	// size
			MESSAGE_END();
		}

		ChemicalRadiusDamage( pev->origin, pev, pevOwner, pev->dmg, 75, CLASS_NONE, ChemicalMixture.explosion );
	}
}

void CChemGrenade::ChemicalTrail( void )
{
	PLAYBACK_EVENT_FULL( FEV_RELIABLE, edict(), m_usChemicalSpray, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, pev->skin, 0, 0, 0 );
	DangerSoundThink( );
}

//=========================================================
// Explode - CGrenade's, with Gunman's explosion sprites
//=========================================================
void CChemGrenade::Explode( TraceResult *pTrace, int bitsDamageType )
{
	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible

	pev->takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if ( pTrace->flFraction != 1.0 )
	{
		pev->origin = pTrace->vecEndPos + (pTrace->vecPlaneNormal * (pev->dmg - 24) * 0.6);
	}

	int iContents = UTIL_PointContents ( pev->origin );

	int iScale = pev->dmg / 2;
	if ( iScale < 20 )
		iScale = 20;

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( pev->origin.x );	// Send to PAS because of the sound
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		if (iContents != CONTENTS_WATER)
		{
			WRITE_SHORT( RandomExplosionSprite() );
		}
		else
		{
			WRITE_SHORT( g_sModelIndexWExplosion );
		}
		WRITE_BYTE( iScale / 3  ); // scale * 10
		WRITE_BYTE( 20  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	RadiusDamage ( pev, pevOwner, pev->dmg, CLASS_NONE, bitsDamageType );

	if ( RANDOM_FLOAT( 0 , 1 ) < 0.5 )
	{
		UTIL_DecalTrace( pTrace, DECAL_SCORCH1 );
	}
	else
	{
		UTIL_DecalTrace( pTrace, DECAL_SCORCH2 );
	}

	switch ( RANDOM_LONG( 0, 2 ) )
	{
		case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris1.wav", 0.55, ATTN_NORM);	break;
		case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris2.wav", 0.55, ATTN_NORM);	break;
		case 2:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris3.wav", 0.55, ATTN_NORM);	break;
	}

	pev->effects |= EF_NODRAW;
	SetThink( &CGrenade::Smoke );
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 0.3;

	if (iContents != CONTENTS_WATER)
	{
		int sparkCount = RANDOM_LONG(0,3);
		for ( int i = 0; i < sparkCount; i++ )
			Create( "spark_shower", pev->origin, pTrace->vecPlaneNormal, NULL );
	}
}


//=========================================================
// sticky acid gibs
//=========================================================
void CGib :: SpawnStickyAcidGibs( entvars_t *pevVictim, Vector vecOrigin, int cGibs )
{
	int i;

	for ( i = 0 ; i < cGibs ; i++ )
	{
		CGib *pGib = GetClassPtr( (CGib *)NULL );

		pGib->Spawn( "models/stickygib.mdl" );
		pGib->pev->body = RANDOM_LONG(0,2);

		pGib->pev->origin.x = vecOrigin.x + RANDOM_FLOAT( -3, 3 );
		pGib->pev->origin.y = vecOrigin.y + RANDOM_FLOAT( -3, 3 );
		pGib->pev->origin.z = vecOrigin.z + RANDOM_FLOAT( -3, 3 );

		// make the gib fly away from the attack vector
		pGib->pev->velocity = g_vecAttackDir;

		// mix in some noise
		pGib->pev->velocity.x += RANDOM_FLOAT ( -100, 0.3 );
		pGib->pev->velocity.y += RANDOM_FLOAT ( -100, 0.3 );
		pGib->pev->velocity.z += RANDOM_FLOAT ( -100, 0.3 );

		pGib->pev->avelocity.x = RANDOM_FLOAT ( 250, 400 );
		pGib->pev->avelocity.y = RANDOM_FLOAT ( 250, 400 );

		pGib->m_bloodColor = 53;

		pGib->pev->movetype = MOVETYPE_TOSS;
		UTIL_SetSize ( pGib->pev, Vector ( 0, 0 ,0 ), Vector ( 0, 0, 0 ) );
		pGib->SetTouch ( &CGib::StickyAcidTouch );
		pGib->SetThink (NULL);

		pGib->LimitVelocity();
	}
}

//
// Sticky acid puts acid on the wall and stays put.
//
void CGib :: StickyAcidTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	TraceResult	tr;

	if ( UTIL_PointContents( pev->origin ) == CONTENTS_SKY )
	{
		UTIL_Remove( this );
		return;
	}

	SetThink ( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.5;

	if ( !FClassnameIs( pOther->pev, "worldspawn" ) )
	{
		pev->nextthink = gpGlobals->time;
		return;
	}

	UTIL_TraceLine ( pev->origin, pev->origin + pev->velocity * 32,  ignore_monsters, ENT(pev), & tr);

	UTIL_DecalTrace( &tr, DECAL_CG_RED );

	pev->velocity = tr.vecPlaneNormal * -1;
	pev->angles = UTIL_VecToAngles ( pev->velocity );
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	pev->movetype = MOVETYPE_NONE;
}


//=========================================================
// ChemicalRadiusDamage - RadiusDamage, with acid hurting
// people and base hurting aliens the most
//=========================================================
static void ChemicalRadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int iChemical )
{
	CBaseEntity *pEntity = NULL;
	TraceResult	tr;
	float		flAdjustedDamage, falloff;
	Vector		vecSpot;

	if ( flRadius )
		falloff = flDamage / flRadius;
	else
		falloff = 1.0;

	int bInWater = (UTIL_PointContents ( vecSrc ) == CONTENTS_WATER);

	vecSrc.z += 1;// in case grenade is lying on the ground

	if ( !pevAttacker )
		pevAttacker = pevInflictor;

	// iterate on all entities in the vicinity.
	while ((pEntity = UTIL_FindEntityInSphere( pEntity, vecSrc, flRadius )) != NULL)
	{
		if ( pEntity->pev->takedamage != DAMAGE_NO )
		{
			// UNDONE: this should check a damage mask, not an ignore
			if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
			{// houndeyes don't hurt other houndeyes with their attack
				continue;
			}

			// blast's don't tavel into or out of water
			if (bInWater && pEntity->pev->waterlevel == 0)
				continue;
			if (!bInWater && pEntity->pev->waterlevel == 3)
				continue;

			vecSpot = pEntity->BodyTarget( vecSrc );

			UTIL_TraceLine ( vecSrc, vecSpot, dont_ignore_monsters, ENT(pevInflictor), &tr );

			if ( tr.flFraction == 1.0 || tr.pHit == pEntity->edict() )
			{// the explosion can 'see' this entity, so hurt them!
				if (tr.fStartSolid)
				{
					// if we're stuck inside them, fixup the position and distance
					tr.vecEndPos = vecSrc;
					tr.flFraction = 0.0;
				}

				// decrease damage for an ent that's farther from the bomb.
				flAdjustedDamage = ( vecSrc - tr.vecEndPos ).Length() * falloff;
				flAdjustedDamage = flDamage - flAdjustedDamage;

				if ( flAdjustedDamage < 0 )
				{
					flAdjustedDamage = 0;
				}

				BOOL bAlien = FALSE;
				bool bHuman = false;
				int bitsDamageType;

				int iClass = pEntity->Classify();
				if ( iClass == CLASS_ALIEN_BEAK || iClass == CLASS_ALIEN_TUBE )
					bAlien = TRUE;
				if ( iClass == CLASS_PLAYER || iClass == CLASS_HUMAN_PASSIVE || iClass == CLASS_HUMAN_MILITARY )
					bHuman = true;

				switch ( iChemical )
				{
				case 1:
					// acid
					if ( !bHuman )
						flAdjustedDamage /= 2;
					bitsDamageType = DMG_ACID | DMG_NEVERGIB;
					break;
				case 2:
					// base
					if ( !bAlien )
						flAdjustedDamage /= 2;
					bitsDamageType = DMG_BURN | DMG_NEVERGIB;
					break;
				case 5:
					flAdjustedDamage /= 4;
					bitsDamageType = DMG_BLAST | DMG_NEVERGIB;
					break;
				default:
					flAdjustedDamage *= 0.8;
					bitsDamageType = DMG_BLAST;
					break;
				}

				if (tr.flFraction != 1.0)
				{
					ClearMultiDamage( );
					pEntity->TakeDamage ( pevInflictor, pevAttacker, flAdjustedDamage, bitsDamageType );
					ApplyMultiDamage( pevInflictor, pevAttacker );
				}
				else
				{
					pEntity->TakeDamage ( pevInflictor, pevAttacker, flAdjustedDamage, bitsDamageType );
				}
			}
		}
	}
}
