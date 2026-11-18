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
// monster_gunner: the big rust robot with a rocket launcher
// on each arm and a rack of homing rockets on its back.
// monster_rustgunr and monster_rustgnr make the same robot;
// monster_gunner_friendly is the AI gunner of the end
// chapter, who fights the end boss on the player's side.
// anime_rocket is the arms' rocket, animebackpack_rocket
// the back rack's.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"animation.h"
#include	"weapons.h"
#include	"effects.h"
#include	"soundent.h"
#include	"decals.h"
#include	"sphereexplosion.h"
#include	"rust.h"

extern DLL_GLOBAL int g_iSkillLevel;

//=========================================================
// anime_rocket: the rocket from the gunner's arms. It flies
// off with some spread and homes in on the gunner's enemy,
// turning at most 20 units a think and gaining speed up to
// 1000.
//=========================================================
class CAnimeRocket : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void ) { return CLASS_HUMAN_MILITARY; }
	void EXPORT IgniteThink( void );
	void EXPORT AccelerateThink( void );
	void EXPORT RocketTouch( CBaseEntity *pOther );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iTrail;
	CSprite *m_pFlare;		// not saved
};

LINK_ENTITY_TO_CLASS( anime_rocket, CAnimeRocket );

TYPEDESCRIPTION	CAnimeRocket::m_SaveData[] =
{
	DEFINE_FIELD( CAnimeRocket, m_iTrail, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CAnimeRocket, CGrenade );

void CAnimeRocket::Precache( void )
{
	PRECACHE_MODEL( "models/rocket.mdl" );
	PRECACHE_MODEL( "sprites/flame.spr" );
	m_iTrail = PRECACHE_MODEL( "sprites/firebeam.spr" );
	PRECACHE_SOUND( "weapons/rocket1.wav" );
	PRECACHE_SOUND( "weapons/kaboom1.wav" );
	PRECACHE_SOUND( "weapons/kaboom2.wav" );
	PRECACHE_SOUND( "weapons/kaboom3.wav" );
}

void CAnimeRocket::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT(pev), "models/rocket.mdl" );
	UTIL_SetSize( pev, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ) );
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CAnimeRocket::IgniteThink );
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.4, 0.6 );
	SetTouch( &CAnimeRocket::RocketTouch );

	// aim ahead of the owner, with some spread
	Vector vecForward, vecRight, vecUp;
	UTIL_MakeVectorsPrivate( pev->owner->v.angles, vecForward, vecRight, vecUp );

	Vector vecTarget = pev->origin + 64 * vecForward +
		( RANDOM_FLOAT( 0, 30 ) - 15 ) * vecRight + ( RANDOM_FLOAT( 0, 40 ) - 20 ) * vecUp;
	pev->velocity = ( vecTarget - pev->origin ).Normalize() * 500;
	pev->angles = UTIL_VecToAngles( pev->velocity );
	pev->dmg = gSkillData.rustgunnerDmgRocket;

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 15 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();

	m_pFlare = NULL;
}

void CAnimeRocket::IgniteThink( void )
{
	m_pFlare = CSprite::SpriteCreate( "sprites/flame.spr", pev->origin, FALSE );
	m_pFlare->SetScale( 1.0 );
	m_pFlare->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
	m_pFlare->pev->spawnflags |= SF_SPRITE_TEMPORARY;
	m_pFlare->SetAttachment( edict(), 0 );

	SetThink( &CAnimeRocket::AccelerateThink );
	pev->nextthink = gpGlobals->time;

	pev->velocity = pev->velocity.Normalize() * 500;
	pev->avelocity.z = 200;
}

void CAnimeRocket::AccelerateThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0.3, 0.5, 0, PITCH_NORM );

	// check world boundaries
	if ( pev->origin.x >= -4096 && pev->origin.x <= 4096 && pev->origin.y >= -4096 && pev->origin.y <= 4096 && pev->origin.z >= -4096 && pev->origin.z <= 4096 )
	{
		Vector vecTarget;

		if ( m_hEnemy != NULL )
		{
			if ( m_hEnemy->IsPlayer() )
				vecTarget = m_hEnemy->pev->origin;
			else
				vecTarget = m_hEnemy->BodyTarget( pev->origin );
		}
		else
		{
			vecTarget = pev->velocity.Normalize() * 1024;	// sic: a direction, not a spot ahead of the rocket
		}

		float flSpeed = pev->velocity.Length();

		// turn toward the target, at most 20 units a think
		Vector vecDir = vecTarget - pev->origin;
		vecDir = vecDir.Normalize() * 128;
		Vector vecCurrent = pev->velocity.Normalize() * 128;
		Vector vecTurn = vecDir - vecCurrent;
		if ( vecTurn.Length() > 20 )
			vecTurn = vecTurn.Normalize() * 20;

		pev->velocity = vecTurn + vecCurrent;
		pev->velocity = pev->velocity.Normalize() * flSpeed;
		pev->angles = UTIL_VecToAngles( pev->velocity );

		if ( flSpeed + 50 < 1000 )
			flSpeed += 50;
		else
			flSpeed = 1000;
		pev->velocity = pev->velocity.Normalize() * flSpeed;
	}
	else
	{
		UTIL_Remove( this );
	}
}

void CAnimeRocket::RocketTouch( CBaseEntity *pOther )
{
	if ( pOther->edict() == pev->owner )
		return;

	if ( m_pFlare )
	{
		UTIL_Remove( m_pFlare );
		m_pFlare = NULL;
	}

	STOP_SOUND( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav" );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/kaboom1.wav", 1, ATTN_NORM );	break;
	case 1:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/kaboom2.wav", 1, ATTN_NORM );	break;
	case 2:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/kaboom3.wav", 1, ATTN_NORM );	break;
	}

	// as CGrenade's ExplodeTouch and Explode
	TraceResult tr;
	Vector vecSpot = pev->origin - pev->velocity.Normalize() * 32;
	UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, ENT(pev), &tr );

	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible
	pev->takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if ( tr.flFraction != 1.0 )
	{
		pev->origin = tr.vecEndPos + (tr.vecPlaneNormal * (pev->dmg - 24) * 0.6);
	}

	int iContents = UTIL_PointContents ( pev->origin );

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
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
		WRITE_BYTE( pev->dmg * .60  ); // scale * 10
		WRITE_BYTE( 15  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END();

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner = VARS( pev->owner );

	::RadiusDamage( pev->origin, pev, pevOwner, pev->dmg, 60, CLASS_NONE, DMG_BLAST );

	if ( RANDOM_FLOAT( 0 , 1 ) < 0.5 )
	{
		UTIL_DecalTrace( &tr, DECAL_SCORCH1 );
	}
	else
	{
		UTIL_DecalTrace( &tr, DECAL_SCORCH2 );
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
			Create( "spark_shower", pev->origin, tr.vecPlaneNormal, NULL );
	}
}


