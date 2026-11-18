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
// the gauss pistol: single shots, rapid fire, a charged bolt,
// and with the sniper case a scoped, charging shot
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "effects.h"
#include "customentity.h"
#include "soundent.h"
#include "gamerules.h"
#include "decals.h"
#include "gaussshots.h"
#include "gausspistol.h"

extern int gmsgWeapPickup;
extern int gmsgSliderX;
extern int gmsgZoom;
extern int gEvilImpulse101;

enum gausspistol_e {
	GAUSSPISTOL_IDLE = 0,
	GAUSSPISTOL_IDLERESTLESS,
	GAUSSPISTOL_SINGLESHOT,
	GAUSSPISTOL_RAPIDFIRE,
	GAUSSPISTOL_CHARGEFIRE,
	GAUSSPISTOL_DRAW,
	GAUSSPISTOL_HOLSTER,
	GAUSSPISTOL_SNIPERIDLE,
	GAUSSPISTOL_SNIPERIDLERESTLESS,
	GAUSSPISTOL_SNIPERIDLEFIDGET,
	GAUSSPISTOL_SNIPERSHOOT,
	GAUSSPISTOL_SNIPERDRAW,
	GAUSSPISTOL_SNIPERHOLSTER
};

// the cust slider's fire types
#define GAUSS_FIRE_SINGLE	1
#define GAUSS_FIRE_BOLT		2
#define GAUSS_FIRE_RAPID	3
#define GAUSS_FIRE_SNIPER	4

// sprite indexes and the fire event (names ours)
int g_iGaussPoofIndex;
int g_iGaussFadeIndex;
unsigned short g_usGaussFire;
int g_iGaussBeamIndex;
int g_iGaussSparks;
int g_iGaussBigIndex;


//=========================================================
// CNullEnt - the sniper shot's beam, fading away
//=========================================================
class CNullEnt : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );

	static CNullEnt *CreateBeam( Vector vecOrigin, Vector vecEnd, CBaseEntity *pOwner, int fCharged );

	void EXPORT WaitThink( void );
	void EXPORT LazerThink( void );
	void EXPORT RemoveThink( void );

	Vector	m_vecEnd;
	int		m_iBrightness;
	int		m_fActive;
	int		m_fCharged;
	int		m_fHitWorld;
	CBaseEntity *m_pGlow;
	int		m_iUnused290;
};

LINK_ENTITY_TO_CLASS( entity_null, CNullEnt );

void CNullEnt::Spawn( void )
{
	Precache( );

	pev->takedamage = DAMAGE_NO;
	SET_MODEL( ENT(pev), "models/null.mdl" );
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ) );

	SetTouch( NULL );
	SetThink( &CNullEnt::WaitThink );
	pev->nextthink = gpGlobals->time + 0.1;

	m_iBrightness = 90;
}

void CNullEnt::Precache( void )
{
	PRECACHE_MODEL( "models/null.mdl" );
	PRECACHE_MODEL( "sprites/gaussbeam1.spr" );
	PRECACHE_MODEL( "sprites/gausspoof.spr" );
}

void CNullEnt::WaitThink( void )
{
	if ( m_fActive )
	{
		m_fHitWorld = 0;
		SetThink( &CNullEnt::LazerThink );
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

void CNullEnt::LazerThink( void )
{
	int iWidth = 4;

	if ( m_fCharged )
		iWidth = 8;

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, m_vecEnd );
		WRITE_BYTE( TE_BEAMENTPOINT );
		WRITE_SHORT( entindex() + 0x1000 );
		WRITE_COORD( m_vecEnd.x );
		WRITE_COORD( m_vecEnd.y );
		WRITE_COORD( m_vecEnd.z );
		WRITE_SHORT( g_iGaussBeamIndex );
		WRITE_BYTE( 0 ); // startframe
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 1 ); // life
		WRITE_BYTE( iWidth );  // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( m_iBrightness );	// brightness
		WRITE_BYTE( 0 );		// speed
	MESSAGE_END();

	if ( m_pGlow )
		m_pGlow->pev->renderamt = m_iBrightness;

	m_iBrightness -= 10;
	if ( m_iBrightness < 5 )
		SetThink( &CNullEnt::RemoveThink );

	pev->nextthink = gpGlobals->time + 0.1;
}

void CNullEnt::RemoveThink( void )
{
	if ( m_pGlow )
	{
		UTIL_Remove( m_pGlow );
		m_pGlow = NULL;
	}

	UTIL_Remove( this );
}

CNullEnt *CNullEnt::CreateBeam( Vector vecOrigin, Vector vecEnd, CBaseEntity *pOwner, int fCharged )
{
	CNullEnt *pBeam = GetClassPtr( (CNullEnt *)NULL );

	pBeam->Spawn();
	UTIL_SetOrigin( pBeam->pev, vecOrigin );
	pBeam->pev->owner = pOwner->edict();
	pBeam->m_vecEnd = vecEnd;
	pBeam->m_fCharged = fCharged;

	return pBeam;
}


