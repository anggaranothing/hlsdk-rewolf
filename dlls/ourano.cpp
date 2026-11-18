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
// monster_ourano: the mayan ouranosaurus, a big squad
// monster on the player's side. It stands about ("borg")
// until something provokes it, then bites; when calm it
// eats what it smells. decore_nest is its nest of eggs:
// touching, using or hurting it calls the ouranos for help.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "squadmonster.h"
#include "soundent.h"
#include "decals.h"

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_OURANO_EAT = LAST_COMMON_SCHEDULE + 1,
	SCHED_OURANO_PRETEND_EAT,
	SCHED_OURANO_UNUSED,			// no code asks for it
	SCHED_OURANO_BORG,
};

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_OURANO_EAT = LAST_COMMON_TASK + 1,	// face the food and chew
	TASK_OURANO_THREAT_DISPLAY,				// roar, if it hasn't for a while
	TASK_OURANO_UNUSED,						// no code handles it
	TASK_OURANO_GET_TO_LEADER,				// walk up to the squad leader
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define OURANO_AE_BITE1		( 1 )
#define OURANO_AE_BITE2		( 2 )

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlOuranoMeleeAttack[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_MELEE_ATTACK1,		(float)0		},
};

Schedule_t	slOuranoMeleeAttack[] =
{
	{
		tlOuranoMeleeAttack,
		ARRAYSIZE ( tlOuranoMeleeAttack ),
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_ENEMY_OCCLUDED,
		0,
		"Ourano Primary Melee Attack"
	},
};

// the squad leader roars now and then instead of biting
Task_t	tlOuranoRoar[] =
{
	{ TASK_STOP_MOVING,				(float)0		},
	{ TASK_OURANO_THREAT_DISPLAY,	(float)0		},
};

Schedule_t	slOuranoRoar[] =
{
	{
		tlOuranoRoar,
		ARRAYSIZE ( tlOuranoRoar ),
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE,
		0,
		"Roaring"
	},
};

// walk to the food and eat it
Task_t	tlOuranoEat[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_OURANO_PRETEND_EAT	},
	{ TASK_STOP_MOVING,				(float)0						},
	{ TASK_STORE_LASTPOSITION,		(float)0						},
	{ TASK_GET_PATH_TO_BESTSCENT,	(float)0						},
	{ TASK_WALK_PATH,				(float)0						},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0						},
	{ TASK_OURANO_EAT,				(float)0						},
};

