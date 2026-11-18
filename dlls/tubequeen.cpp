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
// monster_tubequeen: the tube queen, the rust chapter's boss.
// She sits in her pit, lobs tube mortars at her enemy, slaps
// with her claws when a trigger uses her, pulls tubes out of
// her belly and climbs away when she has had enough.
// tubequeen_mortar: her mortar, which bursts into a spray of
// tube rockets where it lands.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "squadmonster.h"
#include "weapons.h"
#include "soundent.h"
#include "tube.h"

//=========================================================
// CTubeMortar - the queen's mortar: a wiggling tube with a
// beam trail that bursts into tube rockets on impact
//=========================================================
class CTubeMortar : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void GibMonster( void );

	static CTubeMortar *Create( void );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int		m_iModelIndexTubeSuicide;
	int		m_iTrail;
	int		m_bViolentFlinch;	// set in Spawn, never read

private:
	void EXPORT MortarThink( void );
	void EXPORT DecoreTouch( CBaseEntity *pOther );
};

LINK_ENTITY_TO_CLASS( tubequeen_mortar, CTubeMortar );

TYPEDESCRIPTION	CTubeMortar::m_SaveData[] =
{
	DEFINE_FIELD( CTubeMortar, m_iModelIndexTubeSuicide, FIELD_INTEGER ),
	DEFINE_FIELD( CTubeMortar, m_bViolentFlinch, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CTubeMortar, CBaseMonster );

int CTubeMortar::Classify( void )
{
	return CLASS_ALIEN_BIOWEAPON;
}

void CTubeMortar::Precache( void )
{
	m_iModelIndexTubeSuicide = PRECACHE_MODEL( "sprites/tubeguts.spr" );
	m_iTrail = PRECACHE_MODEL( "sprites/laserbeam.spr" );
	PRECACHE_MODEL( "models/tubemortar.mdl" );
	PRECACHE_SOUND( "tubequeen/tq_mortarhit1.wav" );
	PRECACHE_SOUND( "tubequeen/tq_mortarhit2.wav" );
	UTIL_PrecacheOther( "tuberocket" );
}

CTubeMortar *CTubeMortar::Create( void )
{
	CTubeMortar *pMortar = GetClassPtr( (CTubeMortar *)NULL );

	pMortar->pev->classname = MAKE_STRING( "tubequeen_mortar" );
	pMortar->Spawn( );

	return pMortar;
}

void CTubeMortar::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT(pev), "models/tubemortar.mdl" );
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	UTIL_SetSize( pev, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ) );
	UTIL_SetOrigin( pev, pev->origin );

	// beam trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex( ) );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 5 );	// life
		WRITE_BYTE( 8 );	// width
		WRITE_BYTE( 32 );	// r, g, b
		WRITE_BYTE( 128 );	// r, g, b
		WRITE_BYTE( 16 );	// r, g, b
		WRITE_BYTE( 200 );	// brightness
	MESSAGE_END( );

	pev->takedamage = DAMAGE_YES;
	pev->health = 1;

	pev->nextthink = gpGlobals->time + 0.1;
	SetTouch( &CTubeMortar::DecoreTouch );
	pev->nextthink = gpGlobals->time + 0.1;
	SetThink( &CTubeMortar::MortarThink );

	SetSequenceByName( "wiggle" );
	ResetSequenceInfo( );

	m_bViolentFlinch = 1;
	pev->view_ofs = Vector( 0, 0, 80 );
}

