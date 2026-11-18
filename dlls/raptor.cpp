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
// monster_raptor and monster_rheptor: the mayan raptor, a
// squad monster that bites and leaps at its enemy, eats
// what it smells when it is calm and goes for the nests.
// A raptor with skin 1 wears a saddle and sides with the
// bandits. Its relationship and ideal-state code is the
// bullsquid's, with the nest for the headcrab.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "animation.h"
#include "squadmonster.h"
#include "soundent.h"
#include "game.h"

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_RAPTOR_EAT = LAST_COMMON_SCHEDULE + 1,
	SCHED_RAPTOR_PRETEND_EAT,
};

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_RAPTOR_EAT = LAST_COMMON_TASK + 1,	// face the food and chew until interrupted
	TASK_RAPTOR_THREAT_DISPLAY,				// no schedule uses it
	TASK_RAPTOR_UNUSED,						// no code handles it
	TASK_RAPTOR_GET_TO_LEADER,				// walk up to the squad leader
	TASK_RAPTOR_LEAP_AWAY,					// leap off in a random direction
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define RAPTOR_AE_BITE		( 1 )
#define RAPTOR_AE_CLAW		( 2 )
#define RAPTOR_AE_LEAP		( 3 )

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlRaptorMeleeAttack[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_MELEE_ATTACK1,		(float)0		},
};

Schedule_t	slRaptorMeleeAttack[] =
{
	{
		tlRaptorMeleeAttack,
		ARRAYSIZE ( tlRaptorMeleeAttack ),
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
Task_t	tlRaptorUrgent[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_CHASE_ENEMY_FAILED		},
	{ TASK_RAPTOR_LEAP_AWAY,	(float)0							},
	{ TASK_SET_SCHEDULE,		(float)SCHED_TAKE_COVER_FROM_ORIGIN	},
};

Schedule_t	slRaptorUrgent[] =
{
	{
		tlRaptorUrgent,
		ARRAYSIZE ( tlRaptorUrgent ),
		0,
		0,
		"Urgent"
	},
};

// walk to the food and eat it
Task_t	tlRaptorEat[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_RAPTOR_PRETEND_EAT	},
	{ TASK_STOP_MOVING,				(float)0						},
	{ TASK_STORE_LASTPOSITION,		(float)0						},
	{ TASK_GET_PATH_TO_BESTSCENT,	(float)0						},
	{ TASK_WALK_PATH,				(float)0						},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0						},
	{ TASK_RAPTOR_EAT,				(float)0						},
};