//=========================================================
// CFastShot - the rapid fire's shot
//=========================================================
CFastShot *CFastShot::Shoot( Vector vecSrc, Vector vecAngles, CBaseEntity *pOwner, float flDamage )
{
	float x, y, z;

	// get circular gaussian spread
	do {
		x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		z = x*x+y*y;
	} while (z > 1);

	Vector vecDir = gpGlobals->v_forward + x * VECTOR_CONE_7DEGREES.x * gpGlobals->v_right + y * VECTOR_CONE_7DEGREES.y * gpGlobals->v_up;

	CFastShot *pShot = GetClassPtr( (CFastShot *)NULL );
	pShot->Spawn();
	pShot->pev->dmg = flDamage;
	UTIL_SetOrigin( pShot->pev, vecSrc + gpGlobals->v_right * 5 );
	pShot->pev->angles = vecAngles;
	UTIL_MakeVectors( vecAngles );
	pShot->pev->velocity = vecDir * 2000;
	pShot->pev->owner = pOwner->edict();

	return pShot;
}

void CFastShot::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING( "gauss_charged" );
	pev->solid = SOLID_BBOX;
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;

	SET_MODEL( ENT(pev), "sprites/gausswomp.spr" );

	SetTouch( &CFastShot::FastShotTouch );
	SetThink( &CFastShot::Animate );

	pev->frame = 0;
	pev->scale = 0.25;

	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	m_iMaxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 2 ); // life
		WRITE_BYTE( 2 );  // width
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();
}

void CFastShot::Animate( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->frame++ )
	{
		if ( pev->frame > m_iMaxFrame )
		{
			pev->frame = 0;
		}
	}
}

void CFastShot::FastShotTouch( CBaseEntity *pOther )
{
	if ( pOther && pOther->pev->takedamage )
	{
		TraceResult tr = UTIL_GetGlobalTrace( );
		entvars_t	*pevOwner;

		pevOwner = VARS( pev->owner );

		// UNDONE: this needs to call TraceAttack instead
		ClearMultiDamage( );

		pOther->TraceAttack(pevOwner, pev->dmg, pev->velocity.Normalize(), &tr, DMG_BULLET | DMG_NEVERGIB );

		ApplyMultiDamage( pev, pevOwner );

		pev->velocity = Vector( 0, 0, 0 );

		if ( pOther->IsPlayer() )
			EMIT_SOUND_DYN( pOther->edict(), CHAN_VOICE, "weapons/gauss_fire3.wav", 0.35, ATTN_NORM, 0, 70 );
	}

	PLAYBACK_EVENT_FULL( 0, edict(), m_usRapidImpact, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );

	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 600, 0.1 );

	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
	pev->effects |= EF_NODRAW;
}

void CFastShot::Precache( void )
{
	m_iTrail = PRECACHE_MODEL( "sprites/muzzleflash3.spr" );
	m_iGlow = PRECACHE_MODEL( "sprites/gaussglow2.spr" );
	PRECACHE_MODEL( "sprites/gausswomp.spr" );
	m_usRapidImpact = PRECACHE_EVENT( 1, "events/RapidImpact.sc" );
}


//=========================================================
// CBoltShot - the charged bolt
//=========================================================
CBoltShot *CBoltShot::Shoot( Vector vecSrc, Vector vecAngles, CBaseEntity *pOwner )
{
	CBoltShot *pShot = GetClassPtr( (CBoltShot *)NULL );
	pShot->Spawn();

	UTIL_SetOrigin( pShot->pev, vecSrc + gpGlobals->v_right * 5 );
	pShot->pev->angles = vecAngles;
	UTIL_MakeVectors( vecAngles );
	pShot->pev->velocity = gpGlobals->v_forward * 1450;
	pShot->pev->owner = pOwner->edict();

	return pShot;
}

void CBoltShot::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING( "gauss_bolt" );
	pev->solid = SOLID_BBOX;
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;

	SET_MODEL( ENT(pev), "sprites/gausswomp.spr" );

	SetTouch( &CBoltShot::BoltTouch );
	SetThink( &CBoltShot::ThrowSparks );

	pev->frame = 0;
	pev->scale = 1.0;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 4 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();

	pev->nextthink = gpGlobals->time + 0.1;

	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( -4, -4, -4 ), Vector( 4, 4, 4 ) );
}