//=========================================================
// animebackpack_rocket: the rockets from the gunner's back.
// They follow the enemy for the first half second to second
// and a half, then fly on to its last known spot, turning at
// most 25 units a think and gaining speed up to 1200.
//=========================================================
class CBackpackAnimeRocket : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void ) { return CLASS_HUMAN_MILITARY; }
	void EXPORT IgniteThink( void );
	void EXPORT AccelerateThink( void );
	void EXPORT RocketTouch( CBaseEntity *pOther );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iTrail;
	CSprite *m_pFlare;		// not saved
};

LINK_ENTITY_TO_CLASS( animebackpack_rocket, CBackpackAnimeRocket );

TYPEDESCRIPTION	CBackpackAnimeRocket::m_SaveData[] =
{
	DEFINE_FIELD( CBackpackAnimeRocket, m_iTrail, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CBackpackAnimeRocket, CGrenade );

void CBackpackAnimeRocket::Precache( void )
{
	PRECACHE_MODEL( "models/dmlrocket.mdl" );
	PRECACHE_MODEL( "sprites/flame.spr" );
	m_iTrail = PRECACHE_MODEL( "sprites/firebeam.spr" );
	PRECACHE_SOUND( "weapons/rocket1.wav" );
	PRECACHE_SOUND( "weapons/kaboom1.wav" );
	PRECACHE_SOUND( "weapons/kaboom2.wav" );
	PRECACHE_SOUND( "weapons/kaboom3.wav" );
}

void CBackpackAnimeRocket::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT(pev), "models/dmlrocket.mdl" );
	UTIL_SetSize( pev, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ) );
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CBackpackAnimeRocket::IgniteThink );
	pev->nextthink = gpGlobals->time + 0.2;
	SetTouch( &CBackpackAnimeRocket::RocketTouch );

	// aim ahead of the owner, with some spread
	Vector vecForward, vecRight, vecUp;
	UTIL_MakeVectorsPrivate( pev->owner->v.angles, vecForward, vecRight, vecUp );

	Vector vecTarget = pev->origin + 64 * vecForward +
		( RANDOM_FLOAT( 0, 30 ) - 15 ) * vecRight + ( RANDOM_FLOAT( 0, 40 ) - 20 ) * vecUp;
	pev->velocity = ( vecTarget - pev->origin ).Normalize() * 500;
	pev->angles = UTIL_VecToAngles( pev->velocity );
	pev->dmg = gSkillData.rustgunnerDmgMiniRocket;

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 6 ); // life
		WRITE_BYTE( 4 );  // width
		WRITE_BYTE( 200 );   // r, g, b
		WRITE_BYTE( 80 );   // r, g, b
		WRITE_BYTE( 0 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();

	m_pFlare = NULL;

	// follow the enemy until then
	m_flHungryTime = gpGlobals->time + RANDOM_FLOAT( 0.5, 1.4 );
}

void CBackpackAnimeRocket::IgniteThink( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0.3, 0.5, 0, PITCH_NORM );

	if ( m_hEnemy )
	{
		if ( m_hEnemy->IsPlayer() )
			m_vecEnemyLKP = m_hEnemy->pev->origin;
		else
			m_vecEnemyLKP = m_hEnemy->BodyTarget( pev->origin );
	}

	AccelerateThink( );
}

void CBackpackAnimeRocket::AccelerateThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
	SetThink( &CBackpackAnimeRocket::AccelerateThink );

	// check world boundaries
	if ( pev->origin.x >= -4096 && pev->origin.x <= 4096 && pev->origin.y >= -4096 && pev->origin.y <= 4096 && pev->origin.z >= -4096 && pev->origin.z <= 4096 )
	{
		if ( m_hEnemy != NULL && m_flHungryTime > gpGlobals->time )
		{
			if ( m_hEnemy->IsPlayer() )
				m_vecEnemyLKP = m_hEnemy->pev->origin;
			else
				m_vecEnemyLKP = m_hEnemy->BodyTarget( pev->origin );
		}

		float flSpeed = pev->velocity.Length();

		// turn toward the target, at most 25 units a think
		Vector vecDir = m_vecEnemyLKP - pev->origin;
		vecDir = vecDir.Normalize() * 128;
		Vector vecCurrent;
		vecCurrent = pev->velocity.Normalize() * 128;
		Vector vecTurn = vecDir - vecCurrent;
		if ( vecTurn.Length() > 25 )
			vecTurn = vecTurn.Normalize() * 25;

		pev->velocity = vecTurn + vecCurrent;
		pev->angles = UTIL_VecToAngles( pev->velocity );

		if ( flSpeed + 50 < 1200 )
			flSpeed += 50;
		else
			flSpeed = 1200;
		pev->velocity = pev->velocity.Normalize() * flSpeed;
	}
	else
	{
		UTIL_Remove( this );
	}
}

void CBackpackAnimeRocket::RocketTouch( CBaseEntity *pOther )
{
	if ( pOther->edict() == pev->owner )
		return;

	if ( m_pFlare )
	{
		UTIL_Remove( m_pFlare );
		m_pFlare = NULL;
	}

	STOP_SOUND( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav" );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/kaboom1.wav", 0.5, ATTN_NORM );	break;
	case 1:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/kaboom2.wav", 0.5, ATTN_NORM );	break;
	case 2:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/kaboom3.wav", 0.5, ATTN_NORM );	break;
	}

	// as CGrenade's ExplodeTouch and Explode
	TraceResult tr;
	Vector vecSpot = pev->origin - pev->velocity.Normalize() * 32;
	UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, ENT(pev), &tr );

	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible
	pev->takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if ( tr.flFraction != 1.0 )
	{
		pev->origin = tr.vecEndPos + (tr.vecPlaneNormal * (pev->dmg - 24) * 0.6);
	}

	int iContents = UTIL_PointContents ( pev->origin );

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
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
		WRITE_BYTE( 8 ); // scale * 10
		WRITE_BYTE( 15  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END();

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner = VARS( pev->owner );

	::RadiusDamage( pev->origin, pev, pevOwner, pev->dmg, 40, CLASS_NONE, DMG_BLAST );

	if ( RANDOM_FLOAT( 0 , 1 ) < 0.5 )
	{
		UTIL_DecalTrace( &tr, DECAL_SCORCH1 );
	}
	else
	{
		UTIL_DecalTrace( &tr, DECAL_SCORCH2 );
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
			Create( "spark_shower", pev->origin, tr.vecPlaneNormal, NULL );
	}
}