Schedule_t	slRaptorEat[] =
{
	{
		tlRaptorEat,
		ARRAYSIZE ( tlRaptorEat ),
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
Task_t	tlRaptorPretendEat[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_IDLE_STAND			},
	{ TASK_STOP_MOVING,				(float)0						},
	{ TASK_STORE_LASTPOSITION,		(float)0						},
	{ TASK_RAPTOR_GET_TO_LEADER,	(float)50						},
	{ TASK_RAPTOR_EAT,				(float)0						},
};

Schedule_t	slRaptorPretendEat[] =
{
	{
		tlRaptorPretendEat,
		ARRAYSIZE ( tlRaptorPretendEat ),
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

class CRaptor : public CSquadMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void PrecacheSounds( void );
	int  MonsterType( void ) { return 8; }
	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin - Vector( 48, 48, 0 );
		pev->absmax = pev->origin + Vector( 48, 48, 96 );
	}
	int  Classify( void );
	int  IRelationship( CBaseEntity *pTarget );
	int  IgnoreConditions( void );
	MONSTERSTATE GetIdealState( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	void RunAI( void );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckMeleeAttack2( float flDot, float flDist );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	Activity GetDeathActivity( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	void SetActivity( Activity NewActivity );
	void IdleSound( void );
	void AlertSound( void );
	void AttackHitSound( CBaseMonster *pVictim );
	void EXPORT JumpTouch( CBaseEntity *pOther );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

	static const char *pRaptorSounds[];

	int		m_iVoicePitch;
	int		m_iRelaxed;			// no enemy since the last one died: it may go after food
	float	m_tNextSoundTime;	// chewing and threat display wait until then
	float	m_tNextMeleeTime;	// set in Spawn, never read
	int		m_iUnused[4];		// never used
	float	m_flNextJumpTime;	// the leap attack waits until then
	int		m_iUnused2[2];		// never used
	float	m_flNextIdleSound;
	float	m_flLastHurtTime;	// it forgets about nests and food for a while when hurt
};

LINK_ENTITY_TO_CLASS( monster_raptor, CRaptor );
LINK_ENTITY_TO_CLASS( monster_rheptor, CRaptor );

TYPEDESCRIPTION	CRaptor::m_SaveData[] =
{
	DEFINE_FIELD( CRaptor, m_tNextMeleeTime, FIELD_TIME ),
	DEFINE_FIELD( CRaptor, m_tNextSoundTime, FIELD_TIME ),
	DEFINE_FIELD( CRaptor, m_iVoicePitch, FIELD_INTEGER ),
	DEFINE_FIELD( CRaptor, m_iRelaxed, FIELD_INTEGER ),
	DEFINE_FIELD( CRaptor, m_flNextIdleSound, FIELD_FLOAT ),
	DEFINE_FIELD( CRaptor, m_flLastHurtTime, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CRaptor, CSquadMonster );

DEFINE_CUSTOM_SCHEDULES( CRaptor )
{
	slRaptorMeleeAttack,
	slRaptorEat,
	slRaptorPretendEat,
	slRaptorUrgent,
};

IMPLEMENT_CUSTOM_SCHEDULES( CRaptor, CSquadMonster );

const char *CRaptor::pRaptorSounds[] =
{
	"rheptor/step2.wav",
	"rheptor/step1.wav",
	"rheptor/smallflinch.wav",
	"rheptor/seeplayer2.wav",
	"rheptor/seeplayer1.wav",
	"rheptor/roarangry.wav",
	"rheptor/idlenormal2.wav",
	"rheptor/idlenormal1.wav",
	"rheptor/idlechitter3.wav",
	"rheptor/idlechitter2.wav",
	"rheptor/idlechitter1.wav",
	"rheptor/diesimple.wav",
	"rheptor/diefight.wav",
	"rheptor/clawattack4.wav",
	"rheptor/clawattack3.wav",
	"rheptor/clawattack2.wav",
	"rheptor/clawattack1.wav",
	"rheptor/bitechunk.wav",
	"rheptor/biteattack3.wav",
	"rheptor/biteattack2.wav",
	"rheptor/biteattack1.wav",
	"rheptor/bigflinch.wav",
};

void CRaptor::PrecacheSounds( void )
{
	for ( int i = 0; i < ARRAYSIZE( pRaptorSounds ); i++ )
		PRECACHE_SOUND( (char *)pRaptorSounds[i] );
}

//=========================================================
// IdleSound - only when the last try was a while ago
//=========================================================
void CRaptor::IdleSound( void )
{
	if ( m_flNextIdleSound < gpGlobals->time )
	{
		// sic: one time in five it stays quiet
		switch ( RANDOM_LONG( 0, 4 ) )
		{
		case 0:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "rheptor/idlenormal1.wav", 1, 0.8, 0, m_iVoicePitch );
			break;
		case 1:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "rheptor/idlenormal2.wav", 1, 0.8, 0, m_iVoicePitch );
			break;
		case 2:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "rheptor/idlechitter1.wav", 1, 0.8, 0, m_iVoicePitch );
			break;
		case 4:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "rheptor/idlechitter2.wav", 1, 0.8, 0, m_iVoicePitch );
			break;
		}
	}
	else
	{
		m_flNextIdleSound = gpGlobals->time + RANDOM_LONG( 4, 7 );
	}
}

