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
// monster_endboss: the end chapter's boss, a giant robot
// with a katagun, a rocket launcher and a pair of machine
// guns. Only the friendly gunner can hurt it, once tank
// shells and rockets have shot the katagun off. It throws
// flares at tank rockets that come for it, and stomps
// whoever comes close. Also its kataballs, its rockets,
// its anti-rocket flares and the gib it leaves.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "animation.h"
#include "weapons.h"
#include "soundent.h"
#include "player.h"
#include "decals.h"
#include "vehicletank.h"
#include "sphereexplosion.h"

extern DLL_GLOBAL int g_iSkillLevel;

//=========================================================
// endboss_kataball - the katagun's ball. It homes in on
// the boss's enemy and tells the map what it hit. The class
// is a copy of the rocket's, down to its saved fields.
//=========================================================
class CEndBossKataball : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void ) { return CLASS_ENDBOSS; }

	void EXPORT KataTouch( CBaseEntity *pOther );
	void EXPORT IgniteThink( void );
	void EXPORT AccelerateThink( void );

	// IsInWorld without the speed test
	BOOL InWorld( void )
	{
		return pev->origin.x >= -4096 && pev->origin.x <= 4096 &&
			pev->origin.y >= -4096 && pev->origin.y <= 4096 &&
			pev->origin.z >= -4096 && pev->origin.z <= 4096;
	}

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iTrail;					// sprites/firebeam.spr; no trail is drawn
	BOOL m_bEndbossRocketSound;		// never used
	int m_iKataImpact;				// events/kataimpact.sc
};

LINK_ENTITY_TO_CLASS( endboss_kataball, CEndBossKataball );

TYPEDESCRIPTION	CEndBossKataball::m_SaveData[] =
{
	DEFINE_FIELD( CEndBossKataball, m_iTrail, FIELD_INTEGER ),
	DEFINE_FIELD( CEndBossKataball, m_bEndbossRocketSound, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CEndBossKataball, CGrenade );

void CEndBossKataball::Precache( void )
{
	PRECACHE_MODEL( "sprites/kata.spr" );
	m_iTrail = PRECACHE_MODEL( "sprites/firebeam.spr" );
	PRECACHE_MODEL( "sprites/flame.spr" );
	PRECACHE_SOUND( "endboss/imp1_gs.wav" );
	PRECACHE_SOUND( "endboss/imp2_gs.wav" );
	PRECACHE_SOUND( "endboss/imp3_gs.wav" );
	m_iKataImpact = PRECACHE_EVENT( 1, "events/kataimpact.sc" );
	PRECACHE_MODEL( "sprites/kataimpact.spr" );
}

void CEndBossKataball::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_FLYMISSILE;
	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT( pev ), "sprites/kata.spr" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	UTIL_SetOrigin( pev, pev->origin );

	pev->rendermode = kRenderTransAdd;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 255;

	SetThink( &CEndBossKataball::IgniteThink );
	pev->nextthink = gpGlobals->time + 0.1;
	SetTouch( NULL );

	UTIL_MakeAimVectors( pev->angles );
	pev->velocity = gpGlobals->v_forward * 400;

	pev->gravity = 0.5;
	pev->framerate = 15;
	pev->frame = 0;
}

void CEndBossKataball::KataTouch( CBaseEntity *pOther )
{
	if ( pOther->IsPlayer( ) )
	{
		FireTargets( "katahitplayer", this, this, USE_TOGGLE, 0 );
		UTIL_Remove( this );
		return;
	}

	if ( FClassnameIs( pOther->pev, "vehicle_tank_barrel" )
		|| FClassnameIs( pOther->pev, "vehicle_tank_turret" )
		|| FClassnameIs( pOther->pev, "vehicle_tank" )
		|| FClassnameIs( pOther->pev, "vehicle_tank_body" ) )
	{
		// tell the map whether the tank it hit had a driver
		CBaseEntity *pTank = NULL;
		while ( ( pTank = UTIL_FindEntityByClassname( pTank, "vehicle_tank" ) ) != NULL )
		{
			if ( ((CVehicle *)pTank)->m_iVehicleID == ((CVehicle *)pOther)->m_iVehicleID )
			{
				if ( ((CVehicle *)pTank)->m_pPlayer )
					FireTargets( "katahitwithvehicle", this, this, USE_TOGGLE, 0 );
				else
					FireTargets( "katahitwithoutvehicle", this, this, USE_TOGGLE, 0 );
			}
		}
	}

	if ( FClassnameIs( pOther->pev, "monster_gunner_friendly" ) )
	{
		FireTargets( "katahitgunner", this, this, USE_TOGGLE, 0 );
		UTIL_Remove( this );
		return;
	}

	// sic: the angles spelled as &g_vecZero[0] (same address): written as
	// (float *)&g_vecZero twice, VC6 shares the address constant, and that
	// gives this edi and the class name pointer ebp, the other way round
	// from retail
	PLAYBACK_EVENT_FULL( 0, ENT( pev ), m_iKataImpact, 0.0, (float *)&g_vecZero, (float *)&g_vecZero[0], 0.0, 0.0, 0, 0, 0, 0 );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "endboss/imp1_gs.wav", 1, ATTN_NORM );	break;
	case 1:	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "endboss/imp2_gs.wav", 1, ATTN_NORM );	break;
	case 2:	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "endboss/imp3_gs.wav", 1, ATTN_NORM );	break;
	}

	SetThink( &CBaseEntity::SUB_Remove );
	SetTouch( NULL );
	pev->nextthink = gpGlobals->time + 0.2;
	pev->effects |= EF_NODRAW;
}

void CEndBossKataball::IgniteThink( void )
{
	SetTouch( &CEndBossKataball::KataTouch );
	SetThink( &CEndBossKataball::AccelerateThink );
	pev->nextthink = gpGlobals->time + 0.1;
	pev->movetype = MOVETYPE_FLY;
}