//=========================================================
// monster-specific DEFINE's
//=========================================================
#define	GUNNER_AE_SWIPE			1
#define	GUNNER_AE_ROCKET_RIGHT	6
#define	GUNNER_AE_ROCKET_RIGHT2	7
#define	GUNNER_AE_ROCKET_LEFT	8
#define	GUNNER_AE_ROCKET_LEFT2	9
#define	GUNNER_AE_KICK			12
#define	GUNNER_AE_STEP			1008

#define	GUNNER_ARM_RIGHT		1
#define	GUNNER_ARM_LEFT			2
#define	GUNNER_ARM_BACK			3

//=========================================================
// the gunner
//=========================================================
class CGunner : public CBaseRust
{
public:
	CGunner( void ) { m_iNeverRun = 0; }

	void Spawn( void );
	void Precache( void );
	void PrecacheSounds( void );
	int  Classify( void ) { return CLASS_ROBOT_GUNNER; }
	int  MonsterType( void ) { return 14; }
	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin - Vector( 48, 48, 0 );
		pev->absmax = pev->origin + Vector( 48, 48, 150 );
	}
	float HearingSensitivity( void ) { return 2.0; }
	BOOL CheckRangeAttack2( float flDot, float flDist ) { return FALSE; }
	void SetYawSpeed( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void StartTask( Task_t *pTask );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void PreRemove( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );
	void RunAI( void );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckMeleeAttack2( float flDot, float flDist );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	void SetActivity( Activity NewActivity );
	int  IgnoreConditions( void );
	void PrescheduleThink( void );
	Schedule_t *GetSchedule( void );

	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void RangeAttackSound( void );
	void MeleeAttackSound( void );
	void DeathSound( void );

	virtual void StepSound( void );
	virtual CBackpackAnimeRocket *ShootBackpackRocket( Vector vecSrc );

	void EyeOn( int level );
	void EyeOff( void );
	void EyeUpdate( void );

	CGunnerMortar *ShootMortar( Vector vecVelocity );
	CAnimeRocket *ShootRocket( Vector vecSrc );
	Vector GetArmPosition( int iArm );

	void EXPORT ToggleTakedamage( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	static const char *pGunnerSounds[];

	int m_eyeBrightness;
	int m_fHackPitch;			// the voice pitch, an int
	float m_fNextRangeAttack;
	CSprite *m_pEyeGlow;
	int m_iArm;					// the arm to fire next; not saved
	float m_iNeverRun;			// a float; set nowhere but the constructor, read nowhere
	float m_fMeleeDistance;		// no arm rockets nearer than this
	float m_fTimeSinceLastVolly;
	int m_iVollyCount;			// back rockets left in this volley
};

LINK_ENTITY_TO_CLASS( monster_gunner, CGunner );
LINK_ENTITY_TO_CLASS( monster_rustgunr, CGunner );
LINK_ENTITY_TO_CLASS( monster_rustgnr, CGunner );

TYPEDESCRIPTION	CGunner::m_SaveData[] =
{
	DEFINE_FIELD( CGunner, m_pEyeGlow, FIELD_CLASSPTR ),
	DEFINE_FIELD( CGunner, m_eyeBrightness, FIELD_INTEGER ),
	DEFINE_FIELD( CGunner, m_fHackPitch, FIELD_INTEGER ),
	DEFINE_FIELD( CGunner, m_fNextRangeAttack, FIELD_TIME ),
	DEFINE_FIELD( CGunner, m_iNeverRun, FIELD_TIME ),
	DEFINE_FIELD( CGunner, m_fMeleeDistance, FIELD_TIME ),		// sic: a distance
	DEFINE_FIELD( CGunner, m_fTimeSinceLastVolly, FIELD_TIME ),
	DEFINE_FIELD( CGunner, m_iVollyCount, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CGunner, CBaseRust );

const char *CGunner::pGunnerSounds[] =
{
	"gunner/gunner_alert1.wav",
	"gunner/gunner_alert2.wav",
	"gunner/gunner_alert3.wav",
	"gunner/gunner_dieforward.wav",
	"gunner/gunner_dieforward2.wav",
	"gunner/gunner_firegunright.wav",
	"gunner/gunner_flinch.wav",
	"gunner/gunner_idle.wav",
	"gunner/gunner_idle2.wav",
	"gunner/gunner_run_step.wav",
	"gunner/gunner_run_step1.wav",
	"gunner/gunner_run_step2.wav",
	"gunner/gunner_swing.wav",
	"renesaur/ren_bite3.wav",
};

void CGunner::PrecacheSounds( void )
{
	for ( int i = 0; i < ARRAYSIZE( pGunnerSounds ); i++ )
		PRECACHE_SOUND( (char *)pGunnerSounds[i] );
}

void CGunner::PainSound( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gunner/gunner_flinch.wav", 1, ATTN_NORM, 0, m_fHackPitch );
}

void CGunner::AlertSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gunner/gunner_alert1.wav", 1, ATTN_NORM, 0, m_fHackPitch );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gunner/gunner_alert2.wav", 1, ATTN_NORM, 0, m_fHackPitch );
		break;
	case 2:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gunner/gunner_alert3.wav", 1, ATTN_NORM, 0, m_fHackPitch );
		break;
	}
}

void CGunner::IdleSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "gunner/gunner_idle.wav", 1, ATTN_NORM, 8, PITCH_NORM );
		break;
	case 1:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "gunner/gunner_idle2.wav", 1, ATTN_NORM, 8, PITCH_NORM );
		break;
	}
}

void CGunner::RangeAttackSound( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "gunner/gunner_firegunright.wav", 0.7, ATTN_NORM, 0, PITCH_NORM );
}

void CGunner::MeleeAttackSound( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "gunner/gunner_swing.wav", 1, ATTN_NORM, 0, PITCH_NORM );
}

void CGunner::DeathSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "gunner/gunner_dieforward.wav", 1, ATTN_NORM, 0, m_fHackPitch );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "gunner/gunner_dieforward2.wav", 1, ATTN_NORM, 0, m_fHackPitch );
		break;
	}
}