Schedule_t	slOuranoEat[] =
{
	{
		tlOuranoEat,
		ARRAYSIZE ( tlOuranoEat ),
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
Task_t	tlOuranoPretendEat[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_IDLE_STAND			},
	{ TASK_STOP_MOVING,				(float)0						},
	{ TASK_STORE_LASTPOSITION,		(float)0						},
	{ TASK_OURANO_GET_TO_LEADER,	(float)50						},
	{ TASK_OURANO_EAT,				(float)0						},
};

Schedule_t	slOuranoPretendEat[] =
{
	{
		tlOuranoPretendEat,
		ARRAYSIZE ( tlOuranoPretendEat ),
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

// failed in combat with a player in view: face the enemy, roar and wait
Task_t	tlOuranoCombatFail[] =
{
	{ TASK_STOP_MOVING,				(float)0		},
	{ TASK_FACE_ENEMY,				(float)0		},
	{ TASK_OURANO_THREAT_DISPLAY,	(float)0		},
	{ TASK_WAIT_RANDOM,				(float)7		},
};

Schedule_t	slOuranoCombatFail[] =
{
	{
		tlOuranoCombatFail,
		ARRAYSIZE ( tlOuranoCombatFail ),
		bits_COND_NEW_ENEMY			|
		bits_COND_CAN_ATTACK		|
		bits_COND_HEAVY_DAMAGE,
		bits_SOUND_DANGER			|
		bits_SOUND_PLAYER,
		"Failed with PVS"
	},
};

// failed out of combat: roar, then wait for a player to come into view
Task_t	tlOuranoFail[] =
{
	{ TASK_STOP_MOVING,				(float)0		},
	{ TASK_OURANO_THREAT_DISPLAY,	(float)0		},
	{ TASK_WAIT_FACE_ENEMY,			(float)1		},
	{ TASK_WAIT_RANDOM,				(float)2		},
	{ TASK_WAIT_PVS,				(float)0		},
};

Schedule_t	slOuranoFail[] =
{
	{
		tlOuranoFail,
		ARRAYSIZE ( tlOuranoFail ),
		bits_COND_NEW_ENEMY			|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK2	|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER			|
		bits_SOUND_PLAYER,
		"Failed with PVS"
	},
};

// sic: no code uses it
Task_t	tlOuranoMeleeAttack2[] =
{
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_MELEE_ATTACK2,		(float)0		},
};

Schedule_t	slOuranoMeleeAttack2[] =
{
	{
		tlOuranoMeleeAttack2,
		ARRAYSIZE ( tlOuranoMeleeAttack2 ),
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_ENEMY_OCCLUDED,
		0,
		"Secondary Melee Attack"
	},
};

// stand idle until provoked, hurt or hungry
Task_t	tlOuranoBorg[] =
{
	{ TASK_STOP_MOVING,			(float)0					},
	{ TASK_SET_ACTIVITY_IDLE,	(float)0					},
	{ TASK_SET_SCHEDULE,		(float)SCHED_OURANO_BORG	},
};

Schedule_t	slOuranoBorg[] =
{
	{
		tlOuranoBorg,
		ARRAYSIZE ( tlOuranoBorg ),
		bits_COND_SMELL_FOOD		|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_PROVOKED,
		0,
		"Ourano Borg"
	},
};

class COurano : public CSquadMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void PrecacheSounds( void );
	int  MonsterType( void ) { return 7; }
	void SetObjectCollisionBox( void );
	int  Classify( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	void RunAI( void );
	void SetState( MONSTERSTATE State );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckMeleeAttack2( float flDot, float flDist );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

	static const char *pOuranoSounds[];

	int		m_iUnused;					// never used
	float	m_tNextSoundTime;			// the pain, idle and threat sounds wait until then
	float	m_tNextMeleeTime;			// set in Spawn, never read
	BOOL	m_bBorg;					// standing about until provoked
	int		m_iLastHurtRelationship;	// how it feels about whoever hurt it last; never read
};

LINK_ENTITY_TO_CLASS( monster_ourano, COurano );

TYPEDESCRIPTION	COurano::m_SaveData[] =
{
	DEFINE_FIELD( COurano, m_tNextMeleeTime, FIELD_TIME ),
	DEFINE_FIELD( COurano, m_tNextSoundTime, FIELD_TIME ),
	DEFINE_FIELD( COurano, m_bBorg, FIELD_BOOLEAN ),
	DEFINE_FIELD( COurano, m_iLastHurtRelationship, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( COurano, CSquadMonster );

DEFINE_CUSTOM_SCHEDULES( COurano )
{
	slOuranoMeleeAttack,
	slOuranoEat,
	slOuranoPretendEat,
	slOuranoCombatFail,
	slOuranoFail,
	slOuranoBorg,
};

IMPLEMENT_CUSTOM_SCHEDULES( COurano, CSquadMonster );

const char *COurano::pOuranoSounds[] =
{
	"ourano/Ourano_BigFlinch_F0.wav",
	"ourano/Ourano_Bite_F0.wav",
	"ourano/Ourano_Chew_F0.wav",
	"ourano/Ourano_DieDrastic_F0.wav",
	"ourano/Ourano_DieSimple_F0.wav",
	"ourano/Ourano_FootStep1.wav",
	"ourano/Ourano_FootStep2.wav",
	"ourano/Ourano_FootStep3.wav",
	"ourano/Ourano_FootStep4.wav",
	"ourano/Ourano_Idle_2_F0.wav",
	"ourano/Ourano_Idle_F0.wav",
	"ourano/Ourano_MoveAttack_FirstStep.wav",
	"ourano/Ourano_MoveAttack_JawSnap.wav",
	"ourano/Ourano_MoveAttack_SecondStep.wav",
	"ourano/Ourano_MoveAttack_Vocals.wav",
	"ourano/Ourano_QuickAttack_F0.wav",
	"ourano/Ourano_Roar_F0.wav",
	"ourano/Ourano_SeePlayer.wav",
	"ourano/Ourano_SmallFlinch_F0.wav",
};

void COurano::PrecacheSounds( void )
{
	for ( int i = 0; i < ARRAYSIZE( pOuranoSounds ); i++ )
		PRECACHE_SOUND( (char *)pOuranoSounds[i] );
}

void COurano::PainSound( void )
{
	if ( m_tNextSoundTime < gpGlobals->time )
	{
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "ourano/Ourano_SmallFlinch_F0.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		m_tNextSoundTime = gpGlobals->time + 1;
	}
}

void COurano::AlertSound( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "ourano/Ourano_SeePlayer.wav", 1, ATTN_NORM, 0, PITCH_NORM );
}

void COurano::IdleSound( void )
{
	if ( m_tNextSoundTime < gpGlobals->time )
	{
		m_tNextSoundTime = gpGlobals->time + 5.0;

		switch ( RANDOM_LONG( 0, 1 ) )
		{
		case 0:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "ourano/Ourano_Idle_2_F0.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 1:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "ourano/Ourano_Idle_F0.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		}
	}
}

//=========================================================
// Classify - the ourano sides with the player
//=========================================================
int COurano::Classify( void )
{
	return CLASS_PLAYER_ALLY;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void COurano::SetYawSpeed( void )
{
	switch ( m_Activity )
	{
	case ACT_IDLE:
		pev->yaw_speed = 40;
		break;
	case ACT_WALK:
		pev->yaw_speed = 50;
		break;
	case ACT_RUN:
		pev->yaw_speed = 40;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		pev->yaw_speed = 60;
		break;
	default:
		pev->yaw_speed = 60;
		break;
	}
}

BOOL COurano::CheckMeleeAttack1( float flDot, float flDist )
{
	if ( flDist <= 96 && m_hEnemy != NULL )
	{
		return TRUE;
	}
	return FALSE;
}

BOOL COurano::CheckMeleeAttack2( float flDot, float flDist )
{
	return FALSE;
}

//=========================================================
// TakeDamage - calls the other ouranos for help and bleeds
// now and then; raptors hurt it three times as much
//=========================================================
int COurano::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	m_iLastHurtRelationship = IRelationship( (CBaseEntity *)GET_PRIVATE( ENT( pevAttacker ) ) );

	CallForHelp( "monster_ourano", 512, m_hEnemy, m_vecEnemyLKP );

	if ( RANDOM_FLOAT( 0, 1 ) < 0.3 )
		UTIL_BloodDrips( pev->origin, UTIL_RandomBloodVector(), BLOOD_COLOR_RED, 10 );

	if ( FClassnameIs( pevAttacker, "monster_raptor" ) )
		flDamage *= 3;

	return CSquadMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void COurano::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case OURANO_AE_BITE1:
	case OURANO_AE_BITE2:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 96, gSkillData.ouranoDmg, DMG_SLASH );
			if ( pHurt )
			{
				pHurt->pev->punchangle.z = RANDOM_LONG( -15, 15 );
				pHurt->pev->punchangle.x = RANDOM_LONG( -15, 15 );
				pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_forward * 30;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_up * 30;

				CBaseMonster *pMonster = pHurt->MyMonsterPointer();
				if ( pMonster )
				{
					TraceResult tr;

					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					pMonster->TraceBleed( gSkillData.ouranoDmg, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

					AttackHitSound( pMonster );
				}
			}
		}
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
int COurano::ISoundMask( void )
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_PLAYER	|
			bits_SOUND_MEAT		|
			bits_SOUND_DANGER	|
			bits_SOUND_GARBAGE;
}

//=========================================================
// RunAI - provoked, it stops standing about
//=========================================================
void COurano::RunAI( void )
{
	if ( HasMemory( bits_MEMORY_PROVOKED ) && m_bBorg )
		m_bBorg = FALSE;

	CSquadMonster::RunAI();
}

void COurano::StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_OURANO_EAT:
		MakeIdealYaw( m_vecEnemyLKP );
		ChangeYaw( pev->yaw_speed );
		SetActivity( ACT_EAT );
		break;

	case TASK_OURANO_THREAT_DISPLAY:
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

	case TASK_OURANO_GET_TO_LEADER:
		{
			CSquadMonster *pLeader = MySquadLeader();
			if ( pLeader )
				m_hTargetEnt = pLeader;
			else
				TaskFail();
		}
		break;

	default:
		CSquadMonster::StartTask( pTask );
		break;
	}
}