void CEndBossKataball::AccelerateThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	pev->frame += 1;
	pev->frame = (int)pev->frame % 9;

	if ( InWorld( ) )
	{
		Vector vecTarget;
		Vector vecDir;
		Vector vecVelocity;
		Vector vecSteer;
		float flSpeed;
		float flTurn;

		if ( m_hEnemy != NULL )
			vecTarget = m_hEnemy->BodyTarget( pev->origin );
		else
			vecTarget = pev->velocity.Normalize( ) * 1024;	// sic: not added to the origin

		flSpeed = pev->velocity.Length( );

		// speed up to a top speed that depends on the skill
		switch ( g_iSkillLevel )
		{
		case SKILL_MEDIUM:
			if ( flSpeed < 750 )
				flSpeed += 7;
			break;
		case SKILL_HARD:
			if ( flSpeed < 1000 )
				flSpeed += 10;
			break;
		default:
			if ( flSpeed < 500 )
				flSpeed += 5;
			break;
		}

		vecDir = vecTarget - pev->origin;
		vecDir = vecDir.Normalize( );

		// and turn harder at higher skill
		// sic: x and y are scaled by 1, no-ops; the explicit vecDir.x and vecDir.y
		// give retail's multiplies in the inlined Normalize() calls on vecDir
		switch ( g_iSkillLevel )
		{
		case SKILL_MEDIUM:
			flTurn = 12;
			vecDir.x = vecDir.x * 1;
			vecDir.y = vecDir.y * 1;
			vecDir.z = vecDir.z * 1.25;
			break;
		case SKILL_HARD:
			flTurn = 17;
			break;
		default:
			flTurn = 8;
			vecDir.x = vecDir.x * 1;
			vecDir.y = vecDir.y * 1;
			vecDir.z = vecDir.z * 1.5;
			break;
		}

		vecVelocity = pev->velocity.Normalize( ) * 128;
		vecSteer = vecDir.Normalize( ) * flTurn;
		pev->velocity = ( vecSteer + vecVelocity ).Normalize( ) * flSpeed;
		pev->angles = UTIL_VecToAngles( pev->velocity );
	}
	else
	{
		UTIL_Remove( this );
	}
}


//=========================================================
// endboss_rocket - the boss's rocket. It leaves in a random
// direction ahead of the boss, then homes in on its enemy.
//=========================================================
class CEndBossRocket : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void ) { return CLASS_ENDBOSS; }

	void EXPORT IgniteThink( void );
	void EXPORT AccelerateThink( void );
	void EXPORT RocketTouch( CBaseEntity *pOther );

	// IsInWorld without the speed test
	BOOL InWorld( void )
	{
		return pev->origin.x >= -4096 && pev->origin.x <= 4096 &&
			pev->origin.y >= -4096 && pev->origin.y <= 4096 &&
			pev->origin.z >= -4096 && pev->origin.z <= 4096;
	}

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iTrail;
	BOOL m_bEndbossRocketSound;		// never used
};

LINK_ENTITY_TO_CLASS( endboss_rocket, CEndBossRocket );

TYPEDESCRIPTION	CEndBossRocket::m_SaveData[] =
{
	DEFINE_FIELD( CEndBossRocket, m_iTrail, FIELD_INTEGER ),
	DEFINE_FIELD( CEndBossRocket, m_bEndbossRocketSound, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CEndBossRocket, CGrenade );

void CEndBossRocket::Precache( void )
{
	PRECACHE_MODEL( "models/rocket.mdl" );
	m_iTrail = PRECACHE_MODEL( "sprites/firebeam.spr" );
	PRECACHE_MODEL( "sprites/flame.spr" );
	PRECACHE_SOUND( "endboss/boss_boom1.wav" );
	PRECACHE_SOUND( "endboss/boss_boom2.wav" );
	PRECACHE_SOUND( "endboss/boss_boom3.wav" );
	PRECACHE_SOUND( "endboss/boss_boom4.wav" );
}

void CEndBossRocket::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT( pev ), "models/rocket.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CEndBossRocket::IgniteThink );
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.4, 0.8 );
	SetTouch( &CEndBossRocket::RocketTouch );

	Vector vecForward, vecRight, vecUp;
	UTIL_MakeVectorsPrivate( pev->owner->v.angles, vecForward, vecRight, vecUp );

	// sic: the target is never used
	Vector vecTarget;
	if ( m_hEnemy != NULL )
		vecTarget = m_hEnemy->BodyTarget( pev->origin );
	else
		vecTarget = pev->origin + vecForward * 1024;

	// leave toward a random spot 64 units ahead of the boss
	Vector vecSpot = pev->origin + 64 * vecForward +
		( RANDOM_FLOAT( 0, 64 ) - 32 ) * vecRight + ( RANDOM_FLOAT( 0, 64 ) - 32 ) * vecUp;
	pev->velocity = ( vecSpot - pev->origin ).Normalize( ) * 500;
	pev->angles = UTIL_VecToAngles( pev->velocity );

	pev->dmg = 50;

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex( ) );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 8 ); // life
		WRITE_BYTE( 8 );  // width
		WRITE_BYTE( 180 );   // r, g, b
		WRITE_BYTE( 25 );   // r, g, b
		WRITE_BYTE( 25 );   // r, g, b
		WRITE_BYTE( 180 );	// brightness
	MESSAGE_END( );
}