//=========================================================
// StepSound - a heavy footstep that shakes the ground
//=========================================================
void CGunner::StepSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "gunner/gunner_run_step.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		break;
	case 1:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "gunner/gunner_run_step1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		break;
	case 2:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "gunner/gunner_run_step2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		break;
	}

	UTIL_ScreenShake( pev->origin, 25, 150, 0.75, 2000 );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CGunner::SetYawSpeed( void )
{
	int ys = 120;

	switch ( m_Activity )
	{
	case ACT_IDLE:
		ys = 120;
		break;
	case ACT_WALK:
	case ACT_RUN:
		ys = 120;
		break;
	case ACT_MELEE_ATTACK2:
		ys = 120;
		break;
	case ACT_RANGE_ATTACK1:
	case ACT_RANGE_ATTACK2:
		ys = 50;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 180;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CGunner::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case GUNNER_AE_ROCKET_RIGHT:
	case GUNNER_AE_ROCKET_RIGHT2:
		{
			Vector vecSrc = GetArmPosition( GUNNER_ARM_RIGHT );
			ShootRocket( vecSrc );
		}
		break;

	case GUNNER_AE_SWIPE:
		{
			CBaseEntity *pHurt = CheckEnemyMeleeHit( 150, 100, DMG_SLASH, 0, 0 );
			if ( pHurt )
			{
				pHurt->pev->punchangle.z = -15;
				pHurt->pev->punchangle.x = -45;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * -300;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_up * 300;

				CBaseMonster *pMonster = pHurt->MyMonsterPointer();
				if ( pMonster )
				{
					TraceResult tr;
					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					pMonster->TraceBleed( 100, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

					if ( pMonster->IsPlayer() )
						EMIT_SOUND_DYN( pMonster->edict(), CHAN_VOICE, "renesaur/ren_bite3.wav", 1, ATTN_IDLE, 0, 130 );
				}
			}
		}
		break;

	case GUNNER_AE_ROCKET_LEFT:
	case GUNNER_AE_ROCKET_LEFT2:
		{
			Vector vecSrc = GetArmPosition( GUNNER_ARM_LEFT );
			ShootRocket( vecSrc );
		}
		break;

	case GUNNER_AE_STEP:
		StepSound( );
		break;

	case GUNNER_AE_KICK:
		{
			pev->effects |= EF_MUZZLEFLASH;

			Vector vecForward;
			UTIL_MakeVectorsPrivate( pev->angles, vecForward, NULL, NULL );
			vecForward = vecForward * 100;		// sic: never used
			Vector vecArm = GetArmPosition( GUNNER_ARM_RIGHT );	// sic: never used

			CBaseEntity *pHurt = CheckEnemyMeleeHit( 200, 30, DMG_SLASH, 0, 0 );
			if ( pHurt )
			{
				pHurt->pev->punchangle.z = -15;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 400;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_up * 300;

				CBaseMonster *pMonster = pHurt->MyMonsterPointer();
				if ( pMonster )
				{
					TraceResult tr;
					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					pMonster->TraceBleed( 30, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

					if ( pMonster->IsPlayer() )
						EMIT_SOUND_DYN( pMonster->edict(), CHAN_VOICE, "renesaur/ren_bite3.wav", 1, ATTN_IDLE, 0, 140 );
				}
			}
		}
		break;

	default:
		CBaseRust::HandleAnimEvent( pEvent );
		break;
	}
}

void CGunner::StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	if ( pTask->iTask == TASK_MELEE_ATTACK1 || pTask->iTask == TASK_MELEE_ATTACK2 )
		MeleeAttackSound( );

	CBaseRust::StartTask( pTask );
}

void CGunner::EyeOn( int level )
{
	m_eyeBrightness = level;
}

void CGunner::EyeOff( void )
{
	m_eyeBrightness = 0;
}

// as the gargantua's
void CGunner::EyeUpdate( void )
{
	if ( m_pEyeGlow )
	{
		m_pEyeGlow->pev->renderamt = UTIL_Approach( m_eyeBrightness, m_pEyeGlow->pev->renderamt, 26 );
		if ( m_pEyeGlow->pev->renderamt == 0 )
			m_pEyeGlow->pev->effects |= EF_NODRAW;
		else
			m_pEyeGlow->pev->effects &= ~EF_NODRAW;
		UTIL_SetOrigin( m_pEyeGlow->pev, pev->origin );
	}
}

int CGunner::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// not hurt by its own rockets
	if ( pevInflictor->owner == edict() )
		return 0;

	if ( flDamage > 30 )
		flDamage = 30;

	if ( bitsDamageType & ( DMG_SHOCK | DMG_ENERGYBEAM ) )
		flDamage /= 2;

	return CBaseRust::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_ALWAYSGIB );
}

void CGunner::PreRemove( void )
{
	if ( m_pEyeGlow )
	{
		EyeOff( );
		UTIL_Remove( m_pEyeGlow );
		m_pEyeGlow = NULL;
	}

	CBaseRust::PreRemove( );
}

void CGunner::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( m_pEyeGlow )
	{
		EyeOff( );
		UTIL_Remove( m_pEyeGlow );
		m_pEyeGlow = NULL;
	}

	CBaseRust::Killed( pevAttacker, iGib );
}

//=========================================================
// GibMonster - the arms may fire one last time, four back
// rockets tumble out, and the gunner blows apart
//=========================================================
void CGunner::GibMonster( void )
{
	if ( UTIL_PercentChance( 40 ) )
	{
		Vector vecSrc = GetArmPosition( GUNNER_ARM_RIGHT );
		ShootRocket( vecSrc );
	}
	if ( UTIL_PercentChance( 40 ) )
	{
		Vector vecSrc = GetArmPosition( GUNNER_ARM_LEFT );
		ShootRocket( vecSrc );
	}

	for ( int i = 0; i < 4; i++ )
	{
		Vector vecSrc = pev->origin + Vector( RANDOM_FLOAT( -32, 32 ), RANDOM_FLOAT( -32, 32 ), RANDOM_FLOAT( 10, 60 ) );
		CAnimeRocket *pRocket = ShootRocket( vecSrc );
		Vector vecDir = Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( 0, 1 ) );
		vecDir.Normalize();		// sic: the result is thrown away
		pRocket->pev->angles = UTIL_VecToAngles( vecDir );
		UTIL_MakeAimVectors( pRocket->pev->angles );
		pRocket->pev->velocity = gpGlobals->v_forward * 300;
		pRocket->pev->movetype = MOVETYPE_TOSS;
		pRocket->pev->dmg = 60;
		pRocket->SetTouch( NULL );
		pRocket->SetThink( &CGrenade::Detonate );
		pRocket->pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0, 2 );
	}

	CBaseMonster *pGib;

	pGib = SpawnGib( "models/gunnergibs.mdl", 3, 0, 1 );
	if ( pGib )
		UTIL_SetOrigin( pGib->pev, pev->origin + Vector( 0, 0, 50 ) );

	pGib = SpawnGib( "models/gunnergibs.mdl", 4, 1, 1 );
	if ( pGib )
		UTIL_SetOrigin( pGib->pev, pev->origin + gpGlobals->v_right * 20 + gpGlobals->v_up * 50 );

	pGib = SpawnGib( "models/gunnergibs.mdl", 4, 1, 1 );
	if ( pGib )
		UTIL_SetOrigin( pGib->pev, pev->origin + gpGlobals->v_right * -20 + gpGlobals->v_up * 50 );

	pGib = SpawnGib( "models/gunnergibs.mdl", 4, 2, 1 );
	if ( pGib )
		UTIL_SetOrigin( pGib->pev, pev->origin + gpGlobals->v_right * -20 );

	pGib = SpawnGib( "models/gunnergibs.mdl", 4, 2, 1 );
	if ( pGib )
		UTIL_SetOrigin( pGib->pev, pev->origin + gpGlobals->v_right * 20 );

	Vector vecSpot = GetArmPosition( GUNNER_ARM_BACK );

	CSphereExplosion *pSphere = CSphereExplosion::CreateSphereExplosion( vecSpot, this, 400, 8, 255, 15, 1, TRUE, FALSE, 2.5, 1.0, 0.1 );
	CSphereExplosion *pSphere2 = CSphereExplosion::CreateSphereExplosion( vecSpot, this, 100, 2, 220, 10, 1, FALSE, FALSE, 0, 0, 0 );

	if ( pSphere )
		pSphere->UseToggle( this, this, USE_ON, 0 );
	if ( pSphere2 )
		pSphere2->UseToggle( this, this, USE_ON, 0 );

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( RandomExplosionSprite() );
		WRITE_BYTE( 40 );	// scale * 10
		WRITE_BYTE( 15 );	// framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );
		WRITE_SHORT( RandomExplosionSprite() );
		WRITE_BYTE( 60 );	// scale * 10
		WRITE_BYTE( 15 );	// framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	CBaseRust::GibMonster( );
}