//=========================================================
// MortarThink - wiggles and points along its flight
//=========================================================
void CTubeMortar::MortarThink( void )
{
	StudioFrameAdvance( );
	pev->angles = UTIL_VecToAngles( pev->velocity );
	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// DecoreTouch - bursts into a spray of tube rockets; the
// rest is the sitting tube mortar's PlayerTouch
//=========================================================
void CTubeMortar::DecoreTouch( CBaseEntity *pOther )
{
	Vector vecDir, vecSrc, vecAngles;

	if ( RANDOM_LONG( 0, 1 ) )
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "tubequeen/tq_mortarhit1.wav", 1, 0.8, 0, 100 );
	else
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "tubequeen/tq_mortarhit2.wav", 1, 0.8, 0, 100 );

	pev->takedamage = DAMAGE_NO;

	switch ( g_iSkillLevel )
	{
	case SKILL_MEDIUM:
		::RadiusDamage( pev->origin, pev, NULL, 90, 140, CLASS_ALIEN_BIOWEAPON, DMG_GENERIC );
		break;
	case SKILL_HARD:
		::RadiusDamage( pev->origin, pev, NULL, 100, 180, CLASS_ALIEN_BIOWEAPON, DMG_GENERIC );
		break;
	default:
		::RadiusDamage( pev->origin, pev, NULL, 70, 80, CLASS_ALIEN_BIOWEAPON, DMG_GENERIC );
		break;
	}

	int iRockets = RANDOM_LONG( 8, 12 );
	for ( int i = 0; i < iRockets; i++ )
	{
		vecDir = m_vecEnemyLKP - pev->origin;
		vecDir = vecDir.Normalize( );
		vecAngles = UTIL_VecToAngles( vecDir );	// sic: never used
		vecSrc = pev->origin + vecDir * 25;
		vecSrc.z += 16;
		vecDir.z = vecDir.z * -1;

		CBaseEntity *pRocket = CBaseEntity::Create( "tuberocket", vecSrc, UTIL_VecToAngles( vecDir ), edict( ) );
		UTIL_MakeVectors( pRocket->pev->angles );
		pRocket->pev->velocity = gpGlobals->v_forward * 300;

		CBaseMonster *pMonster = pRocket->MyMonsterPointer( );
		if ( pMonster )
			pMonster->m_hEnemy = m_hEnemy;

		// sic: the rest goes through pMonster without testing it
		vecDir.x = RANDOM_FLOAT( -0.5, 0.5 );
		vecDir.y = RANDOM_FLOAT( -0.5, 0.5 );
		vecDir.z = RANDOM_FLOAT( 0.1, 0.5 );
		vecDir = vecDir.Normalize( );
		pMonster->pev->angles = UTIL_VecToAngles( vecDir );
		UTIL_MakeAimVectors( pMonster->pev->angles );
		pMonster->pev->velocity = gpGlobals->v_forward * 300;
		pMonster->pev->origin.x += RANDOM_FLOAT( -20, 20 );
		pMonster->pev->origin.y += RANDOM_FLOAT( -20, 20 );
		pMonster->pev->origin.z += RANDOM_FLOAT( 32, 64 );

		if ( m_MonsterState == MONSTERSTATE_COMBAT )
		{
			pMonster->m_vecEnemyLKP = m_vecEnemyLKP;
			pMonster->m_hEnemy = m_hEnemy;
		}

		pMonster->m_flNextAttack = gpGlobals->time + RANDOM_FLOAT( 0.3, 0.6 );
	}

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( m_iModelIndexTubeSuicide );
		WRITE_BYTE( 10 );	// scale * 10
		WRITE_BYTE( 15 );	// framerate
		WRITE_BYTE( TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND );
	MESSAGE_END( );

	pev->health = -1;
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// TakeDamage - its own blast doesn't hurt it
//=========================================================
int CTubeMortar::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( pev == pevInflictor )
		return 0;

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_BULLET );
}

void CTubeMortar::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if ( pev == pevAttacker )
		return;

	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType | DMG_BULLET );

	if ( pev->health < 0 )
		GibMonster( );
}

