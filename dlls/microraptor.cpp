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
// monster_microraptor: the small mayan raptor. A squad
// monster that bites and leaps at its enemy and eats what
// it smells when it is calm; a smaller, simpler cousin of
// monster_raptor, with which it shares its schedules.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "squadmonster.h"
#include "soundent.h"

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_MRAPTOR_EAT = LAST_COMMON_SCHEDULE + 1,
	SCHED_MRAPTOR_PRETEND_EAT,
};

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_MRAPTOR_EAT = LAST_COMMON_TASK + 1,	// face the food and chew until interrupted
	TASK_MRAPTOR_THREAT_DISPLAY,				// roar, if it hasn't for a while
	TASK_MRAPTOR_UNUSED,						// no code handles it
	TASK_MRAPTOR_GET_TO_LEADER,					// walk up to the squad leader
	TASK_MRAPTOR_LEAP_AWAY,						// leap off in a random direction
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define MRAPTOR_AE_BITE		( 1 )
#define MRAPTOR_AE_CLAW		( 2 )
#define MRAPTOR_AE_LEAP		( 3 )
#define MRAPTOR_AE_LAND		( 4 )

#define MRAPTOR_DMG_BITE	5
#define MRAPTOR_DMG_JUMP	3

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlMRaptorMeleeAttack[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_MELEE_ATTACK1,		(float)0		},
};

Schedule_t	slMRaptorMeleeAttack[] =
{
	{
		tlMRaptorMeleeAttack,
		ARRAYSIZE ( tlMRaptorMeleeAttack ),
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_ENEMY_OCCLUDED,
		0,
		"Raptor Primary Melee Attack"
	},
};

// leap away from a dangerous sound, then take cover
Task_t	tlMRaptorUrgent[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_CHASE_ENEMY_FAILED		},
	{ TASK_MRAPTOR_LEAP_AWAY,	(float)0							},
	{ TASK_SET_SCHEDULE,		(float)SCHED_TAKE_COVER_FROM_ORIGIN	},
};

Schedule_t	slMRaptorUrgent[] =
{
	{
		tlMRaptorUrgent,
		ARRAYSIZE ( tlMRaptorUrgent ),
		0,
		0,
		"Urgent"
	},
};

// walk to the food and eat it
Task_t	tlMRaptorEat[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_MRAPTOR_PRETEND_EAT	},
	{ TASK_STOP_MOVING,				(float)0							},
	{ TASK_STORE_LASTPOSITION,		(float)0							},
	{ TASK_GET_PATH_TO_BESTSCENT,	(float)0							},
	{ TASK_WALK_PATH,				(float)0							},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0							},
	{ TASK_MRAPTOR_EAT,				(float)0							},
};

Schedule_t	slMRaptorEat[] =
{
	{
		tlMRaptorEat,
		ARRAYSIZE ( tlMRaptorEat ),
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_HEAR_SOUND,
		bits_SOUND_COMBAT			|
		bits_SOUND_CARCASS			|
		bits_SOUND_MEAT				|
		bits_SOUND_DANGER,
		"Eating"
	},
};

// the food can't be reached: go stand by the squad leader and chew
Task_t	tlMRaptorPretendEat[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_IDLE_STAND			},
	{ TASK_STOP_MOVING,				(float)0						},
	{ TASK_STORE_LASTPOSITION,		(float)0						},
	{ TASK_MRAPTOR_GET_TO_LEADER,	(float)50						},
	{ TASK_MRAPTOR_EAT,				(float)0						},
};

Schedule_t	slMRaptorPretendEat[] =
{
	{
		tlMRaptorPretendEat,
		ARRAYSIZE ( tlMRaptorPretendEat ),
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_HEAR_SOUND,
		bits_SOUND_COMBAT			|
		bits_SOUND_CARCASS			|
		bits_SOUND_MEAT				|
		bits_SOUND_DANGER,
		"Pretending to Eat"
	},
};

// failed with an enemy in view: face it and roar
Task_t	tlMRaptorCombatFail[] =
{
	{ TASK_STOP_MOVING,				(float)0		},
	{ TASK_FACE_ENEMY,				(float)0		},
	{ TASK_MRAPTOR_THREAT_DISPLAY,	(float)0		},
};