//=========================================================
// RunAI - on the move or standing with an enemy about, the
// gunner fires its back rockets: four in a volley, then a
// four-second pause
//=========================================================
void CGunner::RunAI( void )
{
	pev->framerate = 1.5;
	m_fMeleeDistance *= 0.995;

	CBaseRust::RunAI( );

	if ( ( m_Activity == ACT_RUN || m_Activity == ACT_WALK || m_Activity == ACT_IDLE ) && m_hEnemy != NULL )
	{
		if ( m_iVollyCount != 4 || gpGlobals->time - m_fTimeSinceLastVolly > 4 )
		{
			m_fTimeSinceLastVolly = gpGlobals->time;
			ShootBackpackRocket( GetArmPosition( GUNNER_ARM_BACK ) );
		}
	}
}

//=========================================================
// ShootMortar - nothing calls it
//=========================================================
CGunnerMortar *CGunner::ShootMortar( Vector vecVelocity )
{
	UTIL_MakeAimVectors( pev->angles );

	CGunnerMortar *pMortar = CGunnerMortar::MortarCreate( );
	Vector vecSrc = GetArmPosition( GUNNER_ARM_BACK );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecSrc.x );
		WRITE_COORD( vecSrc.y );
		WRITE_COORD( vecSrc.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 20 ); // scale * 10
		WRITE_BYTE( 12 ); // framerate
	MESSAGE_END();

	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "gunner/gunner_boom1.wav", 1, ATTN_NORM, 0, PITCH_NORM );

	UTIL_SetOrigin( pMortar->pev, vecSrc );
	pMortar->pev->angles = pev->angles;
	pMortar->pev->velocity = vecVelocity;
	pMortar->pev->gravity = 2.0;
	pMortar->pev->owner = ENT(pev);

	return pMortar;
}

//=========================================================
// ShootRocket - an arm rocket, from near the given spot
//=========================================================
CAnimeRocket *CGunner::ShootRocket( Vector vecSrc )
{
	RangeAttackSound( );
	UTIL_MakeAimVectors( pev->angles );

	switch ( RANDOM_LONG( 0, 4 ) % 5 )	// one time in five, from the spot itself
	{
	case 0:
		vecSrc = vecSrc + gpGlobals->v_right * 10;
		break;
	case 1:
		vecSrc = vecSrc - gpGlobals->v_right * 10;
		break;
	case 2:
		vecSrc = vecSrc + gpGlobals->v_up * 10;
		break;
	case 3:
		vecSrc = vecSrc - gpGlobals->v_up * 10;
		break;
	}

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecSrc.x );
		WRITE_COORD( vecSrc.y );
		WRITE_COORD( vecSrc.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 20 ); // scale * 10
		WRITE_BYTE( 12 ); // framerate
	MESSAGE_END();

	CAnimeRocket *pRocket = GetClassPtr( (CAnimeRocket *)NULL );
	pRocket->pev->owner = ENT(pev);
	if ( m_hEnemy )
		pRocket->m_hEnemy = m_hEnemy;
	else
		pRocket->m_hEnemy = NULL;
	pRocket->Spawn( );
	UTIL_SetOrigin( pRocket->pev, vecSrc );

	return pRocket;
}

//=========================================================
// ShootBackpackRocket - one rocket of the back volley; the
// four of a volley start out to either side
//=========================================================
CBackpackAnimeRocket *CGunner::ShootBackpackRocket( Vector vecSrc )
{
	RangeAttackSound( );
	UTIL_MakeAimVectors( pev->angles );

	if ( --m_iVollyCount <= 0 )
		m_iVollyCount = 4;

	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/kaboom1.wav", 0.7, ATTN_NORM, 0, PITCH_NORM );

	Vector vecVelocity;

	switch ( m_iVollyCount % 5 )	// sic: the count runs 4 to 1, so case 0 never happens
	{
	case 0:
		vecSrc = vecSrc + gpGlobals->v_right * 15;
		vecVelocity = gpGlobals->v_up * 80 + gpGlobals->v_right * 75 - gpGlobals->v_forward * 200;
		break;
	case 1:
		vecSrc = vecSrc + gpGlobals->v_right * 30;
		vecVelocity = gpGlobals->v_up * 50 + gpGlobals->v_right * 150 - gpGlobals->v_forward * 200;
		break;
	case 2:
		vecSrc = vecSrc - gpGlobals->v_right * 15;
		vecVelocity = gpGlobals->v_up * 80 - gpGlobals->v_right * 75 - gpGlobals->v_forward * 200;
		break;
	case 3:
		vecSrc = vecSrc - gpGlobals->v_right * 30;
		vecVelocity = gpGlobals->v_up * 50 - gpGlobals->v_right * 150 - gpGlobals->v_forward * 200;
		break;
	case 4:
		vecVelocity = gpGlobals->v_up * 100 - gpGlobals->v_forward * 200;
		break;
	}

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecSrc.x );
		WRITE_COORD( vecSrc.y );
		WRITE_COORD( vecSrc.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 10 ); // scale * 10
		WRITE_BYTE( 16 ); // framerate
	MESSAGE_END();

	CBackpackAnimeRocket *pRocket = GetClassPtr( (CBackpackAnimeRocket *)NULL );
	pRocket->pev->owner = ENT(pev);
	if ( m_hEnemy )
		pRocket->m_hEnemy = m_hEnemy;
	else
		pRocket->m_hEnemy = NULL;
	pRocket->Spawn( );
	UTIL_SetOrigin( pRocket->pev, vecSrc );
	pRocket->pev->velocity = pRocket->pev->velocity + vecVelocity;
	pRocket->pev->angles = UTIL_VecToAngles( pRocket->pev->velocity );

	return pRocket;
}