void CBoltShot::ThrowSparks( void )
{
	if ( !g_pGameRules->IsMultiplayer() )
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SPRITETRAIL );// TE_RAILTRAIL);
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_COORD( pev->origin.x + gpGlobals->v_forward.x );
			WRITE_COORD( pev->origin.y + gpGlobals->v_forward.y );
			WRITE_COORD( pev->origin.z + gpGlobals->v_forward.z );
			WRITE_SHORT( g_iGaussSparks );		// model
			WRITE_BYTE( 3 );				// count
			WRITE_BYTE( 10 );				// life * 10
			WRITE_BYTE( RANDOM_LONG( 1, 2 ) );				// size * 10
			WRITE_BYTE( 10 );				// amplitude * 0.1
			WRITE_BYTE( 20 );				// speed * 100
		MESSAGE_END();
	}

	pev->frame++;
	if ( pev->frame > 7 )
		pev->frame = 0;

	pev->nextthink = gpGlobals->time + 0.1;
}

void CBoltShot::BoltTouch( CBaseEntity *pOther )
{
	if ( pOther->edict() == pev->owner || pev->owner == pOther->pev->owner )
		return;

	if ( pOther->pev->takedamage )
	{
		TraceResult tr = UTIL_GetGlobalTrace( );
		entvars_t	*pevOwner;

		pevOwner = VARS( pev->owner );

		ClearMultiDamage( );

		pOther->TraceAttack(pevOwner, gSkillData.plrDmgGaussSlow, pev->velocity.Normalize(), &tr, DMG_BULLET | DMG_NEVERGIB );

		ApplyMultiDamage( pev, pevOwner );

		pev->velocity = Vector( 0, 0, 0 );
	}
	else
	{
		RadiusDamage( pev, VARS( pev->owner ), gSkillData.plrDmgGaussSlow * 0.87, CLASS_NONE, DMG_NEVERGIB );
	}

	PLAYBACK_EVENT_FULL( 0, edict(), m_usChargedImpact, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );

	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1000, 2.0 );

	UTIL_Remove( this );
}

void CBoltShot::Precache( void )
{
	PRECACHE_MODEL( "sprites/gausswomp.spr" );
	PRECACHE_MODEL( "sprites/smoke.spr" );
	m_iTrail = PRECACHE_MODEL( "sprites/gaussbeam2.spr" );
	UTIL_PrecacheOther( "dmlRocket" );
	m_usChargedImpact = PRECACHE_EVENT( 1, "events/ChargedImpact.sc" );
}

void CBoltShot::CreateBeam( void )
{
	KillBeam( );

	m_pBeam = CBeam::BeamCreate( "sprites/gaussbeam2.spr", 20 );
	m_pBeam->PointEntInit( pev->origin, entindex() );
	m_pBeam->SetFlags( BEAM_FSINE );
	m_pBeam->SetEndAttachment( 1 );
	m_pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;
	m_pBeam->SetScrollRate( 0 );
	m_pBeam->SetNoise( 20 );
	m_pBeam->SetWidth( 20 );
}

void CBoltShot::KillBeam( void )
{
	if ( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}
}


//=========================================================
// CGaussGlow - a glow left where a shot hit
//=========================================================
CGaussGlow *CGaussGlow::CreateGlow( Vector vecOrigin, int iType )
{
	CGaussGlow *pGlow = GetClassPtr( (CGaussGlow *)NULL );
	pGlow->Spawn();
	pGlow->SetGlow( iType );
	UTIL_SetOrigin( pGlow->pev, vecOrigin );

	return pGlow;
}

void CGaussGlow::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING( "gauss_glow" );
	pev->solid = SOLID_NOT;
	pev->rendermode = kRenderGlow;
	pev->renderamt = 255;
	pev->renderfx = kRenderFxNoDissipation;

	SetThink( &CGaussGlow::AnimateGlow );

	pev->frame = 0;
	pev->scale = 0.5;

	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CGaussGlow::Precache( void )
{
	PRECACHE_MODEL( "sprites/gausssmall.spr" );
	PRECACHE_MODEL( "sprites/gaussbig.spr" );
}

void CGaussGlow::AnimateGlow( void )
{
	pev->frame++;
	if ( pev->frame > m_iMaxFrame )
		UTIL_Remove( this );

	pev->nextthink = gpGlobals->time + 0.15;
}

void CGaussGlow::SetGlow( int iType )
{
	if ( iType == 1 )
	{
		SET_MODEL( ENT(pev), "sprites/gausssmall.spr" );
		m_iMaxFrame = 7;
		return;
	}

	SET_MODEL( ENT(pev), "sprites/gaussbig.spr" );
	m_iMaxFrame = 12;
}


//=========================================================
// CGaussBall - a bouncing spark
//=========================================================
CGaussBall *CGaussBall::CreateBall( void )
{
	CGaussBall *pBall = GetClassPtr( (CGaussBall *)NULL );
	pBall->pev->classname = MAKE_STRING( "gauss_ball" );
	pBall->Spawn();

	return pBall;
}