void CTubeMortar::GibMonster( void )
{
	DecoreTouch( NULL );
}

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_TUBEQUEEN_CLIMB_AWAY = LAST_COMMON_TASK + 1,
	TASK_TUBEQUEEN_VIOLENT_FLINCH,
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define TUBEQUEEN_AE_SHAKE				( 1 )
#define TUBEQUEEN_AE_CLAW_SOUND			( 2 )
#define TUBEQUEEN_AE_CLAW_LEFT_SOUND	( 4 )
#define TUBEQUEEN_AE_CLAW_RIGHT_SOUND	( 6 )
#define TUBEQUEEN_AE_GRAB_TUBE			( 9 )
#define TUBEQUEEN_AE_PULL_TUBE			( 10 )
#define TUBEQUEEN_AE_SMALL_FLINCH		( 11 )
#define TUBEQUEEN_AE_BIG_FLINCH			( 12 )
#define TUBEQUEEN_AE_VIOLENT_FLINCH		( 13 )
#define TUBEQUEEN_AE_IDLE1				( 14 )
#define TUBEQUEEN_AE_IDLE2				( 15 )
#define TUBEQUEEN_AE_INSPECT_BELLY		( 16 )
#define TUBEQUEEN_AE_CLAW_SOUND2		( 17 )
#define TUBEQUEEN_AE_CLAW_LEFT_SOUND2	( 18 )
#define TUBEQUEEN_AE_CLAW_RIGHT_SOUND2	( 19 )
#define TUBEQUEEN_AE_GRAB_TUBE_SOUND	( 20 )
#define TUBEQUEEN_AE_REMOVE				( 30 )

class CTubeQueen : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int  ObjectCaps( void ) { return CBaseMonster::ObjectCaps( ) | FCAP_MASTER; }
	void RunAI( void )
	{
		Vector vecLeftClaw, vecRightClaw, vecAngles;

		CBaseMonster::RunAI( );

		GetAttachment( 2, vecLeftClaw, vecAngles );
		GetAttachment( 1, vecRightClaw, vecAngles );

		// the claws hurt what is near them from the seventh think
		// of a slap on
		if ( pev->sequence == LookupSequence( "clawattackleft" ) )
		{
			if ( ++m_iSlapTime > 6 )
				::RadiusDamage( vecLeftClaw, pev, pev, 100, 80, CLASS_NONE, DMG_GENERIC );
		}
		else if ( pev->sequence == LookupSequence( "clawattackright" ) )
		{
			if ( ++m_iSlapTime > 6 )
				::RadiusDamage( vecRightClaw, pev, pev, 100, 80, CLASS_NONE, DMG_GENERIC );
		}
		else if ( pev->sequence == LookupSequence( "clawattack" ) )
		{
			if ( ++m_iSlapTime > 6 )
			{
				::RadiusDamage( vecLeftClaw, pev, pev, 100, 80, CLASS_NONE, DMG_GENERIC );
				::RadiusDamage( vecRightClaw, pev, pev, 100, 80, CLASS_NONE, DMG_GENERIC );
			}
		}
		else
		{
			m_iSlapTime = 0;
		}
	}
	int  MonsterType( void ) { return 24; }
	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin - Vector( 1024, 1024, 0 );
		pev->absmax = pev->origin + Vector( 1024, 1024, 2048 );
	}
	int  Classify( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	Schedule_t *GetSchedule( void );
	BOOL CheckRangeAttack1( float flDot, float flDist );

	void Unused1( void );
	void Unused2( int iUnused );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

	float	m_fNextMortarAttack;
	int		m_iSlapTime;		// thinks into the current slap
	int		m_bViolentFlinch;	// the violent flinch is still to come
	float	m_flNextFlinch;

private:
	// UseQueen - a trigger named after a claw makes her slap
	// with it, when she is idle
	void EXPORT UseQueen( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		if ( pev->sequence == LookupSequence( "idle1" ) || pev->sequence == LookupSequence( "idle2" ) )
		{
			if ( FStrEq( STRING( pCaller->pev->targetname ), "clawcenter" ) )
				SetSequenceByName( "clawattack" );
			else if ( FStrEq( STRING( pCaller->pev->targetname ), "clawleft" ) )
				SetSequenceByName( "clawattackleft" );
			else if ( FStrEq( STRING( pCaller->pev->targetname ), "clawright" ) )
				SetSequenceByName( "clawattackright" );

			ResetSequenceInfo( );
		}
	}
};

LINK_ENTITY_TO_CLASS( monster_tubequeen, CTubeQueen );