void CEndBossRocket::IgniteThink( void )
{
	// a longer, thinner trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex( ) );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 12 ); // life
		WRITE_BYTE( 2 );  // width
		WRITE_BYTE( 180 );   // r, g, b
		WRITE_BYTE( 25 );   // r, g, b
		WRITE_BYTE( 25 );   // r, g, b
		WRITE_BYTE( 180 );	// brightness
	MESSAGE_END( );

	SetThink( &CEndBossRocket::AccelerateThink );
	pev->nextthink = gpGlobals->time;

	pev->velocity = pev->velocity.Normalize( ) * 700;
	pev->avelocity.z = 200;
}

void CEndBossRocket::AccelerateThink( void )
{
	float flSpeed = pev->velocity.Length( );

	if ( InWorld( ) )
	{
		Vector vecTarget;
		Vector vecDir;
		Vector vecVelocity;

		if ( m_hEnemy != NULL )
			vecTarget = m_hEnemy->BodyTarget( pev->origin );
		else
			vecTarget = pev->velocity.Normalize( ) * 1024;	// sic: not added to the origin

		vecDir = vecTarget - pev->origin;
		vecDir.x = vecDir.x * 1;	// sic: a no-op; an explicit vecDir.x gives retail's multiply in the Normalize below
		vecDir = vecDir.Normalize( ) * 128;
		vecVelocity = pev->velocity.Normalize( ) * 128;
		pev->velocity = ( vecDir - vecVelocity ).Normalize( ) * 16 + vecVelocity;

		flSpeed = min( flSpeed + 50, 1000 );

		pev->velocity = pev->velocity.Normalize( ) * flSpeed;
		pev->angles = UTIL_VecToAngles( pev->velocity );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		UTIL_Remove( this );
	}
}

// CGrenade::Explode, with the boss's own sounds
void CEndBossRocket::RocketTouch( CBaseEntity *pOther )
{
	if ( ENT( pOther->pev ) == pev->owner )
		return;

	switch ( RANDOM_LONG( 0, 3 ) )
	{
	case 0:	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "endboss/boss_boom1.wav", 1, ATTN_NORM );	break;
	case 1:	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "endboss/boss_boom2.wav", 1, ATTN_NORM );	break;
	case 2:	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "endboss/boss_boom3.wav", 1, ATTN_NORM );	break;
	case 3:	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "endboss/boss_boom4.wav", 1, ATTN_NORM );	break;
	}

	TraceResult tr;
	Vector vecSpot = pev->origin - pev->velocity.Normalize( ) * 32;
	UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize( ) * 64, ignore_monsters, ENT( pev ), &tr );

	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible

	pev->takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if ( tr.flFraction != 1.0 )
	{
		pev->origin = tr.vecEndPos + ( tr.vecPlaneNormal * ( pev->dmg - 24 ) * 0.6 );
	}

	int iContents = UTIL_PointContents( pev->origin );

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( pev->origin.x );	// Send to PAS because of the sound
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		if ( iContents != CONTENTS_WATER )
		{
			WRITE_SHORT( RandomExplosionSprite( ) );
		}
		else
		{
			WRITE_SHORT( g_sModelIndexWExplosion );
		}
		WRITE_BYTE( pev->dmg * 0.6 ); // scale * 10
		WRITE_BYTE( 15 ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END( );

	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	RadiusDamage( pev, pevOwner, pev->dmg, CLASS_NONE, DMG_BLAST );

	if ( RANDOM_FLOAT( 0, 1 ) < 0.5 )
	{
		UTIL_DecalTrace( &tr, DECAL_SCORCH1 );
	}
	else
	{
		UTIL_DecalTrace( &tr, DECAL_SCORCH2 );
	}

	switch ( RANDOM_LONG( 0, 2 ) )
	{
		case 0:	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris1.wav", 0.55, ATTN_NORM );	break;
		case 1:	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris2.wav", 0.55, ATTN_NORM );	break;
		case 2:	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris3.wav", 0.55, ATTN_NORM );	break;
	}

	pev->effects |= EF_NODRAW;
	SetThink( &CGrenade::Smoke );
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 2;

	if ( iContents != CONTENTS_WATER )
	{
		int sparkCount = RANDOM_LONG( 0, 3 );
		for ( int i = 0; i < sparkCount; i++ )
			Create( "spark_shower", pev->origin, tr.vecPlaneNormal, NULL );
	}
}


//=========================================================
// antirocketflare - an invisible target the boss throws at
// tank rockets that home in on it; gone after 3 seconds
//=========================================================
class CAntirocketflare : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void ) { PRECACHE_MODEL( "models/null.mdl" ); }
	int  Classify( void ) { return CLASS_ALIEN_TUBE; }
};

LINK_ENTITY_TO_CLASS( antirocketflare, CAntirocketflare );

void CAntirocketflare::Spawn( void )
{
	pev->model = MAKE_STRING( "models/null.mdl" );
	pev->classname = MAKE_STRING( "antirocketflare" );
	SET_MODEL( ENT( pev ), "models/null.mdl" );

	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->effects = 0;
	pev->takedamage = DAMAGE_YES;
	pev->effects |= EF_NODRAW;

	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	SetUse( NULL );
	SetBlocked( NULL );
	pev->nextthink = gpGlobals->time + 3;
}


//=========================================================
// endboss_gib - the boss's wreck: its model playing the
// death sequence once
//=========================================================
class CEndBossGib : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void ) { PRECACHE_MODEL( "models/endboss.mdl" ); }
	int  Classify( void ) { return CLASS_ALIEN_TUBE; }

	void EXPORT FlopAnim( void );
};