void CRaptor::AlertSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "rheptor/seeplayer1.wav", 1, 0.8, 0, m_iVoicePitch );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "rheptor/seeplayer2.wav", 1, 0.8, 0, m_iVoicePitch );
		break;
	}
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CRaptor::SetYawSpeed( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
	case ACT_WALK:
		ys = 90;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 270;
		break;
	case ACT_RUN:
	case ACT_LEAP:
		ys = 180;
		break;
	case ACT_FLY:
		ys = 360;
		break;
	default:
		ys = 100;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// RunAI - the raptor animates faster on harder skills
//=========================================================
void CRaptor::RunAI( void )
{
	CSquadMonster::RunAI();

	switch ( g_iSkillLevel )
	{
	case SKILL_MEDIUM:
		pev->framerate = 1.0;
		break;
	case SKILL_HARD:
		pev->framerate = 1.2;
		break;
	default:
		pev->framerate = 0.8;
		break;
	}
}

//=========================================================
// CheckMeleeAttack1 - the bite, at an enemy on the ground
//=========================================================
BOOL CRaptor::CheckMeleeAttack1( float flDot, float flDist )
{
	if ( flDist <= 45 && flDot >= 0.7 && m_hEnemy != NULL && FBitSet( m_hEnemy->pev->flags, FL_ONGROUND ) )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckMeleeAttack2 - the leap, every 5 to 7 seconds
//=========================================================
BOOL CRaptor::CheckMeleeAttack2( float flDot, float flDist )
{
	if ( m_flNextJumpTime <= gpGlobals->time && flDist <= 128 && flDist >= 64 && m_hEnemy != NULL && flDot > 0.9 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// TakeDamage - calls the other raptors for help; raptors
// don't hurt each other and the ourano hurts three times
// as much
//=========================================================
int CRaptor::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	CallForHelp( "monster_raptor", 1024, m_hEnemy, m_vecEnemyLKP );

	m_flLastHurtTime = gpGlobals->time;

	if ( FClassnameIs( pevAttacker, "monster_raptor" ) || FClassnameIs( pevAttacker, "monster_microraptor" ) )
		return 0;

	if ( FClassnameIs( pevAttacker, "monster_ourano" ) )
		flDamage *= 3;

	return CSquadMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// GetDeathActivity - a bullet in the head blows it off
//=========================================================
Activity CRaptor::GetDeathActivity( void )
{
	if ( m_LastHitGroup == HITGROUP_HEAD && ( m_LastHitDamageType & DMG_BULLET ) )
	{
		if ( CVAR_GET_FLOAT( "violence_agibs" ) != 0 )
		{
			SetBodygroup( 1, 1 );
			SetBodygroup( 2, 0 );
		}
	}

	return CSquadMonster::GetDeathActivity();
}

//=========================================================
// JumpTouch - the leap hurts what it lands on
//=========================================================
void CRaptor::JumpTouch( CBaseEntity *pOther )
{
	if ( !FBitSet( pev->flags, FL_ONGROUND ) )
	{
		SetBits( pev->flags, FL_ONGROUND );
		TaskComplete();

		// sic: these two keep the touch function
		if ( pOther->pev->takedamage == DAMAGE_NO )
			return;

		if ( pOther->Classify() == Classify() )
			return;

		pOther->TakeDamage( pev, pev, gSkillData.raptorDmgJump, DMG_SLASH );

		CBaseMonster *pMonster = pOther->MyMonsterPointer();
		if ( pMonster )
		{
			TraceResult tr;

			UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
			pMonster->TraceBleed( gSkillData.raptorDmgJump, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

			if ( pMonster->IsPlayer() )
				EMIT_SOUND_DYN( ENT(pMonster->pev), CHAN_VOICE, "lg_scorp/lg_scorp_attackb.wav", 0.5, ATTN_IDLE, 0, 50 );
		}
	}

	SetTouch( NULL );
}

void CRaptor::AttackHitSound( CBaseMonster *pVictim )
{
	EMIT_SOUND_DYN( ENT(pVictim->pev), CHAN_VOICE, "lg_scorp/lg_scorp_attackb.wav", 0.4, ATTN_IDLE, 0, 60 );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CRaptor::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case RAPTOR_AE_BITE:
		{
			CBaseEntity *pHurt = CheckEnemyMeleeHit( 70, gSkillData.raptorDmgBite, DMG_SLASH, 10, 5 );
			if ( pHurt && pHurt->IsPlayer() )
			{
				CBaseMonster *pMonster = pHurt->MyMonsterPointer();
				if ( pMonster )
				{
					TraceResult tr;

					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					pMonster->TraceBleed( gSkillData.raptorDmgBite, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

					if ( pHurt->IsPlayer() )
						AttackHitSound( pMonster );
				}
			}
		}
		break;

	case RAPTOR_AE_CLAW:
		{
			CBaseEntity *pHurt = CheckEnemyMeleeHit( 70, gSkillData.raptorDmgBite, DMG_SLASH, 10, 5 );
			if ( pHurt )
			{
				CBaseMonster *pMonster = pHurt->MyMonsterPointer();
				if ( pMonster )
				{
					TraceResult tr;

					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					// sic: bleeds as much as the large scorpion's sting
					pMonster->TraceBleed( gSkillData.lrgscorpDmgSting, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

					if ( pHurt->IsPlayer() )
						AttackHitSound( pMonster );
				}
			}
		}
		break;

	case RAPTOR_AE_LEAP:
		{
			m_flNextJumpTime = gpGlobals->time + RANDOM_LONG( 5, 7 );

			if ( m_hEnemy != NULL )
			{
				Vector vecEnemy = m_hEnemy->pev->origin;
				Vector vecJump = VecCheckThrow( pev, pev->origin, vecEnemy, 600, 1.0 );

				if ( vecJump != g_vecZero )
				{
					pev->velocity = vecJump;
					ClearBits( pev->flags, FL_ONGROUND );
					SetTouch( &CRaptor::JumpTouch );
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
int CRaptor::ISoundMask( void )
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_CARCASS	|
			bits_SOUND_MEAT		|
			bits_SOUND_GARBAGE	|
			bits_SOUND_DANGER	|
			bits_SOUND_PLAYER;
}

void CRaptor::StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_RAPTOR_EAT:
		MakeIdealYaw( m_vecEnemyLKP );
		ChangeYaw( pev->yaw_speed );
		SetActivity( ACT_EAT );
		break;

	case TASK_RAPTOR_THREAT_DISPLAY:
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

	case TASK_RAPTOR_GET_TO_LEADER:
		{
			CSquadMonster *pLeader = MySquadLeader();
			if ( pLeader )
				m_hTargetEnt = pLeader;
			else
				TaskFail();
		}
		break;

	case TASK_RAPTOR_LEAP_AWAY:
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

	// sic: no breaks, so the bite also claws, and both start
	// the base task three times
	case TASK_MELEE_ATTACK1:
		CSquadMonster::StartTask( pTask );
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "rheptor/clawattack2.wav", 0.8, 0.8, 0, m_iVoicePitch );

	case TASK_MELEE_ATTACK2:
		CSquadMonster::StartTask( pTask );
		switch ( RANDOM_LONG( 0, 1 ) )
		{
		case 0:
			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "rheptor/biteattack1.wav", 0.8, 0.8, 0, m_iVoicePitch );
			break;
		case 1:
			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "rheptor/biteattack2.wav", 0.8, 0.8, 0, m_iVoicePitch );
			break;
		}

	default:
		CSquadMonster::StartTask( pTask );
		break;
	}
}

void CRaptor::RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RAPTOR_THREAT_DISPLAY:
		if ( m_fSequenceFinished )
			TaskComplete();
		break;

	case TASK_RAPTOR_EAT:
		// sic: never completes; it eats until something interrupts it
		if ( m_fSequenceFinished && m_tNextSoundTime < gpGlobals->time )
			m_tNextSoundTime = gpGlobals->time + 9;
		break;

	case TASK_RAPTOR_GET_TO_LEADER:
		if ( m_hTargetEnt == NULL )
		{
			TaskFail();
		}
		else if ( ( m_hTargetEnt->Center() - pev->origin ).Length() < 100.0 )
		{
			TaskComplete();
		}
		else
		{
			m_vecMoveGoal = m_hTargetEnt->Center();
			if ( !MoveToTarget( ACT_WALK, 2 ) )
				TaskFail();
		}
		break;

	case TASK_RAPTOR_LEAP_AWAY:
		if ( FBitSet( pev->flags, FL_ONGROUND ) )
			TaskComplete();

		if ( m_fSequenceFinished )
		{
			SetBits( pev->flags, FL_ONGROUND );
			TaskComplete();
		}
		break;

	default:
		CSquadMonster::RunTask( pTask );
		break;
	}
}

//=========================================================
// GetSchedule
//=========================================================
Schedule_t *CRaptor::GetSchedule( void )
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
				if ( UTIL_PercentChance( 10 ) )
					return GetScheduleOfType( SCHED_SMALL_FLINCH );
			}

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK2 );

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
					return &slRaptorUrgent[ 0 ];
				}
			}

			if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			{
				m_iRelaxed = FALSE;
				if ( UTIL_PercentChance( 10 ) )
					return GetScheduleOfType( SCHED_SMALL_FLINCH );
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ORIGIN );
			}

			if ( HasConditions( bits_COND_SMELL_FOOD ) && m_iRelaxed == TRUE )
			{
				// sic: looks for the scent and doesn't use it
				PBestScent();
				return GetScheduleOfType( SCHED_RAPTOR_EAT );
			}
		}
		break;
	}

	return CSquadMonster::GetSchedule();
}