Schedule_t	slMRaptorCombatFail[] =
{
	{
		tlMRaptorCombatFail,
		ARRAYSIZE ( tlMRaptorCombatFail ),
		bits_COND_NEW_ENEMY			|
		bits_COND_CAN_ATTACK		|
		bits_COND_HEAVY_DAMAGE,
		bits_SOUND_DANGER			|
		bits_SOUND_PLAYER,
		"Failed with PVS"
	},
};

// failed: roar, then wait for a player to come into view
Task_t	tlMRaptorFail[] =
{
	{ TASK_STOP_MOVING,				(float)0		},
	{ TASK_MRAPTOR_THREAT_DISPLAY,	(float)0		},
	{ TASK_WAIT_FACE_ENEMY,			(float)1		},
	{ TASK_WAIT_PVS,				(float)0		},
};

Schedule_t	slMRaptorFail[] =
{
	{
		tlMRaptorFail,
		ARRAYSIZE ( tlMRaptorFail ),
		bits_COND_NEW_ENEMY			|
		bits_COND_CAN_ATTACK		|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER			|
		bits_SOUND_PLAYER,
		"Failed with PVS"
	},
};

class CMicroRaptor : public CSquadMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void PrecacheSounds( void );
	int  Classify( void ) { return CLASS_ALIEN_PREDATOR; }
	int  MonsterType( void ) { return 9; }
	void SetYawSpeed( void );
	int  ISoundMask( void );
	void RunAI( void );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckMeleeAttack2( float flDot, float flDist );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	void IdleSound( void );
	void AlertSound( void );
	void EXPORT JumpTouch( CBaseEntity *pOther );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

	static const char *pMicroRaptorSounds[];

	int		m_iRelaxed;			// no enemy since the last one died: it may go after food
	float	m_tNextSoundTime;	// chewing and threat display wait until then
	float	m_tNextMeleeTime;	// set in Spawn, never read
	int		m_iUnused[4];		// never used
	float	m_flNextJumpTime;	// the leap attack waits until then
	int		m_iUnused2[2];		// never used
};

LINK_ENTITY_TO_CLASS( monster_microraptor, CMicroRaptor );