void CGaussBall::Spawn( void )
{
	float flLife = RANDOM_FLOAT( 0.8, 5 );

	pev->dmg = 100;

	Precache( );

	pev->movetype = MOVETYPE_BOUNCE;
	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_BBOX;
	pev->gravity = 0.5;
	pev->friction = 0.6;
	pev->scale = 0.5;

	SET_MODEL( ENT(pev), "sprites/gausspark.spr" );
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	SetTouch( &CGaussBall::BallBounceTouch );
	SetThink( &CGaussBall::TumbleClusterThink );

	pev->dmgtime = gpGlobals->time + flLife;
	pev->nextthink = gpGlobals->time + 0.1;
	pev->framerate = 1.0;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 20 ); // life
		WRITE_BYTE( 3 );  // width
		WRITE_BYTE( 0 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 0 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();

	pev->rendermode = kRenderGlow;
	pev->renderamt = 255;
	pev->renderfx = kRenderFxNoDissipation;
}

void CGaussBall::Precache( void )
{
	PRECACHE_MODEL( "sprites/gausspark.spr" );
	m_iTrail = PRECACHE_MODEL( "sprites/smoke.spr" );
	m_sKaboom = PRECACHE_MODEL( "sprites/kaboom.spr" );
}

void CGaussBall::RemoveBall( void )
{
	pev->effects |= EF_NODRAW;
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 0.3;
	UTIL_Remove( this );
}

void CGaussBall::TumbleClusterThink( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime <= gpGlobals->time)
	{
		SetThink( &CGaussBall::RemoveBall );
	}
	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}

	if ( pev->frame++ )
	{
		if ( pev->frame > 7 )
		{
			pev->frame = 0;
		}
	}
}

void CGaussBall::BallBounceTouch( CBaseEntity *pOther )
{
	RemoveBall( );
}

LINK_ENTITY_TO_CLASS( gauss_charged, CFastShot );
LINK_ENTITY_TO_CLASS( gauss_glow, CGaussGlow );
LINK_ENTITY_TO_CLASS( gauss_ball, CGaussBall );
LINK_ENTITY_TO_CLASS( guass_bolt, CBoltShot );	// sic


//=========================================================
// the weapon
//=========================================================
LINK_ENTITY_TO_CLASS( weapon_pistol, CGaussPistol );
LINK_ENTITY_TO_CLASS( weapon_gausspistol, CGaussPistol );