//=========================================================
// GetArmPosition - the right or left arm's launcher, or the
// back rack
//=========================================================
Vector CGunner::GetArmPosition( int iArm )
{
	Vector vecPos, vecAng;

	switch ( iArm )
	{
	case 0:
		GetAttachment( 0, vecPos, vecAng );
		break;
	case GUNNER_ARM_RIGHT:
		GetAttachment( 2, vecPos, vecAng );
		break;
	case GUNNER_ARM_LEFT:
		GetAttachment( 3, vecPos, vecAng );
		break;
	case GUNNER_ARM_BACK:
		GetAttachment( 4, vecPos, vecAng );
		vecPos = vecPos + Vector( 0, 0, 96 );
		break;
	default:
		ALERT( at_console, "No gunner arm specified correctly\n" );
		break;
	}

	return vecPos;
}

BOOL CGunner::CheckMeleeAttack1( float flDot, float flDist )
{
	if ( flDist <= 100 && flDot >= 0.5 )
		return TRUE;
	return FALSE;
}

BOOL CGunner::CheckMeleeAttack2( float flDot, float flDist )
{
	if ( flDist <= 100 && flDot >= 0.5 )
		return TRUE;
	return FALSE;
}

//=========================================================
// CheckRangeAttack1 - an arm rocket, from an arm that has a
// clear shot at the enemy
//=========================================================
BOOL CGunner::CheckRangeAttack1( float flDot, float flDist )
{
	if ( flDist > m_fMeleeDistance && flDot > 0.7 && m_fNextRangeAttack <= gpGlobals->time )
	{
		TraceResult tr;
		BOOL bRight = FALSE;
		BOOL bLeft;

		if ( UTIL_PointContents( GetArmPosition( GUNNER_ARM_RIGHT ) ) != CONTENTS_SOLID )
		{
			UTIL_TraceLine( GetArmPosition( GUNNER_ARM_RIGHT ), m_hEnemy->BodyTarget( GetArmPosition( GUNNER_ARM_RIGHT ) ), ignore_monsters, ignore_glass, ENT(pev), &tr );
			if ( tr.flFraction == 1.0 || tr.pHit == m_hEnemy->edict() )
				bRight = TRUE;
		}

		bLeft = FALSE;
		if ( UTIL_PointContents( GetArmPosition( GUNNER_ARM_LEFT ) ) != CONTENTS_SOLID )
		{
			UTIL_TraceLine( GetArmPosition( GUNNER_ARM_LEFT ), m_hEnemy->BodyTarget( GetArmPosition( GUNNER_ARM_LEFT ) ), ignore_monsters, ignore_glass, ENT(pev), &tr );
			if ( tr.flFraction == 1.0 || tr.pHit == m_hEnemy->edict() )
				bLeft = TRUE;
		}

		if ( !bRight && !bLeft )
			return FALSE;

		if ( bRight && bLeft )
		{
			if ( RANDOM_LONG( 0, 1 ) )
				m_iArm = GUNNER_ARM_RIGHT;
			else
				m_iArm = GUNNER_ARM_LEFT;
		}
		else if ( bRight )
			m_iArm = GUNNER_ARM_RIGHT;
		else
			m_iArm = GUNNER_ARM_LEFT;

		m_fMeleeDistance = 500;
		m_fNextRangeAttack = gpGlobals->time + RANDOM_FLOAT( 3, 5 );
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// SetActivity - the arm rocket's sequence by the arm; far
// from its enemy, the gunner walks a quarter of the time
//=========================================================
void CGunner::SetActivity( Activity NewActivity )
{
	int iSequence;
	Vector vecDist;

	switch ( NewActivity )
	{
	case ACT_RUN:
		vecDist = m_vecEnemyLKP - pev->origin;
		if ( (int)vecDist.Length() > 700 && UTIL_PercentChance( 25 ) )
			iSequence = LookupActivity( ACT_WALK );
		else
			iSequence = LookupActivity( ACT_RUN );
		break;
	case ACT_RANGE_ATTACK1:
		if ( m_iArm == GUNNER_ARM_LEFT )
			iSequence = LookupSequence( "firerocketsleft" );
		else
			iSequence = LookupSequence( "firerocketright" );
		break;
	default:
		iSequence = LookupActivity( NewActivity );
		break;
	}

	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		ALERT ( at_console, "%s has no sequence for act:%d\n", STRING(pev->classname), NewActivity );
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}

int CGunner::IgnoreConditions( void )
{
	if ( m_afMemory & bits_MEMORY_FLINCHED )
		return bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE;

	return CBaseRust::IgnoreConditions( );
}

void CGunner::ToggleTakedamage( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->takedamage != DAMAGE_NO )
		pev->takedamage = DAMAGE_NO;
	else
		pev->takedamage = DAMAGE_AIM;
}

// sic: sets no movetype, blood colour, field of view or monster state
void CGunner::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT(pev), "models/gunner.mdl" );
	UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 150 ) );

	pev->solid = SOLID_SLIDEBOX;
	pev->health = gSkillData.rustgunnerHealth;
	m_fHackPitch = RANDOM_LONG( 0, 15 ) + 75;
	m_fTimeSinceLastVolly = m_fNextRangeAttack = gpGlobals->time;

	m_pEyeGlow = CSprite::SpriteCreate( "sprites/gunr_eye.spr", pev->origin, FALSE );
	m_pEyeGlow->SetTransparency( kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation );
	m_pEyeGlow->SetAttachment( edict(), 1 );
	EyeOff( );

	RustInit( );
	MonsterInit( );

	pev->view_ofs = Vector( 0, 0, 115 );
	m_iVollyCount = 4;
	m_flDistTooFar = 4096;

	SetUse( &CGunner::ToggleTakedamage );
}