void CEndBossGib::FlopAnim( void )
{
	StudioFrameAdvance( );

	if ( m_fSequenceFinished )
	{
		SetThink( NULL );
		pev->framerate = 0;
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

LINK_ENTITY_TO_CLASS( endboss_gib, CEndBossGib );

void CEndBossGib::Spawn( void )
{
	pev->model = MAKE_STRING( "models/endboss.mdl" );
	pev->classname = MAKE_STRING( "endboss_gib" );
	SET_MODEL( ENT( pev ), "models/endboss.mdl" );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	pev->effects = 0;
	pev->takedamage = DAMAGE_NO;

	SetThink( &CEndBossGib::FlopAnim );
	SetSequenceByName( "death" );
	SetTouch( NULL );
	SetUse( NULL );
	SetBlocked( NULL );

	SetBodygroup( 1, 2 );
	pev->nextthink = gpGlobals->time;
}


//=========================================================
// monster-specific DEFINE's
//=========================================================
#define	ENDBOSS_AE_ROCKET			3
#define	ENDBOSS_AE_KATAGUN			4
#define	ENDBOSS_AE_MGUN_LEFT		6
#define	ENDBOSS_AE_MGUN_RIGHT		7
#define	ENDBOSS_AE_STOMP			10

// GetGunPosition's guns
#define ENDBOSS_GUN_MGUN_LOW		0	// nothing fires from it
#define ENDBOSS_GUN_MGUN_HIGH		1
#define ENDBOSS_GUN_ROCKET			2
#define ENDBOSS_GUN_KATAGUN			3

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_ENDBOSS_MACHINEGUN = LAST_COMMON_SCHEDULE + 1,
};

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlEndBossMachinegun[] =
{
	{ TASK_STOP_MOVING,			(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_SPECIAL_ATTACK2	},
	{ TASK_FACE_ENEMY,			(float)0					},
};

Schedule_t	slEndBossMachinegun[] =
{
	{
		tlEndBossMachinegun,
		ARRAYSIZE ( tlEndBossMachinegun ),
		0,
		0,
		"Special Machinegun Attack"
	},
};

//=========================================================
// monster_endboss
//=========================================================
class CEndBoss : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void PrecacheSounds( void );
	void SetObjectCollisionBox( void );
	int  Classify( void ) { return CLASS_ENDBOSS; }
	int  MonsterType( void ) { return 22; }
	float HearingSensitivity( void ) { return 2.0; }
	void SetYawSpeed( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );
	void RunAI( void );
	void SetActivity( Activity NewActivity );
	int  IgnoreConditions( void );

	void CheckAttacks( CBaseEntity *pTarget, float flDist );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack2( float flDot, float flDist );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckMachinegunAttack( float flDot, float flDist );

	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );

	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void RangeAttackSound( void );
	void MeleeAttackSound( void );
	void DeathSound( void );
	void StompSound( void );

	Vector GetGunPosition( int iGun );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

	static const char *pSounds[];

	float m_fNextRocketAttack;
	float m_fNextKataAttack;
	float m_fNextMachinegunAttack;
	float m_flLostKatagunTime;		// the katagun's stump smokes until then
	float m_fNextSpeak;
	BOOL m_bHasKatagun;
	int m_iEnemyHitMe;				// tank shells and rockets that hit the katagun
	int m_iKataFiring;				// kataballs fired, and hits on the katagun past 15

	int m_iFireball;
	int m_iMetalGibs;
	unsigned short m_usTankMGuns;
};

LINK_ENTITY_TO_CLASS( monster_endboss, CEndBoss );