TYPEDESCRIPTION	CMicroRaptor::m_SaveData[] =
{
	DEFINE_FIELD( CMicroRaptor, m_tNextMeleeTime, FIELD_TIME ),
	DEFINE_FIELD( CMicroRaptor, m_tNextSoundTime, FIELD_TIME ),
	DEFINE_FIELD( CMicroRaptor, m_iRelaxed, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CMicroRaptor, CSquadMonster );

DEFINE_CUSTOM_SCHEDULES( CMicroRaptor )
{
	slMRaptorMeleeAttack,
	slMRaptorEat,
	slMRaptorPretendEat,
	slMRaptorUrgent,
	slMRaptorCombatFail,
	slMRaptorFail,
};

IMPLEMENT_CUSTOM_SCHEDULES( CMicroRaptor, CSquadMonster );

const char *CMicroRaptor::pMicroRaptorSounds[] =
{
	"raptor/Raptor_BigFlinch_F0.wav",
	"raptor/Raptor_BiteChunk_F0.wav",
	"raptor/Raptor_ChewChunk_F0.wav",
	"raptor/Raptor_DieFight.wav",
	"raptor/Raptor_DieSimple.wav",
	"raptor/Raptor_FootStep1.wav",
	"raptor/Raptor_FootStep2.wav",
	"raptor/Raptor_IdleChitter_F0.wav",
	"raptor/Raptor_IdleNormal_F0.wav",
	"raptor/Raptor_IdleNormal_F02.wav",
	"raptor/Raptor_LeapStartMiddleEnd_F0.wav",
	"raptor/Raptor_RoarAngry_F0.wav",
	"raptor/Raptor_Run_F0.wav",
	"raptor/Raptor_SeePlayer.wav",
	"raptor/Raptor_SeePlayer2.wav",
	"raptor/Raptor_SmallFlinch_F0.wav",
};

void CMicroRaptor::PrecacheSounds( void )
{
	for ( int i = 0; i < ARRAYSIZE( pMicroRaptorSounds ); i++ )
		PRECACHE_SOUND( (char *)pMicroRaptorSounds[i] );
}

//=========================================================
// IdleSound - sic: two times in five it stays quiet
//=========================================================
void CMicroRaptor::IdleSound( void )
{
	switch ( RANDOM_LONG( 0, 4 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "raptor/Raptor_IdleChitter_F0.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "raptor/Raptor_IdleNormal_F0.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		break;
	case 2:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "raptor/Raptor_IdleNormal_F02.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		break;
	}
}

void CMicroRaptor::AlertSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "raptor/Raptor_SeePlayer.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "raptor/Raptor_SeePlayer2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		break;
	}
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CMicroRaptor::SetYawSpeed( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
		ys = 40;
		break;
	case ACT_WALK:
		ys = 90;
		break;
	case ACT_RUN:
		ys = 120;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 160;
		break;
	case ACT_LEAP:
		ys = 0;
		break;
	default:
		ys = 100;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// CheckMeleeAttack1 - the bite, at an enemy on the ground
//=========================================================
BOOL CMicroRaptor::CheckMeleeAttack1( float flDot, float flDist )
{
	if ( flDist <= 64 && flDot >= 0.7 && m_hEnemy != NULL && FBitSet( m_hEnemy->pev->flags, FL_ONGROUND ) )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckMeleeAttack2 - the leap, every 5 to 7 seconds
//=========================================================
BOOL CMicroRaptor::CheckMeleeAttack2( float flDot, float flDist )
{
	if ( m_flNextJumpTime <= gpGlobals->time && flDist <= 256 && flDist > 64 && m_hEnemy != NULL && flDot > 0.9 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// TakeDamage - raptors don't hurt each other; anything
// else calls the other microraptors for help
//=========================================================
int CMicroRaptor::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( FClassnameIs( pevAttacker, "monster_raptor" ) || FClassnameIs( pevAttacker, "monster_microraptor" ) )
		return 0;

	CallForHelp( "monster_microraptor", 1024, m_hEnemy, m_vecEnemyLKP );

	return CSquadMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// JumpTouch - the leap hurts what it lands on
//=========================================================
void CMicroRaptor::JumpTouch( CBaseEntity *pOther )
{
	if ( !FBitSet( pev->flags, FL_ONGROUND ) )
	{
		// sic: these two keep the touch function
		if ( pOther->pev->takedamage == DAMAGE_NO )
			return;

		if ( pOther->Classify() == Classify() )
			return;

		TaskComplete();

		pOther->TakeDamage( pev, pev, MRAPTOR_DMG_JUMP, DMG_SLASH );

		CBaseMonster *pMonster = pOther->MyMonsterPointer();
		if ( pMonster )
		{
			TraceResult tr;

			UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
			pMonster->TraceBleed( MRAPTOR_DMG_JUMP, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

			AttackHitSound( pMonster );
		}
	}

	SetTouch( NULL );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CMicroRaptor::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case MRAPTOR_AE_BITE:
		{
			CBaseEntity *pHurt = CheckEnemyMeleeHit( 70, MRAPTOR_DMG_BITE, DMG_SLASH, 5, 0 );
			if ( pHurt )
			{
				CBaseMonster *pMonster = pHurt->MyMonsterPointer();
				if ( pMonster )
				{
					TraceResult tr;

					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					pMonster->TraceBleed( MRAPTOR_DMG_BITE, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

					if ( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ) )
					{
						pHurt->pev->punchangle.z = RANDOM_LONG( -5, 5 );
						pHurt->pev->punchangle.x = RANDOM_LONG( -5, 5 );
						AttackHitSound( pMonster );
					}
				}
			}
		}
		break;

	case MRAPTOR_AE_CLAW:
		{
			CBaseEntity *pHurt = CheckEnemyMeleeHit( 70, MRAPTOR_DMG_BITE, DMG_SLASH, 5, 0 );
			if ( pHurt )
			{
				CBaseMonster *pMonster = pHurt->MyMonsterPointer();
				if ( pMonster )
				{
					TraceResult tr;

					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					pMonster->TraceBleed( MRAPTOR_DMG_BITE, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

					if ( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ) )
					{
						pHurt->pev->punchangle.z = RANDOM_LONG( -5, 5 );
						pHurt->pev->punchangle.x = RANDOM_LONG( -5, 5 );
						AttackHitSound( pMonster );
					}
				}
			}
		}
		break;

	case MRAPTOR_AE_LEAP:
		{
			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "raptor/Raptor_LeapStartMiddleEnd_F0.wav", 1, ATTN_NORM, 0, PITCH_NORM );

			m_flNextJumpTime = gpGlobals->time + RANDOM_LONG( 5, 7 );

			if ( m_hEnemy != NULL )
			{
				Vector vecDest = m_hEnemy->Center();
				Vector vecJump = VecCheckThrow( pev, pev->origin, vecDest, 600, 1.0 );

				if ( vecJump != g_vecZero )
				{
					pev->velocity = vecJump;
					ClearBits( pev->flags, FL_ONGROUND );
					SetTouch( &CMicroRaptor::JumpTouch );
				}
			}
		}
		break;

	case MRAPTOR_AE_LAND:
		break;

	default:
		CSquadMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards.
//=========================================================
int CMicroRaptor::ISoundMask( void )
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_CARCASS	|
			bits_SOUND_MEAT		|
			bits_SOUND_GARBAGE	|
			bits_SOUND_DANGER	|
			bits_SOUND_PLAYER;
}

//=========================================================
// RunAI - it moves fast
//=========================================================
void CMicroRaptor::RunAI( void )
{
	switch ( m_Activity )
	{
	case ACT_RUN:
		pev->framerate = 1.6;
		break;
	case ACT_WALK:
		pev->framerate = 1.6;
		break;
	case ACT_FLY:
	case ACT_HOP:
	case ACT_LAND:
		pev->framerate = 1.6;
		break;
	default:
		pev->framerate = 1.0;
		break;
	}

	CSquadMonster::RunAI();
}

void CMicroRaptor::StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_DIE:
		RouteClear();
		if ( RANDOM_LONG( 0, 3 ) == 0 )
			m_IdealActivity = ACT_DIEVIOLENT;
		else
			m_IdealActivity = ACT_DIESIMPLE;
		pev->deadflag = DEAD_DYING;
		pev->solid = SOLID_NOT;
		break;

	case TASK_MRAPTOR_EAT:
		MakeIdealYaw( m_vecEnemyLKP );
		ChangeYaw( pev->yaw_speed );
		SetActivity( ACT_EAT );
		break;

	case TASK_MRAPTOR_THREAT_DISPLAY:
		if ( m_tNextSoundTime < gpGlobals->time )
		{
			m_IdealActivity = ACT_THREAT_DISPLAY;
			m_tNextSoundTime = gpGlobals->time + 9;
		}
		else
		{
			TaskFail();
		}
		break;

	case TASK_MRAPTOR_GET_TO_LEADER:
		{
			CSquadMonster *pLeader = MySquadLeader();
			if ( pLeader )
				m_hTargetEnt = pLeader;
			else
				TaskFail();
		}
		break;

	case TASK_MRAPTOR_LEAP_AWAY:
		{
			if ( HasConditions( bits_COND_SEE_ENEMY ) )
			{
				TaskFail();
				break;
			}

			m_IdealActivity = ACT_LEAP;

			float flGravity = CVAR_GET_FLOAT( "sv_gravity" );

			ClearBits( pev->flags, FL_ONGROUND );

			pev->velocity = gpGlobals->v_forward * 100 * RANDOM_FLOAT( -1, 1 );
			pev->velocity.z += RANDOM_FLOAT( 0.1, 0.4 ) * ( flGravity * 0.5 );
			pev->velocity.x += RANDOM_FLOAT( -0.4, 0.4 ) * ( flGravity * 0.5 );
		}
		break;

	default:
		CSquadMonster::StartTask( pTask );
		break;
	}
}

void CMicroRaptor::RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_MRAPTOR_THREAT_DISPLAY:
		if ( m_fSequenceFinished )
			TaskComplete();
		break;

	case TASK_MRAPTOR_EAT:
		// sic: never completes; it eats until something interrupts it
		if ( m_fSequenceFinished && m_tNextSoundTime < gpGlobals->time )
			m_tNextSoundTime = gpGlobals->time + 9;
		break;

	case TASK_MRAPTOR_GET_TO_LEADER:
		if ( m_hTargetEnt == NULL )
		{
			TaskFail();
		}
		else if ( ( m_hTargetEnt->pev->origin - pev->origin ).Length() < 100.0 )
		{
			TaskComplete();
		}
		else
		{
			m_vecMoveGoal = m_hTargetEnt->pev->origin;
			if ( !MoveToTarget( ACT_WALK, 2 ) )
				TaskFail();
		}
		break;

	case TASK_MRAPTOR_LEAP_AWAY:
		if ( FBitSet( pev->flags, FL_ONGROUND ) )
			TaskComplete();

		if ( m_fSequenceFinished )
			TaskComplete();
		break;

	default:
		CSquadMonster::RunTask( pTask );
		break;
	}
}

//=========================================================
// GetSchedule
//=========================================================
Schedule_t *CMicroRaptor::GetSchedule( void )
{
	switch ( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		{
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			{
				m_iRelaxed = TRUE;
				break;
			}

			if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			{
				m_iRelaxed = FALSE;
				if ( UTIL_PercentChance( 5 ) )
					return GetScheduleOfType( SCHED_SMALL_FLINCH );
			}

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
				return GetScheduleOfType( SCHED_JUMP_AIR );
		}
		break;

	case MONSTERSTATE_ALERT:
		{
			if ( HasConditions( bits_COND_SEE_ENEMY | bits_COND_NEW_ENEMY ) )
				m_IdealMonsterState = MONSTERSTATE_COMBAT;

			if ( HasConditions( bits_COND_HEAR_SOUND ) )
			{
				CSound *pSound = PBestSound();
				if ( pSound && ( pSound->m_iType & ( bits_SOUND_COMBAT | bits_SOUND_DANGER ) ) )
				{
					m_iRelaxed = FALSE;
					MakeIdealYaw( pSound->m_vecOrigin );
					return &slMRaptorUrgent[ 0 ];
				}
			}

			if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			{
				m_iRelaxed = FALSE;
				if ( UTIL_PercentChance( 30 ) )
					return GetScheduleOfType( SCHED_SMALL_FLINCH );
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ORIGIN );
			}

			if ( HasConditions( bits_COND_SMELL_FOOD ) && m_iRelaxed == TRUE )
			{
				// sic: looks for the scent and doesn't use it
				PBestScent();
				return GetScheduleOfType( SCHED_MRAPTOR_EAT );
			}
		}
		break;
	}

	return CSquadMonster::GetSchedule();
}