void COurano::RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_OURANO_EAT:
		if ( m_fSequenceFinished )
			TaskComplete();
		break;

	case TASK_OURANO_THREAT_DISPLAY:
		if ( m_fSequenceFinished )
			TaskComplete();
		break;

	case TASK_OURANO_GET_TO_LEADER:
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

	default:
		CSquadMonster::RunTask( pTask );
		break;
	}
}

//=========================================================
// SetState - sic: nothing calls it; while it stands about
// the ourano would stay idle
//=========================================================
void COurano::SetState( MONSTERSTATE State )
{
	CSquadMonster::SetState( m_bBorg == TRUE ? MONSTERSTATE_IDLE : State );
}

//=========================================================
// GetSchedule
//=========================================================
Schedule_t *COurano::GetSchedule( void )
{
	if ( m_bBorg == TRUE )
		ClearConditions( bits_COND_HEAR_SOUND );

	switch ( m_MonsterState )
	{
	case MONSTERSTATE_IDLE:
		if ( HasConditions( bits_COND_SMELL_FOOD ) )
			return GetScheduleOfType( SCHED_OURANO_EAT );
		break;

	case MONSTERSTATE_COMBAT:
		{
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			{
				m_bBorg = TRUE;
				break;
			}

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK2 );

			if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			{
				m_bBorg = FALSE;
				if ( !HasMemory( bits_MEMORY_FLINCHED ) )
				{
					if ( UTIL_PercentChance( 75 ) )
						return GetScheduleOfType( SCHED_SMALL_FLINCH );
					return GetScheduleOfType( SCHED_BIG_FLINCH );
				}
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ORIGIN );
			}

			return GetScheduleOfType( SCHED_CHASE_ENEMY );
		}
		break;

	case MONSTERSTATE_ALERT:
		{
			if ( HasConditions( bits_COND_SEE_ENEMY | bits_COND_NEW_ENEMY ) )
				m_IdealMonsterState = MONSTERSTATE_COMBAT;

			if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			{
				m_bBorg = FALSE;
				if ( !HasMemory( bits_MEMORY_FLINCHED ) )
				{
					if ( UTIL_PercentChance( 75 ) )
						return GetScheduleOfType( SCHED_SMALL_FLINCH );
					return GetScheduleOfType( SCHED_BIG_FLINCH );
				}
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ORIGIN );
			}
		}
		break;
	}

	return CSquadMonster::GetSchedule();
}