TYPEDESCRIPTION	CEndBoss::m_SaveData[] =
{
	DEFINE_FIELD( CEndBoss, m_fNextSpeak, FIELD_TIME ),
	DEFINE_FIELD( CEndBoss, m_fNextKataAttack, FIELD_TIME ),
	DEFINE_FIELD( CEndBoss, m_fNextRocketAttack, FIELD_TIME ),
	DEFINE_FIELD( CEndBoss, m_fNextMachinegunAttack, FIELD_TIME ),
	DEFINE_FIELD( CEndBoss, m_flLostKatagunTime, FIELD_TIME ),
	DEFINE_FIELD( CEndBoss, m_bHasKatagun, FIELD_BOOLEAN ),
	DEFINE_FIELD( CEndBoss, m_iKataFiring, FIELD_BOOLEAN ),	// sic: a count
	DEFINE_FIELD( CEndBoss, m_iEnemyHitMe, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CEndBoss, CBaseMonster );

DEFINE_CUSTOM_SCHEDULES( CEndBoss )
{
	slEndBossMachinegun,
};

IMPLEMENT_CUSTOM_SCHEDULES( CEndBoss, CBaseMonster );

const char *CEndBoss::pSounds[] =
{
	"endboss/endboss_stomp1.wav",
	"endboss/endboss_stomp2.wav",
	"endboss/endboss_stomp3.wav",
	"endboss/endboss_stomp4.wav",
	"endboss/endboss_stomp5.wav",
	"endboss/endboss_stomp6.wav",
	"endboss/endboss_stomp7.wav",
	"endboss/endboss_stomp8.wav",
	"endboss/kata_cannon1.wav",
	"endboss/end_stomp.wav",
	"endboss/end_stomp2.wav",
	"general/boss_hurt1.wav",
	"general/boss_hurt2.wav",
	"general/boss_hurt3.wav",
	"general/boss_hurt4.wav",
	"general/boss_hurt5.wav",
};

void CEndBoss::PrecacheSounds( void )
{
	for ( int i = 0; i < ARRAYSIZE( pSounds ); i++ )
		PRECACHE_SOUND( (char *)pSounds[i] );
}

// the boss is silent but for its attacks and its hurt lines
void CEndBoss::PainSound( void )
{
}

void CEndBoss::AlertSound( void )
{
}

void CEndBoss::IdleSound( void )
{
}

void CEndBoss::RangeAttackSound( void )
{
}

void CEndBoss::MeleeAttackSound( void )
{
}

void CEndBoss::DeathSound( void )
{
}

// nothing calls it
void CEndBoss::StompSound( void )
{
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CEndBoss::SetYawSpeed( void )
{
	int ys;

	ys = 45;

	switch ( m_Activity )
	{
	case ACT_WALK:
	case ACT_RUN:
		ys = 60;
		break;
	case ACT_RANGE_ATTACK1:
	case ACT_RANGE_ATTACK2:
		ys = 50;
		break;
	case ACT_MELEE_ATTACK2:
		ys = 70;
		break;
	case ACT_IDLE:
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 40;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CEndBoss::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	Vector vecShootDir;
	float flZ;

	switch ( pEvent->event )
	{
	case ENDBOSS_AE_ROCKET:
	{
		RangeAttackSound( );

		Vector vecSrc = GetGunPosition( ENDBOSS_GUN_ROCKET );

		if ( UTIL_PercentChance( 50 ) )
		{
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
				WRITE_BYTE( TE_SMOKE );
				WRITE_COORD( vecSrc.x );
				WRITE_COORD( vecSrc.y );
				WRITE_COORD( vecSrc.z );
				WRITE_SHORT( g_sModelIndexSmoke );
				WRITE_BYTE( 20 ); // scale * 10
				WRITE_BYTE( 12 ); // framerate
			MESSAGE_END( );
		}

		CEndBossRocket *pRocket = GetClassPtr( (CEndBossRocket *)NULL );
		pRocket->pev->owner = edict( );
		if ( m_hEnemy != NULL )
			pRocket->m_hEnemy = m_hEnemy;
		else
			pRocket->m_hEnemy = NULL;
		pRocket->pev->origin = vecSrc;
		pRocket->Spawn( );

		UTIL_SetOrigin( pRocket->pev, vecSrc );
		CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 2048, 3 );
	}
	break;

	// sic: both machine gun events fire from the same barrel
	case ENDBOSS_AE_MGUN_LEFT:
	{
		Vector vecBarrel, vecMuzzle, vecAngles;
		GetAttachment( 1, vecBarrel, vecAngles );
		GetAttachment( 0, vecMuzzle, vecAngles );

		vecShootDir = vecBarrel - vecMuzzle;

		// aim up or down at the enemy, by a tenth of the barrel at most
		flZ = vecShootDir.z;
		if ( m_hEnemy != NULL )
		{
			flZ = ShootAtEnemy( GetGunPosition( ENDBOSS_GUN_MGUN_HIGH ) ).z;
			flZ = max( vecShootDir.z - vecShootDir.Length( ) * 0.1, flZ );
			flZ = min( vecShootDir.z + vecShootDir.Length( ) * 0.1, flZ );
		}
		vecShootDir.z = flZ;
		vecShootDir = vecShootDir.Normalize( );

		FireBullets( 1, GetGunPosition( ENDBOSS_GUN_MGUN_HIGH ), vecShootDir, g_vecZero, 4096, BULLET_PLAYER_9MM, 3 );

		vecShootDir = UTIL_VecToAngles( vecShootDir );
		vecShootDir.x = vecShootDir.x * -1;

		PLAYBACK_EVENT_FULL( FEV_RELIABLE, edict( ), m_usTankMGuns, 0.0, ( GetGunPosition( ENDBOSS_GUN_MGUN_HIGH ) - pev->origin ), vecShootDir, 1.0, 0.0, 0, 0, 0, 0 );

		CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 2048, 0.1 );
	}
	break;

	case ENDBOSS_AE_MGUN_RIGHT:
	{
		Vector vecBarrel, vecMuzzle, vecAngles;
		float flZ;
		GetAttachment( 1, vecBarrel, vecAngles );
		GetAttachment( 0, vecMuzzle, vecAngles );

		vecShootDir = vecBarrel - vecMuzzle;

		flZ = vecShootDir.z;
		if ( m_hEnemy != NULL )
		{
			flZ = ShootAtEnemy( GetGunPosition( ENDBOSS_GUN_MGUN_HIGH ) ).z;
			flZ = max( vecShootDir.z - vecShootDir.Length( ) * 0.1, flZ );
			flZ = min( vecShootDir.z + vecShootDir.Length( ) * 0.1, flZ );
		}
		vecShootDir.z = flZ;
		vecShootDir = vecShootDir.Normalize( );

		FireBullets( 1, GetGunPosition( ENDBOSS_GUN_MGUN_HIGH ), vecShootDir, g_vecZero, 4096, BULLET_PLAYER_9MM, 3 );

		vecShootDir = UTIL_VecToAngles( vecShootDir );
		vecShootDir.x = vecShootDir.x * -1;

		PLAYBACK_EVENT_FULL( FEV_RELIABLE, edict( ), m_usTankMGuns, 0.0, ( GetGunPosition( ENDBOSS_GUN_MGUN_HIGH ) - pev->origin ), vecShootDir, 1.0, 0.0, 0, 0, 0, 0 );

		CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 2048, 0.1 );
	}
	break;

	case ENDBOSS_AE_KATAGUN:
	{
		RangeAttackSound( );
		m_iKataFiring++;

		CEndBossKataball *pKata = GetClassPtr( (CEndBossKataball *)NULL );
		pKata->pev->angles = pev->angles;
		pKata->Spawn( );
		pKata->pev->owner = edict( );
		if ( m_hEnemy != NULL )
			pKata->m_hEnemy = m_hEnemy;
		else
			pKata->m_hEnemy = NULL;

		UTIL_EmitAmbientSound( ENT( pev ), pev->origin, "endboss/kata_cannon1.wav", 1, ATTN_NONE, 0, 100 );

		UTIL_SetOrigin( pKata->pev, GetGunPosition( ENDBOSS_GUN_KATAGUN ) );
		CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 2048, 3 );
	}
	break;

	case ENDBOSS_AE_STOMP:
	{
		float flDamage;
		switch ( g_iSkillLevel )
		{
		case SKILL_MEDIUM:
			flDamage = 150;
			break;
		case SKILL_HARD:
			flDamage = 200;
			break;
		default:
			flDamage = 130;
			break;
		}

		UTIL_MakeVectors( pev->angles );
		::RadiusDamage( pev->origin + gpGlobals->v_forward * 64, pev, pev, flDamage, 300, CLASS_NONE, DMG_CRUSH );
		UTIL_ScreenShake( pev->origin + gpGlobals->v_forward * 64, 12.0, 3.0, 0.5, 600 );
		UTIL_EmitAmbientSound( ENT( pev ), pev->origin, "endboss/end_stomp2.wav", 1, ATTN_NONE, 0, 100 );
	}
	// sic: falls through

	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// GetSchedule - the base's combat schedules, with the
// boss's own attacks
//=========================================================
Schedule_t *CEndBoss::GetSchedule( void )
{
	switch ( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		{
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			{
				// clear the current (dead) enemy and try to find another.
				m_hEnemy = NULL;

				if ( GetEnemy( ) )
				{
					ClearConditions( bits_COND_ENEMY_DEAD );
					return GetSchedule( );
				}
				else
				{
					SetState( MONSTERSTATE_ALERT );
					return GetSchedule( );
				}
			}

			if ( HasConditions( bits_COND_NEW_ENEMY ) )
			{
				return GetScheduleOfType( SCHED_WAKE_ANGRY );
			}
			else if ( !HasConditions( bits_COND_SEE_ENEMY ) )
			{
				// we can't see the enemy
				if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) )
				{
					// enemy is unseen, but not occluded!
					// turn to face enemy
					return GetScheduleOfType( SCHED_COMBAT_FACE );
				}
				else
				{
					// chase!
					return GetScheduleOfType( SCHED_CHASE_ENEMY );
				}
			}
			else
			{
				// we can see the enemy
				if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
				{
					UTIL_EmitAmbientSound( ENT( pev ), pev->origin, "endboss/end_stomp.wav", 1, ATTN_NONE, 0, 100 );
					return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
				}
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				{
					return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
				}
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) )
				{
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
				}
				if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
				{
					return GetScheduleOfType( SCHED_ENDBOSS_MACHINEGUN );
				}
				if ( !HasConditions( bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK1 ) )
				{
					// if we can see enemy but can't use either attack type, we must need to get closer to enemy
					return GetScheduleOfType( SCHED_CHASE_ENEMY );
				}
				else if ( !FacingIdeal( ) )
				{
					//turn
					return GetScheduleOfType( SCHED_COMBAT_FACE );
				}
			}
			break;
		}
	}

	return CBaseMonster::GetSchedule( );
}