#ifndef CLIENT_DLL
TYPEDESCRIPTION	CFastShot::m_SaveData[] =
{
	DEFINE_FIELD( CFastShot, m_iMaxFrame, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CFastShot, CGrenade );

TYPEDESCRIPTION	CBoltShot::m_SaveData[] =
{
	DEFINE_FIELD( CBoltShot, m_iMaxFrame, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CBoltShot, CGrenade );

TYPEDESCRIPTION	CGaussPistol::m_SaveData[] =
{
	DEFINE_FIELD( CGaussPistol, m_fInAttack, FIELD_INTEGER ),
	DEFINE_FIELD( CGaussPistol, m_flStartCharge, FIELD_TIME ),
	DEFINE_FIELD( CGaussPistol, m_flNextAmmoBurn, FIELD_TIME ),
	DEFINE_FIELD( CGaussPistol, m_iNormalShotCount, FIELD_INTEGER ),
	DEFINE_FIELD( CGaussPistol, m_iSlidePos, FIELD_INTEGER ),
	DEFINE_FIELD( CGaussPistol, m_iFireType, FIELD_INTEGER ),
	DEFINE_FIELD( CGaussPistol, m_flTimeGaussHolster, FIELD_TIME ),
	DEFINE_FIELD( CGaussPistol, m_flSniperTime, FIELD_TIME ),
	DEFINE_FIELD( CGaussPistol, m_fSniperAttack, FIELD_INTEGER ),
	DEFINE_FIELD( CGaussPistol, m_bCharged, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGaussPistol, m_iHaveSniperRifle, FIELD_INTEGER ),
	DEFINE_FIELD( CGaussPistol, m_iHolstered, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CGaussPistol, CBasePlayerWeapon );
#endif


float CGaussPistol::GetFullChargeTime( void )
{
	if ( g_pGameRules->IsMultiplayer() )
	{
		return 1.5;
	}

	return 4;
}

void CGaussPistol::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_gausspistol" );
	Precache( );
	m_iId = WEAPON_GAUSSPISTOL;
	SET_MODEL(ENT(pev), "models/w_gauss.mdl");

	m_iNormalShotCount = 0;
	m_iDefaultAmmo = 35;

	FallInit();// get ready to fall down.

	m_iSlidePos = GAUSS_FIRE_SINGLE;
	m_iHolstered = FALSE;
}


void CGaussPistol::Precache( void )
{
	g_iGaussBigIndex = PRECACHE_MODEL( "sprites/gaussbig.spr" );
	g_iGaussSparks = PRECACHE_MODEL( "sprites/gausspark.spr" );
	g_iGaussBeamIndex = PRECACHE_MODEL( "sprites/gaussbeam2.spr" );
	g_iGaussFadeIndex = PRECACHE_MODEL( "sprites/gaussfade.spr" );
	g_iGaussPoofIndex = PRECACHE_MODEL( "sprites/gausspoof.spr" );
	PRECACHE_MODEL( "sprites/gausspuff.spr" );
	PRECACHE_MODEL( "sprites/gaussbeam2.spr" );

	PRECACHE_MODEL("models/w_gauss.mdl");
	PRECACHE_MODEL("models/v_guasspistol.mdl");
	PRECACHE_MODEL("models/p_357.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");

	UTIL_PrecacheOther( "gauss_charged" );
	UTIL_PrecacheOther( "guass_bolt" );
	UTIL_PrecacheOther( "gauss_glow" );
	UTIL_PrecacheOther( "entity_null" );

	PRECACHE_SOUND("weapons/gauss_charge.wav");
	PRECACHE_SOUND("weapons/gauss_fire1.wav");
	PRECACHE_SOUND("weapons/gauss_fire2.wav");
	PRECACHE_SOUND("weapons/gauss_fire3.wav");
	PRECACHE_SOUND("weapons/gauss_fire4.wav");

	m_iBeam = PRECACHE_MODEL( "sprites/white.spr" );

	PRECACHE_SOUND("weapons/gsnipe_zoom.wav");
	PRECACHE_SOUND("weapons/sniperzoom.wav");
	PRECACHE_SOUND("weapons/sniperunzoom.wav");

	g_usGaussFire = PRECACHE_EVENT( 1, "events/gauss.sc" );
	m_usSniperShot = PRECACHE_EVENT( 1, "events/snipershot.sc" );
}

int CGaussPistol::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		if ( !gEvilImpulse101 )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
				WRITE_BYTE( m_iId );
			MESSAGE_END();
		}

		pPlayer->GiveNamedItem( "cust_1GaussPistolFireType" );

		MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
			WRITE_BYTE( WEAPON_CUST_FIRETYPE );
			WRITE_BYTE( m_iSlidePos );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

int CGaussPistol::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = gSkillData.gaussMaxAmmo;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 1;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_GAUSSPISTOL;
	p->iFlags = 0;
	p->iWeight = -1;
	p->pszCustomizes = NULL;
	p->iCustomSettings = 0;
	p->iIsCustomization = 0;
	p->iCustomDefault = 0;

	return 1;
}

void CGaussPistol::Customize( char cOption, int iValue )
{
	if ( iValue == GAUSS_FIRE_SNIPER && !m_iHaveSniperRifle )
	{
		// the sniper fire type needs the sniper case
		CBasePlayerItem *pItem = m_pPlayer->m_rgpPlayerItems[0];
		while ( pItem )
		{
			if ( FClassnameIs( pItem->pev, "cust_2GaussPistolSniper" ) )
			{
				m_iHaveSniperRifle = TRUE;
				break;
			}
			pItem = pItem->m_pNext;
		}

		if ( !m_iHaveSniperRifle )
			return;
	}

	m_iFireType = iValue;
	m_iSlidePos = iValue;
}

#ifndef CLIENT_DLL
void CGaussPistol::UpdateOwner( void )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgSliderX, NULL, m_pPlayer->pev );
		WRITE_BYTE( WEAPON_CUST_FIRETYPE );
		WRITE_BYTE( m_iSlidePos );
	MESSAGE_END();
}
#endif

BOOL CGaussPistol::Deploy( )
{
	m_iHolstered = FALSE;

	if ( pev->body )
		return DefaultDeploy( "models/v_guasspistol.mdl", "models/p_357.mdl", GAUSSPISTOL_SNIPERDRAW, "python" );

	return DefaultDeploy( "models/v_guasspistol.mdl", "models/p_357.mdl", GAUSSPISTOL_DRAW, "python" );
}

void CGaussPistol::Holster( int skiplocal /* = 0 */ )
{
	m_iHolstered = TRUE;
	m_pPlayer->m_flNextAttack = gpGlobals->time + 2.0;
	m_fInAttack = 0;

	EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM );

	MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	m_fSniperAttack = FALSE;
	m_bCharged = FALSE;
	m_flSniperTime = 0;

	CBasePlayerWeapon::Holster( skiplocal );
}

void CGaussPistol::InventoryFrame( void )
{
}

void CGaussPistol::SecondaryAttack( void )
{
	if ( m_iFireType == GAUSS_FIRE_SNIPER )
	{
		// cancel the sniper shot
		m_fSniperAttack = FALSE;
		m_pPlayer->m_flNextAttack = gpGlobals->time + 2.0;

		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		m_bCharged = FALSE;
		m_flSniperTime = 0;

		EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM );
	}
}