void CGunner::Precache( void )
{
	PRECACHE_MODEL( "sprites/gunr_eye.spr" );
	PRECACHE_MODEL( "models/gunner.mdl" );
	PRECACHE_MODEL( "models/gunnergibs.mdl" );
	UTIL_PrecacheOther( "anime_rocket" );
	UTIL_PrecacheOther( "animebackpack_rocket" );
	UTIL_PrecacheOther( "sphere_explosion" );
	PRECACHE_SOUND( "gunner/gunner_boom1.wav" );
	PRECACHE_SOUND( "gunner/gunner_boom2.wav" );
	CBaseRust::Precache( );
}

void CGunner::PrescheduleThink( void )
{
	if ( !HasConditions( bits_COND_SEE_ENEMY ) )
		EyeOff( );
	else
		EyeOn( 200 );

	if ( m_MonsterState != MONSTERSTATE_COMBAT && RANDOM_LONG( 0, 60 ) == 0 )
		IdleSound( );

	EyeUpdate( );
}

Schedule_t *CGunner::GetSchedule( void )
{
	switch ( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) && HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
		{
			if ( UTIL_PercentChance( 90 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
			else
				return GetScheduleOfType( SCHED_MELEE_ATTACK2 );
		}
		break;
	}

	return CBaseRust::GetSchedule( );
}


//=========================================================
// the AI gunner of the end chapter: on the player's side,
// it tells how the end boss fares and fires its back rockets
// at the boss. It loses no health to speak of, ignores the
// aliens unless one hurt it lately, and the boss and the
// player can't hurt it at all.
//=========================================================
class CFriendlyGunner : public CGunner
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void ) { return CLASS_PLAYER_ALLY; }
	void AlertSound( void ) { }
	void IdleSound( void ) { }
	void PainSound( void );
	void StepSound( void );
	int  IRelationship( CBaseEntity *pTarget );
	void PrescheduleThink( void );
	void EndbossStatus( void );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	CBackpackAnimeRocket *ShootBackpackRocket( Vector vecSrc );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	float m_flPainTime;
	float m_flDistractedTime;		// fights the aliens until then
	int m_iEndbossStatusUpdate;		// the next health report, by the boss's health in tenths
};

LINK_ENTITY_TO_CLASS( monster_gunner_friendly, CFriendlyGunner );

TYPEDESCRIPTION	CFriendlyGunner::m_SaveData[] =
{
	DEFINE_FIELD( CFriendlyGunner, m_flPainTime, FIELD_TIME ),
	DEFINE_FIELD( CFriendlyGunner, m_flDistractedTime, FIELD_TIME ),
	DEFINE_FIELD( CFriendlyGunner, m_iEndbossStatusUpdate, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CFriendlyGunner, CGunner );

void CFriendlyGunner::Precache( void )
{
	PRECACHE_MODEL( "models/aigunner.mdl" );
	PRECACHE_MODEL( "sprites/cameye.spr" );
	PRECACHE_SOUND( "mainframe/end_7.wav" );
	PRECACHE_SOUND( "mainframe/end_8.wav" );
	PRECACHE_SOUND( "mainframe/end_9.wav" );
	PRECACHE_SOUND( "mainframe/mainend1.wav" );
	PRECACHE_SOUND( "mainframe/mainend2.wav" );
	PRECACHE_SOUND( "mainframe/mainend3.wav" );
	PRECACHE_SOUND( "mainframe/mainend4.wav" );
	PRECACHE_SOUND( "mainframe/mainend5.wav" );
	CGunner::Precache( );
}

void CFriendlyGunner::StepSound( void )
{
}

void CFriendlyGunner::PainSound( void )
{
	if ( m_flPainTime <= gpGlobals->time )
	{
		switch ( RANDOM_LONG( 0, 4 ) )
		{
		case 0:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "mainframe/mainend1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 1:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "mainframe/mainend2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 2:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "mainframe/mainend3.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 3:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "mainframe/mainend4.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 4:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "mainframe/mainend5.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		}

		m_flPainTime = gpGlobals->time + 3;
	}
}

void CFriendlyGunner::Spawn( void )
{
	CGunner::Spawn( );

	if ( m_pEyeGlow )
	{
		UTIL_Remove( m_pEyeGlow );
		m_pEyeGlow = NULL;
	}

	m_pEyeGlow = CSprite::SpriteCreate( "sprites/cameye.spr", pev->origin, FALSE );
	m_pEyeGlow->SetTransparency( kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation );
	m_pEyeGlow->SetAttachment( edict(), 1 );
	m_pEyeGlow->SetScale( 0.2 );
	EyeOff( );

	SET_MODEL( ENT(pev), "models/aigunner.mdl" );
	UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 150 ) );

	switch ( g_iSkillLevel )
	{
	case SKILL_MEDIUM:
		pev->health = pev->max_health = 50;
		break;
	case SKILL_HARD:
		pev->health = pev->max_health = 35;
		break;
	default:
		pev->health = pev->max_health = 75;
		break;
	}

	m_flFieldOfView = VIEW_FIELD_FULL;
	pev->takedamage = DAMAGE_YES;
	m_flPainTime = gpGlobals->time;
	m_flDistractedTime = gpGlobals->time;
	m_iEndbossStatusUpdate = 7;
}

int CFriendlyGunner::IRelationship( CBaseEntity *pTarget )
{
	int iRelationship = CGunner::IRelationship( pTarget );

	// the aliens don't matter, unless one hurt it lately
	if ( pTarget && m_flDistractedTime < gpGlobals->time )
	{
		if ( pTarget->Classify() == CLASS_ALIEN_BEAK || pTarget->Classify() == CLASS_ALIEN_TUBE || pTarget->Classify() == CLASS_ALIEN_CRITTER )
			return R_NO;
	}

	return iRelationship;
}

void CFriendlyGunner::PrescheduleThink( void )
{
	// the eye lights up while the player is in front
	CBaseEntity *pPlayer = (CBaseEntity *)GET_PRIVATE( INDEXENT( 1 ) );
	if ( pPlayer )
	{
		Vector vecForward;
		UTIL_MakeVectorsPrivate( pev->angles, vecForward, NULL, NULL );

		if ( DotProduct( ( pPlayer->pev->origin - pev->origin ).Normalize(), vecForward ) > 0 )
		{
			EyeOn( 200 );
			EyeUpdate( );
			EndbossStatus( );
			return;
		}
	}

	EyeOff( );
	EyeUpdate( );
	EndbossStatus( );
}