Schedule_t *CEndBoss::GetScheduleOfType( int Type )
{
	switch ( Type )
	{
	case SCHED_ENDBOSS_MACHINEGUN:
		return &slEndBossMachinegun[ 0 ];
	}

	return CBaseMonster::GetScheduleOfType( Type );
}

int CEndBoss::IgnoreConditions( void )
{
	return CBaseMonster::IgnoreConditions( ) | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE;
}

//=========================================================
// TakeDamage - the boss takes no damage but from the
// friendly gunner, once tank shells and rockets have shot
// its katagun off
//=========================================================
int CEndBoss::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( pevInflictor->owner == edict( ) )
		return 0;

	// a player on foot who hits it gets a hurt line now and then
	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );
	if ( pAttacker && pAttacker->IsPlayer( ) && ((CBasePlayer *)pAttacker)->m_pVehicle == NULL && m_fNextSpeak < gpGlobals->time )
	{
		m_fNextSpeak = gpGlobals->time + RANDOM_LONG( 5, 8 );

		switch ( RANDOM_LONG( 0, 4 ) )
		{
		case 0:	UTIL_EmitAmbientSound( ENT( pev ), pev->origin, "general/boss_hurt1.wav", 1, ATTN_NONE, 0, 100 );	break;
		case 1:	UTIL_EmitAmbientSound( ENT( pev ), pev->origin, "general/boss_hurt2.wav", 1, ATTN_NONE, 0, 100 );	break;
		case 2:	UTIL_EmitAmbientSound( ENT( pev ), pev->origin, "general/boss_hurt3.wav", 1, ATTN_NONE, 0, 100 );	break;
		case 3:	UTIL_EmitAmbientSound( ENT( pev ), pev->origin, "general/boss_hurt4.wav", 1, ATTN_NONE, 0, 100 );	break;
		case 4:	UTIL_EmitAmbientSound( ENT( pev ), pev->origin, "general/boss_hurt5.wav", 1, ATTN_NONE, 0, 100 );	break;
		}
	}

	if ( m_bHasKatagun )
	{
		if ( FClassnameIs( pevInflictor, "vehicle_tank_shell" ) || FClassnameIs( pevInflictor, "vehicle_tank_rocket" ) )
		{
			// past 15 hits, five hits turn into one more on the kataball count
			m_iEnemyHitMe++;
			if ( m_iEnemyHitMe > 10 && m_iEnemyHitMe - 15 > 0 )
			{
				m_iEnemyHitMe -= 5;
				m_iKataFiring++;
			}

			if ( m_iKataFiring > 4 && m_iEnemyHitMe > 10 )
			{
				// the katagun blows off
				m_bHasKatagun = FALSE;
				m_iEnemyHitMe = 0;
				SetBodygroup( 1, 1 );
				m_flLostKatagunTime = gpGlobals->time + 10;

				Vector vecGun, vecAngles;
				GetAttachment( 3, vecGun, vecAngles );

				CSphereExplosion *pExplosion = CSphereExplosion::CreateSphereExplosion( vecGun, this, 200, 8, 255, 5, 1, FALSE, FALSE, 2.5, 1, 0.5 );
				if ( pExplosion )
					pExplosion->UseToggle( this, this, USE_ON, 0 );

				Create( "spark_shower", vecGun, pev->v_angle, NULL );
				Create( "spark_shower", vecGun, pev->v_angle, NULL );
				Create( "spark_shower", vecGun, pev->v_angle, NULL );
				Create( "spark_shower", vecGun, pev->v_angle, NULL );

				FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
			}
		}
	}
	else
	{
		if ( FClassnameIs( pevAttacker, "monster_gunner_friendly" ) && IsAlive( ) )
			CBaseMonster::TakeDamage( pevAttacker, pevAttacker, 1, DMG_CRUSH );
	}

	if ( pev->health <= 0 )
		SetBodygroup( 1, 2 );

	return 0;
}