//=========================================================
// GetScheduleOfType
//=========================================================
Schedule_t *CRaptor::GetScheduleOfType( int Type )
{
	switch ( Type )
	{
	case SCHED_MELEE_ATTACK1:
		return &slRaptorMeleeAttack[ 0 ];
	case SCHED_RAPTOR_EAT:
		return &slRaptorEat[ 0 ];
	case SCHED_RAPTOR_PRETEND_EAT:
		return &slRaptorPretendEat[ 0 ];
	}

	return CSquadMonster::GetScheduleOfType( Type );
}

//=========================================================
// SetActivity - the leap plays "biteattack" at a tall
// enemy and "attack" at a nest or a short one
//=========================================================
void CRaptor::SetActivity( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	switch ( NewActivity )
	{
	case ACT_MELEE_ATTACK2:
		if ( m_hEnemy == NULL || ( !FStrEq( STRING( m_hEnemy->pev->classname ), "decore_nest" ) && m_hEnemy->pev->maxs.z - m_hEnemy->pev->mins.z > 48 ) )
			iSequence = LookupSequence( "biteattack" );
		else
			iSequence = LookupSequence( "attack" );
		break;
	default:
		CSquadMonster::SetActivity( NewActivity );
		return;
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

void CRaptor::Precache( void )
{
	g_iGoreHuman = PRECACHE_MODEL( "sprites/gorehuman.spr" );
	g_iGibHumanSpr = PRECACHE_MODEL( "sprites/gibhuman.spr" );
	PRECACHE_MODEL( "models/rheptor.mdl" );

	CSquadMonster::Precache();
}

//=========================================================
// Classify - the saddled raptor rides with the bandits
//=========================================================
int CRaptor::Classify( void )
{
	if ( pev->skin == 1 )
		return CLASS_HUMAN_MILITARY;
	return CLASS_ALIEN_PREDATOR;
}

//=========================================================
// IRelationship - raptors hate nests unless they are
// busy with an enemy that isn't one
//=========================================================
int CRaptor::IRelationship( CBaseEntity *pTarget )
{
	if ( pTarget && FClassnameIs( pTarget->pev, "decore_nest" ) )
	{
		if ( m_hEnemy == NULL || ( m_hEnemy != NULL && FStrEq( STRING( m_hEnemy->pev->classname ), "decore_nest" ) ) )
			return R_NM;

		return R_NO;
	}

	return CSquadMonster::IRelationship( pTarget );
}

//=========================================================
// IgnoreConditions
//=========================================================
int CRaptor::IgnoreConditions( void )
{
	int iIgnore = CSquadMonster::IgnoreConditions();

	if ( m_MonsterState != MONSTERSTATE_SCRIPT )
	{
		if ( gpGlobals->time - m_flLastHurtTime <= 20 )
		{
			// hurt in the last 20 seconds: no time for food
			iIgnore = bits_COND_SMELL | bits_COND_SMELL_FOOD;
		}

		if ( m_hEnemy != NULL )
		{
			if ( FClassnameIs( m_hEnemy->pev, "decore_nest" ) )
			{
				// (Unless after a nest)
				iIgnore = bits_COND_SMELL | bits_COND_SMELL_FOOD;
			}
		}
	}

	return iIgnore;
}

//=========================================================
// GetIdealState - the bullsquid's: something that hurts it
// makes it forget about a nest for a while
//=========================================================
MONSTERSTATE CRaptor::GetIdealState( void )
{
	int	iConditions;

	iConditions = IScheduleFlags();

	// If no schedule conditions, the new ideal state is probably the reason we're in here.
	switch ( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		/*
		COMBAT goes to ALERT upon death of enemy
		*/
		{
			if ( m_hEnemy != NULL && ( iConditions & bits_COND_LIGHT_DAMAGE || iConditions & bits_COND_HEAVY_DAMAGE ) && FClassnameIs( m_hEnemy->pev, "decore_nest" ) )
			{
				// if the raptor has a nest enemy and something hurts it, it's going to forget about the nest for a while.
				m_hEnemy = NULL;
				m_IdealMonsterState = MONSTERSTATE_ALERT;
			}
			break;
		}
	}

	m_IdealMonsterState = CSquadMonster::GetIdealState();

	return m_IdealMonsterState;
}

void CRaptor::Spawn( void )
{
	Precache();

	SET_MODEL( ENT(pev), "models/rheptor.mdl" );

	pev->movetype	= MOVETYPE_STEP;
	pev->solid		= SOLID_SLIDEBOX;
	pev->health		= UTIL_RandomizeHealth( gSkillData.raptorHealth );

	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 40 ) );

	m_tNextSoundTime	= gpGlobals->time;
	m_tNextMeleeTime	= gpGlobals->time;
	m_flLastHurtTime	= gpGlobals->time - 20;
	m_iRelaxed			= TRUE;
	m_iVoicePitch		= RANDOM_LONG( 97, 103 );

	MonsterInit();

	m_afCapability		|= bits_CAP_MELEE_ATTACK2;
	pev->view_ofs		= Vector( 0, 0, 48 );

	// the saddle
	if ( pev->skin == 1 )
		SetBodygroup( 2, 1 );
}