//=========================================================
// EndbossStatus - reports the end boss's health as it falls
// below seven, five and two tenths
//=========================================================
void CFriendlyGunner::EndbossStatus( void )
{
	CBaseEntity *pBoss = UTIL_FindEntityByClassname( NULL, "monster_endboss" );
	if ( pBoss && pBoss->IsAlive() )
	{
		switch ( m_iEndbossStatusUpdate )
		{
		case 2:
			if ( pBoss->pev->health < pBoss->pev->max_health * 0.2 )
			{
				UTIL_EmitAmbientSound( ENT(pev), pev->origin, "mainframe/end_8.wav", 1, ATTN_NONE, 0, PITCH_NORM );
				m_iEndbossStatusUpdate = 10;
			}
			break;
		case 5:
			if ( pBoss->pev->health < pBoss->pev->max_health * 0.5 )
			{
				UTIL_EmitAmbientSound( ENT(pev), pev->origin, "mainframe/end_7.wav", 1, ATTN_NONE, 0, PITCH_NORM );
				m_iEndbossStatusUpdate = 2;
			}
			break;
		case 7:
			if ( pBoss->pev->health < pBoss->pev->max_health * 0.7 )
			{
				UTIL_EmitAmbientSound( ENT(pev), pev->origin, "mainframe/end_9.wav", 1, ATTN_NONE, 0, PITCH_NORM );
				m_iEndbossStatusUpdate = 5;
			}
			break;
		}
	}
}

int CFriendlyGunner::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( !strcmp( STRING( pevAttacker->classname ), "monster_endboss" ) )
		return 0;

	if ( !strcmp( STRING( pevAttacker->classname ), "player" ) )
		return 0;

	PainSound( );

	if ( UTIL_PercentChance( 25 ) )
		m_flDistractedTime = gpGlobals->time + RANDOM_LONG( 7, 15 );

	return CGunner::TakeDamage( pevInflictor, pevAttacker, 1, bitsDamageType | DMG_NEVERGIB );
}

void CFriendlyGunner::Killed( entvars_t *pevAttacker, int iGib )
{
	FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
	CGunner::Killed( pevAttacker, iGib );
}

BOOL CFriendlyGunner::CheckRangeAttack1( float flDot, float flDist )
{
	if ( m_fNextRangeAttack > gpGlobals->time )
		return FALSE;

	// let the beaks come close
	if ( m_hEnemy != NULL && FClassnameIs( m_hEnemy->pev, "monster_beak" ) && flDist < 256 )
		return FALSE;

	TraceResult tr;
	BOOL bRight = FALSE;
	BOOL bLeft;

	if ( UTIL_PointContents( GetArmPosition( GUNNER_ARM_RIGHT ) ) != CONTENTS_SOLID )
	{
		UTIL_TraceLine( GetArmPosition( GUNNER_ARM_RIGHT ), m_hEnemy->BodyTarget( GetArmPosition( GUNNER_ARM_RIGHT ) ), ignore_monsters, ignore_glass, ENT(pev), &tr );
		if ( tr.flFraction == 1.0 || tr.pHit == m_hEnemy->edict() )
			bRight = TRUE;
	}

	bLeft = FALSE;
	if ( UTIL_PointContents( GetArmPosition( GUNNER_ARM_LEFT ) ) != CONTENTS_SOLID )
	{
		UTIL_TraceLine( GetArmPosition( GUNNER_ARM_LEFT ), m_hEnemy->BodyTarget( GetArmPosition( GUNNER_ARM_LEFT ) ), ignore_monsters, ignore_glass, ENT(pev), &tr );
		if ( tr.flFraction == 1.0 || tr.pHit == m_hEnemy->edict() )
			bLeft = TRUE;
	}

	if ( !bRight && !bLeft )
		return FALSE;

	if ( bRight && bLeft )
	{
		if ( RANDOM_LONG( 0, 1 ) )
			m_iArm = GUNNER_ARM_RIGHT;
		else
			m_iArm = GUNNER_ARM_LEFT;
	}
	else if ( bRight )
		m_iArm = GUNNER_ARM_RIGHT;
	else
		m_iArm = GUNNER_ARM_LEFT;

	m_fNextRangeAttack = gpGlobals->time + 2;
	return TRUE;
}

//=========================================================
// ShootBackpackRocket - as the gunner's, but the rockets go
// for the end boss while it lives
//=========================================================
CBackpackAnimeRocket *CFriendlyGunner::ShootBackpackRocket( Vector vecSrc )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/kaboom1.wav", 0.6, ATTN_NORM, 0, PITCH_NORM );
	UTIL_MakeAimVectors( pev->angles );

	if ( --m_iVollyCount <= 0 )
		m_iVollyCount = 4;

	Vector vecVelocity;

	switch ( m_iVollyCount % 5 )	// sic: the count runs 4 to 1, so case 0 never happens
	{
	case 0:
		vecSrc = vecSrc + gpGlobals->v_right * 15;
		vecVelocity = gpGlobals->v_up * 80 + gpGlobals->v_right * 75 - gpGlobals->v_forward * 200;
		break;
	case 1:
		vecSrc = vecSrc + gpGlobals->v_right * 30;
		vecVelocity = gpGlobals->v_up * 50 + gpGlobals->v_right * 150 - gpGlobals->v_forward * 200;
		break;
	case 2:
		vecSrc = vecSrc - gpGlobals->v_right * 15;
		vecVelocity = gpGlobals->v_up * 80 - gpGlobals->v_right * 75 - gpGlobals->v_forward * 200;
		break;
	case 3:
		vecSrc = vecSrc - gpGlobals->v_right * 30;
		vecVelocity = gpGlobals->v_up * 50 - gpGlobals->v_right * 150 - gpGlobals->v_forward * 200;
		break;
	case 4:
		vecVelocity = gpGlobals->v_up * 100 - gpGlobals->v_forward * 200;
		break;
	}

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecSrc.x );
		WRITE_COORD( vecSrc.y );
		WRITE_COORD( vecSrc.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 10 ); // scale * 10
		WRITE_BYTE( 16 ); // framerate
	MESSAGE_END();

	CBackpackAnimeRocket *pRocket = GetClassPtr( (CBackpackAnimeRocket *)NULL );
	pRocket->pev->owner = ENT(pev);

	CBaseEntity *pBoss = UTIL_FindEntityByClassname( NULL, "monster_endboss" );
	if ( pBoss )
	{
		if ( pBoss->IsAlive() )
			pRocket->m_hEnemy = pBoss;
	}
	else if ( m_hEnemy )
		pRocket->m_hEnemy = m_hEnemy;
	else
		pRocket->m_hEnemy = NULL;

	pRocket->Spawn( );
	UTIL_SetOrigin( pRocket->pev, vecSrc );
	pRocket->pev->velocity = pRocket->pev->velocity + vecVelocity;
	pRocket->pev->angles = UTIL_VecToAngles( pRocket->pev->velocity );

	return pRocket;
}