void CGaussPistol::PrimaryAttack( void )
{
	if ( m_iHolstered )
		return;

	// the sniper case has to come on or off first
	if ( ( m_iFireType == GAUSS_FIRE_SNIPER && pev->body == 0 ) || ( m_iFireType != GAUSS_FIRE_SNIPER && pev->body == 1 ) )
	{
		WeaponIdle( );
		return;
	}

	if ( m_iFireType == GAUSS_FIRE_SNIPER )
	{
		ChargeSniper( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, NORMAL_GUN_VOLUME, 0.3 );

	if ( m_iFireType == GAUSS_FIRE_RAPID )
	{
		FireRapid( );
		return;
	}

	if ( m_iFireType == GAUSS_FIRE_BOLT )
	{
		FireBolt( );
		return;
	}

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 1 )
	{
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

	FireSingle( );

	m_fInAttack = 0;
	m_flTimeWeaponIdle = gpGlobals->time + 1.0;
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.15;
	if ( g_pGameRules->IsMultiplayer() )
		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.1;

	if ( ++m_iNormalShotCount >= 3 )
	{
		// three shots, then a pause
		m_iNormalShotCount = 0;
		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.45;
		if ( g_pGameRules->IsMultiplayer() )
			m_pPlayer->m_flNextAttack = gpGlobals->time + 0.25;
	}

	m_pPlayer->pev->punchangle.x -= 1;
}

//=========================================================
// FireRapid - one shot of the rapid fire
//=========================================================
void CGaussPistol::FireRapid( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 0 )
	{
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

	m_flNextPrimaryAttack = gpGlobals->time + 0.1;
	m_flTimeWeaponIdle = gpGlobals->time + 1.0;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	CFastShot::Shoot( vecSrc, m_pPlayer->pev->v_angle, m_pPlayer, gSkillData.plrDmgGaussFast );

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	SendWeaponAnim( GAUSSPISTOL_RAPIDFIRE );

	EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gauss_fire1.wav", 1, ATTN_NORM );
}

//=========================================================
// FireBolt - the charged bolt, for ten units of ammo
//=========================================================
void CGaussPistol::FireBolt( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 10 )
	{
		PlayEmptySound( );
		return;
	}

	SendWeaponAnim( GAUSSPISTOL_CHARGEFIRE );

	m_flNextPrimaryAttack = gpGlobals->time + 1.5;
	m_flTimeWeaponIdle = gpGlobals->time + 2.0;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecAiming = gpGlobals->v_forward;	// sic: unused
	Vector vecSrc = m_pPlayer->GetGunPosition( );

	CBoltShot::Shoot( vecSrc, m_pPlayer->pev->v_angle, m_pPlayer );

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 10;

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->pev->punchangle.x -= 10;

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gauss_fire2.wav", 1, ATTN_NORM );
	EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/gauss_charge.wav", 1, ATTN_NORM );
}

//=========================================================
// ChargeSniper - while the attack button is held
//=========================================================
void CGaussPistol::ChargeSniper( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 20 )
	{
		PlayEmptySound( );
		return;
	}

	m_fSniperAttack = TRUE;

	if ( m_flSniperTime == 0 )
	{
		m_flSniperTime = gpGlobals->time;
		EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gsnipe_zoom.wav", 1, ATTN_NORM );

		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_SHORT( 1 );
		MESSAGE_END();
	}

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.01;

	if ( m_pPlayer->pev->button & IN_JUMP )
	{
		// cancelled
		m_fSniperAttack = FALSE;
		m_pPlayer->m_flNextAttack = gpGlobals->time + 3.0;

		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		m_bCharged = FALSE;
		m_flSniperTime = 0;

		EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM );
		return;
	}

	if ( gpGlobals->time - m_flSniperTime > 0.5 && m_flSniperTime != 0 )
		m_bCharged = TRUE;
}