void CEndBoss::Killed( entvars_t *pevAttacker, int iGib )
{
	CBaseMonster::Killed( pevAttacker, iGib | GIB_ALWAYS );
}

//=========================================================
// GibMonster - leaves the wreck in a sphere explosion, a
// shower of metal plates and a fireball
//=========================================================
void CEndBoss::GibMonster( void )
{
	CEndBossGib *pGib = GetClassPtr( (CEndBossGib *)NULL );
	pGib->Spawn( );
	pGib->pev->origin = pev->origin;
	pGib->pev->angles = pev->angles;

	CSphereExplosion *pExplosion = CSphereExplosion::CreateSphereExplosion( pev->origin + Vector( 0, 0, 128 ), this, 200, 8, 255, 5, 1, FALSE, FALSE, 2.5, 1, 0.5 );
	if ( pExplosion )
		pExplosion->UseToggle( this, this, USE_ON, 0 );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BREAKMODEL );

		// position
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 128 );

		// size
		WRITE_COORD( 400 );
		WRITE_COORD( 400 );
		WRITE_COORD( 400 );

		// velocity
		WRITE_COORD( pev->velocity.x );
		WRITE_COORD( pev->velocity.y );
		WRITE_COORD( pev->velocity.z + 128 );

		// randomization
		WRITE_BYTE( 50 );

		// Model
		WRITE_SHORT( m_iMetalGibs );	//model id#

		// # of shards
		WRITE_BYTE( 150 );

		// duration
		WRITE_BYTE( 30 );// 3.0 seconds

		// flags
		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END( );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 256 );
		WRITE_SHORT( m_iFireball );
		WRITE_BYTE( 90 ); // scale * 10
		WRITE_BYTE( 255 ); // brightness
	MESSAGE_END( );

	CBaseMonster::GibMonster( );
}

//=========================================================
// GetGunPosition - the machine guns' two barrels, one
// above the other, the rocket launcher and the katagun
//=========================================================
Vector CEndBoss::GetGunPosition( int iGun )
{
	Vector vecPos, vecAngles;

	switch ( iGun )
	{
	case ENDBOSS_GUN_MGUN_LOW:
		GetAttachment( 1, vecPos, vecAngles );
		vecPos = vecPos - Vector( 0, 0, 8 );
		break;
	case ENDBOSS_GUN_MGUN_HIGH:
		GetAttachment( 1, vecPos, vecAngles );
		vecPos = vecPos + Vector( 0, 0, 8 );
		break;
	case ENDBOSS_GUN_ROCKET:
		GetAttachment( 2, vecPos, vecAngles );
		break;
	case ENDBOSS_GUN_KATAGUN:
		GetAttachment( 3, vecPos, vecAngles );
		break;
	}

	return vecPos;
}

//=========================================================
// CheckAttacks - the base's, but the stomp and the machine
// guns need no capability bits, and the machine guns take
// the second melee attack's condition
//=========================================================
void CEndBoss::CheckAttacks( CBaseEntity *pTarget, float flDist )
{
	Vector2D	vec2LOS;
	float		flDot;

	UTIL_MakeVectors( pev->angles );

	vec2LOS = ( pTarget->pev->origin - pev->origin ).Make2D( );
	vec2LOS = vec2LOS.Normalize( );

	flDot = DotProduct( vec2LOS, gpGlobals->v_forward.Make2D( ) );

	// Clear all attack conditions
	ClearConditions( bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_RANGE_ATTACK2 | bits_COND_CAN_MELEE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK2 | bits_COND_SPECIAL1 );

	if ( m_afCapability & bits_CAP_RANGE_ATTACK1 )
	{
		if ( CheckRangeAttack1( flDot, flDist ) )
			SetConditions( bits_COND_CAN_RANGE_ATTACK1 );
	}
	if ( m_afCapability & bits_CAP_RANGE_ATTACK2 )
	{
		if ( CheckRangeAttack2( flDot, flDist ) )
			SetConditions( bits_COND_CAN_RANGE_ATTACK2 );
	}

	if ( CheckMeleeAttack1( flDot, flDist ) )
		SetConditions( bits_COND_CAN_MELEE_ATTACK1 );

	if ( CheckMachinegunAttack( flDot, flDist ) )
		SetConditions( bits_COND_CAN_MELEE_ATTACK2 );
}

// the katagun
BOOL CEndBoss::CheckRangeAttack1( float flDot, float flDist )
{
	if ( m_fNextKataAttack > gpGlobals->time )
		return FALSE;

	if ( !m_bHasKatagun )
		return FALSE;

	m_fNextKataAttack = gpGlobals->time + RANDOM_FLOAT( 2, 5 );
	return TRUE;
}

// the rockets
BOOL CEndBoss::CheckRangeAttack2( float flDot, float flDist )
{
	if ( m_fNextRocketAttack > gpGlobals->time )
		return FALSE;

	m_fNextRocketAttack = gpGlobals->time + RANDOM_FLOAT( 5, 8 );
	return TRUE;
}