//=========================================================
// GetScheduleOfType
//=========================================================
Schedule_t *CMicroRaptor::GetScheduleOfType( int Type )
{
	switch ( Type )
	{
	case SCHED_MELEE_ATTACK1:
		return &slMRaptorMeleeAttack[ 0 ];
	case SCHED_MRAPTOR_EAT:
		return &slMRaptorEat[ 0 ];
	case SCHED_MRAPTOR_PRETEND_EAT:
		return &slMRaptorPretendEat[ 0 ];
	}

	return CSquadMonster::GetScheduleOfType( Type );
}

void CMicroRaptor::Precache( void )
{
	g_iGoreHuman = PRECACHE_MODEL( "sprites/gorehuman.spr" );
	g_iGibHumanSpr = PRECACHE_MODEL( "sprites/gibhuman.spr" );
	PRECACHE_MODEL( "models/raptor.mdl" );

	CSquadMonster::Precache();
}

void CMicroRaptor::Spawn( void )
{
	Precache();

	SET_MODEL( ENT(pev), "models/raptor.mdl" );
	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 32 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->health			= 12;
	m_tNextSoundTime	= gpGlobals->time;
	m_tNextMeleeTime	= gpGlobals->time;
	m_iRelaxed			= TRUE;

	MonsterInit();

	m_afCapability		|= bits_CAP_MELEE_ATTACK2;
	pev->view_ofs		= Vector( 0, 0, 24 );
}