//=========================================================
// GetScheduleOfType
//=========================================================
Schedule_t *COurano::GetScheduleOfType( int Type )
{
	switch ( Type )
	{
	case SCHED_OURANO_BORG:
		return &slOuranoBorg[ 0 ];

	case SCHED_OURANO_EAT:
		return &slOuranoEat[ 0 ];

	case SCHED_OURANO_PRETEND_EAT:
		return &slOuranoPretendEat[ 0 ];

	case SCHED_MELEE_ATTACK1:
		// the squad leader roars now and then
		if ( IsLeader() && InSquad() && UTIL_PercentChance( 5 ) )
			return &slOuranoRoar[ 0 ];
		return &slOuranoMeleeAttack[ 0 ];

	case SCHED_FAIL:
		if ( m_MonsterState == MONSTERSTATE_COMBAT )
		{
			if ( !FNullEnt( FIND_CLIENT_IN_PVS( edict() ) ) )
				return &slOuranoCombatFail[ 0 ];
		}
		else
		{
			return &slOuranoFail[ 0 ];
		}
		break;
	}

	return CSquadMonster::GetScheduleOfType( Type );
}

void COurano::Precache( void )
{
	g_iGoreHuman = PRECACHE_MODEL( "sprites/gorehuman.spr" );
	g_iGibHumanSpr = PRECACHE_MODEL( "sprites/gibhuman.spr" );
	PRECACHE_MODEL( "models/ourano.mdl" );

	CSquadMonster::Precache();
}