TYPEDESCRIPTION	CTubeQueen::m_SaveData[] =
{
	DEFINE_FIELD( CTubeQueen, m_fNextMortarAttack, FIELD_TIME ),
	DEFINE_FIELD( CTubeQueen, m_bViolentFlinch, FIELD_INTEGER ),
	DEFINE_FIELD( CTubeQueen, m_iSlapTime, FIELD_INTEGER ),
	DEFINE_FIELD( CTubeQueen, m_flNextFlinch, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CTubeQueen, CBaseMonster );

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlTubeQueenPullTube[] =
{
	{ TASK_STOP_MOVING,			(float)0						},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_SPECIAL_ATTACK1		},
	{ TASK_SET_SCHEDULE,		(float)SCHED_IDLE_STAND			},
};

Schedule_t	slTubeQueenPullTube[] =
{
	{
		tlTubeQueenPullTube,
		ARRAYSIZE ( tlTubeQueenPullTube ),
		0,
		0,
		"pulling out a tube"
	},
};

Task_t	tlTubeQueenClimbAway[] =
{
	{ TASK_STOP_MOVING,				(float)0		},
	{ TASK_TUBEQUEEN_CLIMB_AWAY,	(float)0		},
};

Schedule_t	slTubeQueenClimbAway[] =
{
	{
		tlTubeQueenClimbAway,
		ARRAYSIZE ( tlTubeQueenClimbAway ),
		0,
		0,
		"plays the death animation"
	},
};

Task_t	tlTubeQueenViolentFlinch[] =
{
	{ TASK_STOP_MOVING,					(float)0		},
	{ TASK_TUBEQUEEN_VIOLENT_FLINCH,	(float)0		},
};

Schedule_t	slTubeQueenViolentFlinch[] =
{
	{
		tlTubeQueenViolentFlinch,
		ARRAYSIZE ( tlTubeQueenViolentFlinch ),
		0,
		0,
		"plays the violent flinch animation"
	},
};

DEFINE_CUSTOM_SCHEDULES( CTubeQueen )
{
	slTubeQueenPullTube,
	slTubeQueenClimbAway,
	slTubeQueenViolentFlinch,
};

IMPLEMENT_CUSTOM_SCHEDULES( CTubeQueen, CBaseMonster );

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CTubeQueen::Classify( void )
{
	return CLASS_ALIEN_TUBE;
}

//=========================================================
// SetYawSpeed - she doesn't turn
//=========================================================
void CTubeQueen::SetYawSpeed( void )
{
	pev->yaw_speed = 0;
}

// two empty members nothing calls
void CTubeQueen::Unused1( void )
{
}