//=========================================================
// FireSniper - the sniper shot, charged or not
//=========================================================
void CGaussPistol::FireSniper( void )
{
	TraceResult tr;
	float flDamage;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecDest = vecSrc + gpGlobals->v_forward * 8192;
	edict_t *pentIgnore = ENT( m_pPlayer->pev );

	SendWeaponAnim( GAUSSPISTOL_SNIPERSHOOT );
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	if ( m_bCharged )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_SHORT( 3 );
		MESSAGE_END();

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 20;

		flDamage = (gpGlobals->time - m_flSniperTime) * 100;
		if ( flDamage > 133 )
			flDamage = 133;

		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.8;
		m_flSniperTime = gpGlobals->time + 0.5;

		EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/sniperzoom.wav", 1, ATTN_NORM );
	}
	else
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 8;

		EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/sniperunzoom.wav", 1, ATTN_NORM );

		flDamage = 40;
		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.65;
	}

	UTIL_TraceLine( vecSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr );

	if ( tr.fAllSolid )
		return;

	CSoundEnt::InsertSound( bits_SOUND_COMBAT, tr.vecEndPos, NORMAL_GUN_VOLUME, 0.4 );

	CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
	if ( pEntity == NULL )
		return;

	if ( pEntity->pev->takedamage )
	{
		ClearMultiDamage();
		pEntity->TraceAttack( m_pPlayer->pev, flDamage, gpGlobals->v_forward, &tr, DMG_BULLET | DMG_NEVERGIB );
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );
	}
	else
	{
		UTIL_DecalTrace( &tr, DECAL_SMALLSCORCH1 );

		if ( g_pGameRules->IsMultiplayer() )
		{
			entvars_t *pevOwner = m_pPlayer->pev;
			entvars_t *pevInflictor = pev;
			::RadiusDamage( tr.vecEndPos, pevInflictor, pevOwner, flDamage * 0.8, (flDamage * 0.7) * 2, CLASS_NONE, DMG_ENERGYBEAM | DMG_NEVERGIB );
		}
	}

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	PLAYBACK_EVENT_FULL( 0, ENT(m_pPlayer->pev), m_usSniperShot, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 1, 0, m_bCharged, 0 );

	CNullEnt *pBeam = CNullEnt::CreateBeam( m_pPlayer->GetGunPosition( ) + gpGlobals->v_right * 1 + gpGlobals->v_forward * 11 + gpGlobals->v_up * -1, tr.vecEndPos, m_pPlayer, m_bCharged );
	pBeam->m_fActive = TRUE;
	if ( pEntity->pev->takedamage == DAMAGE_NO )
		pBeam->m_fHitWorld = TRUE;

	m_bCharged = FALSE;
}

//=========================================================
// GaussShot - the single shot's trace
//=========================================================
static void GaussShot( Vector vecOrigSrc, Vector vecDir, float flDamage, CBasePlayer *pPlayer )
{
	Vector vecSrc = vecOrigSrc;
	Vector vecDest = vecSrc + vecDir * 8192;
	edict_t		*pentIgnore;
	TraceResult tr, beam_tr;	// sic: beam_tr is never used

	pentIgnore = ENT( pPlayer->pev );

	UTIL_TraceLine( vecSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr );

	if ( tr.fAllSolid )
		return;

	CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

	if ( pEntity == NULL )
		return;

	Vector vecMuzzle = vecSrc + gpGlobals->v_up * -8 + gpGlobals->v_right * 3;

	if ( pEntity->pev->takedamage )
	{
		ClearMultiDamage();
		pEntity->TraceAttack( pPlayer->pev, flDamage, vecDir, &tr, DMG_ENERGYBEAM );
		ApplyMultiDamage( pPlayer->pev, pPlayer->pev );
	}

	vecSrc = tr.vecEndPos + vecDir;
}