void COurano::Spawn( void )
{
	Precache();

	SET_MODEL( ENT(pev), "models/ourano.mdl" );
	UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 60 ) );

	m_afCapability		|= bits_CAP_SQUAD;
	pev->solid			= SOLID_SLIDEBOX;
	pev->health			= UTIL_RandomizeHealth( gSkillData.ouranoHealth );
	m_flFieldOfView		= -0.7;
	m_tNextSoundTime	= gpGlobals->time;
	m_tNextMeleeTime	= gpGlobals->time;
	m_bBorg				= TRUE;

	MonsterInit();

	pev->view_ofs		= Vector( 0, 0, 40 );
}

void COurano::SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector( -48, -48, 0 );
	pev->absmax = pev->origin + Vector( 48, 48, 72 );
}

//=========================================================
// decore_nest - the ourano's nest. It calls the ouranos
// for help when touched, used or hurt; a player who uses
// it eats an egg for 40 to 60 health, and it breaks open
// when it dies.
//=========================================================
class CNest : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int  ObjectCaps( void ) { return FCAP_ACROSS_TRANSITION | FCAP_ONOFF_USE; }
	int  Classify( void );
	int  BloodColor( void );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void GibMonster( void );

private:
	void EXPORT NestTouch( CBaseEntity *pOther )
	{
		CallForHelp( "monster_ourano", 512, m_hEnemy, m_vecEnemyLKP );
	}
	void EXPORT NestUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		pActivator->TakeHealth( RANDOM_LONG( 40, 60 ), DMG_GENERIC );

		CallForHelp( "monster_ourano", 512, m_hEnemy, m_vecEnemyLKP );

		pev->body = 1;
		pev->health = -1;

		if ( RANDOM_LONG( 0, 1 ) )
			EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "ourano/eggs_crack1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		else
			EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "ourano/eggs_crack2.wav", 1, ATTN_NORM, 0, PITCH_NORM );

		SetUse( NULL );
	}
};

LINK_ENTITY_TO_CLASS( decore_nest, CNest );

void CNest::Spawn( void )
{
	Precache();

	SET_MODEL( ENT(pev), "models/ornest.mdl" );
	UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 16 ) );
	UTIL_SetOrigin( pev, pev->origin );

	pev->takedamage		= DAMAGE_AIM;
	pev->movetype		= MOVETYPE_NONE;
	pev->solid			= SOLID_SLIDEBOX;
	m_bloodColor		= 0;
	pev->health			= 12;
	m_afCapability		|= bits_CAP_SQUAD;

	if ( DROP_TO_FLOOR( ENT(pev) ) == 0 )
	{
		ALERT( at_error, "%s fell out of level at %f,%f,%f", STRING( pev->classname ), pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
		return;
	}

	MonsterInit();

	SetTouch( &CNest::NestTouch );
	SetUse( &CNest::NestUse );
}

int CNest::BloodColor( void )
{
	return DONT_BLEED;
}

//=========================================================
// Classify - on the player's side while it is whole
//=========================================================
int CNest::Classify( void )
{
	return IsAlive() ? CLASS_PLAYER_ALLY : CLASS_NONE;
}

void CNest::Precache( void )
{
	PRECACHE_MODEL( "models/ornest.mdl" );
	PRECACHE_SOUND( "ourano/eggs_crack1.wav" );
	PRECACHE_SOUND( "ourano/eggs_crack2.wav" );
}

//=========================================================
// GibMonster - no gibs, just egg splats on the floor
//=========================================================
void CNest::GibMonster( void )
{
	TraceResult tr;

	UTIL_TraceLine( pev->origin + Vector( 0, 0, 16 ), pev->origin - Vector( 0, 0, 16 ), ignore_monsters, ENT(pev), &tr );
	UTIL_DecalTrace( &tr, DECAL_EGG1 );
	UTIL_DecalTrace( &tr, DECAL_EGG2 );
}

//=========================================================
// TakeDamage - calls the ouranos for help; it cracks at
// every hit and breaks open, with a smell of meat, when it
// dies
//=========================================================
int CNest::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	CallForHelp( "monster_ourano", 512, m_hEnemy, m_vecEnemyLKP );

	CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

	if ( pev->health < 0 && pev->deadflag != DEAD_DEAD )
	{
		pev->body = 1;
		CSoundEnt::InsertSound( bits_SOUND_MEAT, pev->origin, 384, 10 );
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "ourano/eggs_crack2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		pev->deadflag = DEAD_DEAD;
		return 1;
	}

	EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "ourano/eggs_crack1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
	return 1;
}