void CTubeQueen::Unused2( int iUnused )
{
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CTubeQueen::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case TUBEQUEEN_AE_SHAKE:
		UTIL_ScreenShake( pev->origin, 24, 7, 0.5, 2400 );
		break;

	case TUBEQUEEN_AE_CLAW_SOUND:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_clawattack.wav", 1, 0.8, 0, 90 );
		break;

	case 3:
	case 5:
	case 7:
	case 8:
		break;

	case TUBEQUEEN_AE_CLAW_LEFT_SOUND:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_clawattack_left.wav", 1, 0.8, 0, 90 );
		break;

	case TUBEQUEEN_AE_CLAW_RIGHT_SOUND:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_clawattack_right.wav", 1, 0.8, 0, 90 );
		break;

	case TUBEQUEEN_AE_GRAB_TUBE:
		SetBodygroup( 1, 1 );
		break;

	case TUBEQUEEN_AE_PULL_TUBE:
		{
			// sic: the blast and the new tube are at a fixed spot in
			// the queen's map
			::RadiusDamage( Vector( 563, 147, -580 ), pev, NULL, 1000, 64, CLASS_NONE, DMG_CRUSH );

			CTube *pTube = GetClassPtr( (CTube *)NULL );
			pTube->pev->classname = MAKE_STRING( "monster_tube" );
			pTube->pev->owner = edict( );
			pTube->Spawn( );
			pTube->pev->velocity = g_vecZero;
			pTube->pev->angles = pev->angles;
			pTube->pev->solid = SOLID_BBOX;
			UTIL_SetOrigin( pTube->pev, Vector( 563, 147, -580 ) );

			SetBodygroup( 1, 0 );
		}
		break;

	case TUBEQUEEN_AE_SMALL_FLINCH:
		switch ( RANDOM_LONG( 0, 1 ) )
		{
		case 0:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_smallflinch.wav", 1, 0.8, 0, 90 );
			break;
		case 1:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_smallflinch2.wav", 1, 0.8, 0, 90 );
			break;
		}
		break;

	case TUBEQUEEN_AE_BIG_FLINCH:
		switch ( RANDOM_LONG( 0, 1 ) )
		{
		case 0:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_bigflinch.wav", 1, 0.8, 0, 90 );
			break;
		case 1:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_bigflinch2.wav", 1, 0.8, 0, 90 );
			break;
		}
		break;

	case TUBEQUEEN_AE_VIOLENT_FLINCH:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/flinchviolent.wav", 1, 0.8, 0, 90 );
		break;

	case TUBEQUEEN_AE_IDLE1:
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_idle1.wav", 1, 0.8, 0, 90 );
			break;
		case 1:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_idle1a.wav", 1, 0.8, 0, 90 );
			break;
		case 2:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_idle1b.wav", 1, 0.8, 0, 90 );
			break;
		}
		break;

	case TUBEQUEEN_AE_IDLE2:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_idle2.wav", 1, 0.8, 0, 90 );
		break;

	case TUBEQUEEN_AE_INSPECT_BELLY:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_inspectbelly.wav", 1, 0.8, 0, 90 );
		break;

	case TUBEQUEEN_AE_CLAW_SOUND2:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_clawattack.wav", 1, 0.8, 0, 90 );
		break;

	case TUBEQUEEN_AE_CLAW_LEFT_SOUND2:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_clawattack_left.wav", 1, 0.8, 0, 90 );
		break;

	case TUBEQUEEN_AE_CLAW_RIGHT_SOUND2:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_clawattack_right.wav", 1, 0.8, 0, 90 );
		break;

	case TUBEQUEEN_AE_GRAB_TUBE_SOUND:
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_grabtube1.wav", 1, 0.8, 0, 90 );
			break;
		case 1:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_grabtube2.wav", 1, 0.8, 0, 90 );
			break;
		case 2:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_grabtube3.wav", 1, 0.8, 0, 90 );
			break;
		}
		break;

	case TUBEQUEEN_AE_REMOVE:
		UTIL_Remove( this );
		break;

	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// StartTask
//=========================================================
void CTubeQueen::StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_TUBEQUEEN_CLIMB_AWAY:
		ALERT( at_console, "Breaking Queen name, can't be scripted anymore\n" );
		pev->targetname = 0;
		SetSequenceByName( "climbaway" );
		ResetSequenceInfo( );
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_climbaway.wav", 1, 0.8, 0, 90 );
		UTIL_ScreenShake( pev->origin, 24, 7, 8, 3600 );
		FireTargets( "tubeblocker", this, this, USE_TOGGLE, 0 );
		break;

	case TASK_TUBEQUEEN_VIOLENT_FLINCH:
		UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_flinchviolent.wav", 1, 0.8, 0, 90 );
		SetSequenceByName( "flinchviolent" );
		ResetSequenceInfo( );
		break;
	}

	CBaseMonster::StartTask( pTask );
}

//=========================================================
// RunTask
//=========================================================
void CTubeQueen::RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_TUBEQUEEN_CLIMB_AWAY:
		if ( m_fSequenceFinished )
		{
			UTIL_Remove( this );
			return;
		}
		// fall through: the next test fails too

	case TASK_TUBEQUEEN_VIOLENT_FLINCH:
		if ( m_fSequenceFinished )
		{
			TaskComplete( );
			return;
		}
		// fall through

	default:
		CBaseMonster::RunTask( pTask );
		break;
	}
}