void CGaussPistol::FireSingle( void )
{
	if ( m_fInAttack == 3 )
		return;

	PLAYBACK_EVENT_FULL( 0, ENT(m_pPlayer->pev), g_usGaussFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 1, 0, 0, 0 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( );

	GaussShot( vecSrc, vecAiming, gSkillData.plrDmgGaussShot, m_pPlayer );

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
}

void CGaussPistol::WeaponIdle( void )
{
	if ( !m_fFirstIdle )
	{
		if ( pev->body )
			SendWeaponAnim( GAUSSPISTOL_SNIPERIDLE );
		else
			SendWeaponAnim( GAUSSPISTOL_IDLE );

		m_fFirstIdle = TRUE;
	}

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_fSniperAttack )
	{
		FireSniper( );
		m_fSniperAttack = FALSE;
	}

	m_flSniperTime = 0;
	ResetEmptySound( );
	m_iNormalShotCount = 0;

	if ( m_iFireType == GAUSS_FIRE_SNIPER && pev->body == 0 )
	{
		// put the sniper case on
		m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0;
		m_flTimeWeaponIdle = gpGlobals->time + 2.0;

		if ( m_flTimeGaussHolster == 0 )
		{
			SendWeaponAnim( GAUSSPISTOL_HOLSTER );
			m_flTimeGaussHolster = gpGlobals->time + 0.25;
		}
		else if ( gpGlobals->time > m_flTimeGaussHolster )
		{
			m_flTimeGaussHolster = 0;
			pev->body = 1;
			SendWeaponAnim( GAUSSPISTOL_SNIPERDRAW );
			m_flTimeWeaponIdle = gpGlobals->time;
		}
		return;
	}

	if ( m_iFireType != GAUSS_FIRE_SNIPER && pev->body == 1 )
	{
		// take the sniper case off
		m_pPlayer->m_flNextAttack = gpGlobals->time + 2.0;
		m_flTimeWeaponIdle = gpGlobals->time + 1.5;

		if ( m_flTimeGaussHolster == 0 )
		{
			SendWeaponAnim( GAUSSPISTOL_SNIPERHOLSTER );
			m_flTimeGaussHolster = gpGlobals->time + 0.25;
		}
		else if ( gpGlobals->time > m_flTimeGaussHolster )
		{
			m_flTimeGaussHolster = 0;
			pev->body = 0;
			SendWeaponAnim( GAUSSPISTOL_DRAW );
			m_flTimeWeaponIdle = gpGlobals->time + 0.25;
			m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0;
		}
		return;
	}

	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_fInAttack )
	{
		FireSingle( );
		m_fInAttack = 0;
		m_flTimeWeaponIdle = gpGlobals->time + 2.0;
		return;
	}

	if ( UTIL_PercentChance( 96 ) )
	{
		if ( UTIL_PercentChance( 75 ) )
		{
			if ( m_iFireType != GAUSS_FIRE_SNIPER )
				SendWeaponAnim( GAUSSPISTOL_IDLE );
			else
				SendWeaponAnim( GAUSSPISTOL_SNIPERIDLE );

			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 3, 10 );
		}
		else
		{
			if ( m_iFireType != GAUSS_FIRE_SNIPER )
				SendWeaponAnim( GAUSSPISTOL_IDLERESTLESS );
			else
				SendWeaponAnim( GAUSSPISTOL_SNIPERIDLERESTLESS );

			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 3, 10 );
		}
	}
	else
	{
		if ( m_iFireType != GAUSS_FIRE_SNIPER )
			SendWeaponAnim( GAUSSPISTOL_IDLE );
		else
			SendWeaponAnim( GAUSSPISTOL_SNIPERIDLEFIDGET );

		m_flTimeWeaponIdle = gpGlobals->time + 3;
	}
}


class CGaussAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL(ENT(pev), "models/guassammo.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/guassammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if (pOther->GiveAmmo( gSkillData.gaussClipAmmo, "uranium", gSkillData.gaussMaxAmmo ) != -1)
		{
			if ( PlayPickupSound() )
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_gaussclip, CGaussAmmo );


//=========================================================
// the gauss pistol's customization items
//=========================================================
class CCust1GaussPistolFireType : public CBaseCustomization
{
public:
	void Spawn( void );
	int GetItemInfo(ItemInfo *p);
};
LINK_ENTITY_TO_CLASS( cust_1GaussPistolFireType, CCust1GaussPistolFireType );

void CCust1GaussPistolFireType::Spawn( )
{
	Precache( );
	Precache( );
	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
	m_iId = WEAPON_CUST_FIRETYPE;
}

int CCust1GaussPistolFireType::GetItemInfo(ItemInfo *p)
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
	p->iId = WEAPON_CUST_FIRETYPE;
	p->iPosition = 13;
	p->iCustomSettings = 4;
	p->iCustomDefault = 1;
	p->pszCustomizes = "weapon_gausspistol";

	return 1;
}


class CCust2GaussPistolSniper : public CBaseCustomization
{
public:
	void Spawn( void );
	void Precache( void );
	int AddToPlayer( CBasePlayer *pPlayer );
	int GetItemInfo(ItemInfo *p);
};
LINK_ENTITY_TO_CLASS( cust_2GaussPistolSniper, CCust2GaussPistolSniper );
LINK_ENTITY_TO_CLASS( customization_sniper, CCust2GaussPistolSniper );

void CCust2GaussPistolSniper::Spawn( )
{
	pev->classname = MAKE_STRING( "cust_2GaussPistolSniper" );
	Precache( );
	Precache( );
	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
	m_iId = WEAPON_CUST_SNIPER;

	SET_MODEL(ENT(pev), "models/w_snipercase.mdl");
}

int CCust2GaussPistolSniper::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();

		// tell whatever owns the case it has been taken
		CBaseEntity *pOwner = CBaseEntity::Instance( pev->owner );
		if ( pOwner )
			pOwner->DeathNotice( pev );

		return TRUE;
	}
	return FALSE;
}

void CCust2GaussPistolSniper::Precache( void )
{
	PRECACHE_MODEL( "models/w_snipercase.mdl" );
	PRECACHE_MODEL( "models/w_crowbar.mdl" );
}

int CCust2GaussPistolSniper::GetItemInfo(ItemInfo *p)
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
	p->iId = WEAPON_CUST_SNIPER;
	p->iPosition = 19;
	p->iCustomSettings = 2;
	p->iCustomDefault = 1;
	p->iIsCustomization = 1;
	p->pszCustomizes = "weapon_gausspistol";

	return 1;
}