BOOL CEndBoss::CheckMachinegunAttack( float flDot, float flDist )
{
	if ( m_fNextMachinegunAttack > gpGlobals->time )
		return FALSE;

	m_fNextMachinegunAttack = gpGlobals->time + RANDOM_FLOAT( 5, 8 );
	return TRUE;
}

// the stomp
BOOL CEndBoss::CheckMeleeAttack1( float flDot, float flDist )
{
	if ( flDot > 0.7 && flDist < 256 )
		return TRUE;

	return FALSE;
}

//=========================================================
// RunAI - aims the machine guns, smokes from the katagun's
// stump, and throws flares at tank rockets that come for it
//=========================================================
void CEndBoss::RunAI( void )
{
	CBaseMonster::RunAI( );

	if ( HasConditions( bits_COND_SEE_ENEMY ) )
	{
		Vector vecShootDir = ShootAtEnemy( GetGunPosition( ENDBOSS_GUN_MGUN_HIGH ) );
		Vector vecAngles = UTIL_VecToAngles( vecShootDir );
		SetBlending( 0, vecAngles.x );
	}
	else
	{
		SetBlending( 0, 0 );
	}

	if ( !m_bHasKatagun )
	{
		Vector vecSrc, vecAngles;
		GetAttachment( 3, vecSrc, vecAngles );

		if ( UTIL_PercentChance( 1 ) )
			Create( "spark_shower", vecSrc, pev->v_angle, NULL );

		if ( gpGlobals->time < m_flLostKatagunTime )
		{
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
				WRITE_BYTE( TE_SMOKE );
				WRITE_COORD( vecSrc.x );
				WRITE_COORD( vecSrc.y );
				WRITE_COORD( vecSrc.z );
				WRITE_SHORT( g_sModelIndexSmoke );
				WRITE_BYTE( RANDOM_LONG( 0, 5 ) + 10 ); // scale * 10
				WRITE_BYTE( 15 ); // framerate
			MESSAGE_END( );
		}
	}

	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, 800 ) ) != NULL )
	{
		if ( !FClassnameIs( pEntity->pev, "vehicle_tank_rocket" ) )
			continue;

		CBaseMonster *pRocket = pEntity->MyMonsterPointer( );
		if ( pRocket && pRocket->m_hEnemy != NULL && pRocket->m_hEnemy == this )
		{
			// throw flares; the last one lures the rocket away
			for ( int i = 5; i > 0; i-- )
			{
				Vector vecFlare, vecFlareAngles;
				GetAttachment( 3, vecFlare, vecFlareAngles );

				CAntirocketflare *pFlare = GetClassPtr( (CAntirocketflare *)NULL );
				// sic: adds i * 16 to a float pointer to the sum, reading the stack past it
				UTIL_SetOrigin( pFlare->pev, vecFlare + gpGlobals->v_forward + i * 16 );
				pFlare->Spawn( );
				pFlare->pev->owner = edict( );
				pFlare->pev->angles.x = pev->angles.x;
				pFlare->pev->angles.y = pev->angles.y;
				pFlare->pev->angles.z = pev->angles.z;
				pFlare->pev->velocity = gpGlobals->v_forward * 512 + gpGlobals->v_up * RANDOM_LONG( -128, 128 ) + gpGlobals->v_right * RANDOM_LONG( -128, 128 );

				pRocket->m_hEnemy = pFlare;
			}
		}
	}
}

//=========================================================
// SetActivity - runs with the walk sequence
//=========================================================
void CEndBoss::SetActivity( Activity NewActivity )
{
	int iSequence;

	if ( NewActivity != ACT_RUN )
		iSequence = LookupActivity( NewActivity );
	else
		iSequence = LookupActivity( ACT_WALK );

	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence = iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
		SetYawSpeed( );
	}
	else
	{
		// Not available try to get default anim
		ALERT( at_console, "%s has no sequence for act:%d\n", STRING( pev->classname ), NewActivity );
		pev->sequence = 0;	// Set to the reset anim (if it's there)
	}
}

//=========================================================
// Spawn
//=========================================================
void CEndBoss::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT( pev ), "models/endboss.mdl" );
	pev->solid = SOLID_BBOX;
	pev->health = 200;
	UTIL_SetSize( pev, Vector( -48, -48, 0 ), Vector( 48, 48, 300 ) );

	m_fNextMachinegunAttack = m_fNextKataAttack = m_fNextRocketAttack = gpGlobals->time;
	m_fNextSpeak = gpGlobals->time;
	m_bHasKatagun = TRUE;

	MonsterInit( );

	m_flFieldOfView = VIEW_FIELD_FULL;
	pev->view_ofs = Vector( 0, 0, 300 );
	m_flDistTooFar = 4096;
	m_iEnemyHitMe = 0;
	m_iKataFiring = 0;
}

// the boss is bigger than its hull
void CEndBoss::SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin - Vector( 256, 256, 0 );
	pev->absmax = pev->origin + Vector( 256, 256, 400 );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CEndBoss::Precache( void )
{
	PRECACHE_MODEL( "models/endboss.mdl" );

	UTIL_PrecacheOther( "endboss_kataball" );
	UTIL_PrecacheOther( "endboss_rocket" );
	UTIL_PrecacheOther( "antirocketflare" );
	UTIL_PrecacheOther( "endboss_gib" );
	UTIL_PrecacheOther( "sphere_explosion" );

	m_usTankMGuns = PRECACHE_EVENT( 1, "events/tankmguns.sc" );
	m_iFireball = PRECACHE_MODEL( "sprites/fexplo.spr" );
	m_iMetalGibs = PRECACHE_MODEL( "models/metalplategibs_green.mdl" );

	CBaseMonster::Precache( );
}