//=========================================================
// ISoundMask - she hears nothing
//=========================================================
int CTubeQueen::ISoundMask( void )
{
	return bits_SOUND_NONE;
}

//=========================================================
// Spawn
//=========================================================
void CTubeQueen::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT(pev), "models/tubequeen.mdl" );
	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 24 ) );

	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;

	// sic: she is tougher on medium than on hard
	switch ( g_iSkillLevel )
	{
	case SKILL_MEDIUM:
		pev->health = 1200;
		break;
	case SKILL_HARD:
		pev->health = 900;
		break;
	default:
		pev->health = 800;
		break;
	}

	m_bloodColor = BLOOD_COLOR_RED;
	m_flFieldOfView = 0;
	m_MonsterState = MONSTERSTATE_IDLE;
	pev->takedamage = DAMAGE_YES;
	m_flNextFlinch = m_fNextMortarAttack = gpGlobals->time;

	UTIL_SetOrigin( pev, pev->origin );
	m_iSlapTime = 0;

	MonsterInit( );

	SetUse( &CTubeQueen::UseQueen );
	m_afCapability |= bits_CAP_RANGE_ATTACK1;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CTubeQueen::Precache( void )
{
	PRECACHE_MODEL( "models/tubequeen.mdl" );

	PRECACHE_SOUND( "tubequeen/tq_bigflinch.wav" );
	PRECACHE_SOUND( "tubequeen/tq_bigflinch2.wav" );
	PRECACHE_SOUND( "tubequeen/tq_clawattack.wav" );
	PRECACHE_SOUND( "tubequeen/tq_clawattack_left.wav" );
	PRECACHE_SOUND( "tubequeen/tq_clawattack_right.wav" );
	PRECACHE_SOUND( "tubequeen/tq_climbaway.wav" );
	PRECACHE_SOUND( "tubequeen/tq_flinchviolent.wav" );
	PRECACHE_SOUND( "tubequeen/tq_grabtube1.wav" );
	PRECACHE_SOUND( "tubequeen/tq_grabtube2.wav" );
	PRECACHE_SOUND( "tubequeen/tq_grabtube3.wav" );
	PRECACHE_SOUND( "tubequeen/tq_idle1.wav" );
	PRECACHE_SOUND( "tubequeen/tq_idle1a.wav" );
	PRECACHE_SOUND( "tubequeen/tq_idle1b.wav" );
	PRECACHE_SOUND( "tubequeen/tq_idle2.wav" );
	PRECACHE_SOUND( "tubequeen/tq_inspectbelly.wav" );
	PRECACHE_SOUND( "tubequeen/tq_smallflinch.wav" );
	PRECACHE_SOUND( "tubequeen/tq_smallflinch2.wav" );
	PRECACHE_SOUND( "tubequeen/tq_mortarfire1.wav" );
	PRECACHE_SOUND( "tubequeen/tq_mortarfire2.wav" );
	PRECACHE_SOUND( "tubequeen/tq_mortarfire3.wav" );

	UTIL_PrecacheOther( "monster_tube" );
	UTIL_PrecacheOther( "tubequeen_mortar" );

	g_iGoreBirth = PRECACHE_MODEL( "sprites/gorebirth.spr" );
	g_iGibBirth = PRECACHE_MODEL( "sprites/gibbirth.spr" );
}

//=========================================================
// Killed - she doesn't die; she climbs away
//=========================================================
void CTubeQueen::Killed( entvars_t *pevAttacker, int iGib )
{
}

//=========================================================
// TakeDamage - her own blasts don't hurt her, shock and
// energy weapons only a tenth; her skin shows the wounds
//=========================================================
int CTubeQueen::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( !FClassnameIs( pevAttacker, "monster_tubequeen" ) && !FClassnameIs( pevInflictor, "monster_tubequeen" ) )
	{
		if ( bitsDamageType & ( DMG_SHOCK | DMG_ENERGYBEAM ) )
		{
			ALERT( at_console, "taking 10% damage from this gun\n" );
			flDamage *= 0.1;
		}

		CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

		if ( pev->max_health / 3 > pev->health )
			pev->skin = 2;
		else if ( pev->health < pev->max_health / 1.5 )
			pev->skin = 1;

		return 1;
	}

	return 0;
}

//=========================================================
// GetSchedule
//=========================================================
Schedule_t *CTubeQueen::GetSchedule( void )
{
	if ( pev->health < 0 )
		return &slTubeQueenClimbAway[ 0 ];

	if ( gpGlobals->time > m_flNextFlinch )
	{
		if ( pev->max_health / 3 > pev->health && m_bViolentFlinch )
		{
			m_bViolentFlinch = FALSE;
			m_flNextFlinch = gpGlobals->time + RANDOM_LONG( 8, 12 );
			return &slTubeQueenViolentFlinch[ 0 ];
		}

		if ( HasConditions( bits_COND_HEAVY_DAMAGE ) )
		{
			m_flNextFlinch = gpGlobals->time + RANDOM_LONG( 8, 12 );
			return GetScheduleOfType( SCHED_BIG_FLINCH );
		}

		if ( HasConditions( bits_COND_LIGHT_DAMAGE ) && !HasMemory( bits_MEMORY_FLINCHED ) )
		{
			Remember( bits_MEMORY_FLINCHED );
			m_flNextFlinch = gpGlobals->time + RANDOM_LONG( 8, 12 );
			return GetScheduleOfType( SCHED_SMALL_FLINCH );
		}
	}

	if ( m_hEnemy != NULL )
	{
		if ( pev->sequence == LookupSequence( "idle1" ) || pev->sequence == LookupSequence( "idle2" ) )
			return &slTubeQueenPullTube[ 0 ];
	}

	return CBaseMonster::GetScheduleOfType( SCHED_IDLE_STAND );
}

//=========================================================
// CheckRangeAttack1 - lobs a mortar from her mouth or from
// either side of it at her enemy, 256 to 1024 units away
//=========================================================
BOOL CTubeQueen::CheckRangeAttack1( float flDot, float flDist )
{
	if ( flDist <= 256 )
		return FALSE;

	if ( flDist >= 1024 )
		return FALSE;

	if ( m_fNextMortarAttack > gpGlobals->time )
		return FALSE;

	Vector vecSrc, vecAngles, vecRight;
	Vector vecTarget = m_vecEnemyLKP;

	if ( RANDOM_LONG( 0, 1 ) )
		vecTarget = Vector( m_hEnemy->pev->origin.x, m_hEnemy->pev->origin.y, m_hEnemy->pev->absmin.z );

	GetAttachment( 0, vecSrc, vecAngles );
	g_engfuncs.pfnAngleVectors( pev->angles, NULL, vecRight, NULL );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 1:
		vecSrc = vecSrc + vecRight * 68;
		break;
	case 2:
		vecSrc = vecSrc + vecRight * -68;
		break;
	}

	Vector vecToss = VecCheckToss( pev, vecSrc, vecTarget, 1.0 );

	if ( vecToss != g_vecZero )
	{
		// sic: the third sound never plays
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_mortarfire1.wav", 1, 0.8, 0, 90 );
			break;
		case 1:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_mortarfire2.wav", 1, 0.8, 0, 90 );
			break;
		case 3:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "tubequeen/tq_mortarfire3.wav", 1, 0.8, 0, 90 );
			break;
		}

		UTIL_MakeAimVectors( pev->angles );

		CTubeMortar *pMortar = CTubeMortar::Create( );
		UTIL_SetOrigin( pMortar->pev, vecSrc );
		pMortar->pev->angles = pev->angles;
		pMortar->pev->velocity = vecToss;
		pMortar->pev->gravity = 1.0;
		pMortar->pev->owner = edict( );
		pMortar->m_hEnemy = m_hEnemy;

		m_fNextMortarAttack = gpGlobals->time + RANDOM_FLOAT( 2, 4 );
	}

	// sic: the mortar is already away, so the range attack itself
	// never comes up
	return FALSE;
}
