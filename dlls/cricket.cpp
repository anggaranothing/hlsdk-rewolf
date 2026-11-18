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
// monster_cricket: the mayan cricket. It hops about when
// the player comes near, gibs at the first hit and is
// squashed when a player touches it. Written from the
// headcrab.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_CRICKET_RANDOM_JUMP = LAST_COMMON_SCHEDULE + 1,
};

Task_t	tlCricketRAFast[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_MELEE_ATTACK1,		(float)0		},
	{ TASK_SET_ACTIVITY_IDLE,	(float)0		},
};

Schedule_t	slCricketRAFast[] =
{
	{
		tlCricketRAFast,
		ARRAYSIZE ( tlCricketRAFast ),
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED,
		0,
		"CricketRAFast"
	},
};

Task_t	tlCricketRandomJump[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_MELEE_ATTACK1,		(float)0		},
	{ TASK_SET_ACTIVITY_IDLE,	(float)0		},
	{ TASK_SET_ACTIVITY_IDLE,	(float)0		},
};

Schedule_t	slCricketRandomJump[] =
{
	{
		tlCricketRandomJump,
		ARRAYSIZE ( tlCricketRandomJump ),
		0,
		0,
		"CricketRandomJump"
	},
};

class CCricket : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void PrecacheSounds( void );
	int  Classify( void ) { return CLASS_ALIEN_PREY; }
	int  MonsterType( void ) { return 5; }
	void SetYawSpeed( void );
	Vector Center( void );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void RunAI( void );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
	void PrescheduleThink( void );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	Schedule_t *GetScheduleOfType( int Type );
	void IdleSound( void );
	void AlertSound( void );
	void PainSound( void );
	void GibSound( void );
	void EXPORT SquashTouch( CBaseEntity *pOther );

	CUSTOM_SCHEDULES;

	static const char *pCricketSounds[];

	float	m_flNextHopTime;	// when the player's nearness may set off another hop
	BOOL	m_fFirstHop;		// the first hop may go for the player
};

LINK_ENTITY_TO_CLASS( monster_cricket, CCricket );

DEFINE_CUSTOM_SCHEDULES( CCricket )
{
	slCricketRAFast,
	slCricketRandomJump,
};

IMPLEMENT_CUSTOM_SCHEDULES( CCricket, CBaseMonster );

const char *CCricket::pCricketSounds[] =
{
	"cricket/cricket_angry.wav",
	"cricket/cricket_angry2.wav",
	"cricket/cricket_angry3.wav",
	"cricket/cricket_flinch.wav",
	"cricket/cricket_idle1.wav",
	"cricket/cricket_idle2.wav",
	"cricket/cricket_idle3.wav",
	"cricket/cricket_jump1.wav",
	"cricket/cricket_jump2.wav",
	"cricket/cricket_jump3.wav",
	"cricket/cricket_splat1.wav",
	"cricket/cricket_splat2.wav",
	"cricket/cricket_splat3.wav",
};

void CCricket::PrecacheSounds( void )
{
	for ( int i = 0; i < ARRAYSIZE( pCricketSounds ); i++ )
		PRECACHE_SOUND( (char *)pCricketSounds[i] );
}

void CCricket::IdleSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "cricket/cricket_idle1.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "cricket/cricket_idle2.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	case 2:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "cricket/cricket_idle3.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	}
}

void CCricket::AlertSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "cricket/cricket_angry.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "cricket/cricket_angry2.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	case 2:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "cricket/cricket_angry3.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	}
}

void CCricket::PainSound( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "cricket/cricket_flinch.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
}

void CCricket::GibSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "cricket/cricket_splat1.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "cricket/cricket_splat2.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	case 2:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "cricket/cricket_splat3.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	}
}

//=========================================================
// Center - the cricket's middle, a little above its feet
//=========================================================
Vector CCricket::Center( void )
{
	return Vector( pev->origin.x, pev->origin.y, pev->origin.z + 10 );
}

void CCricket::SetYawSpeed( void )
{
	pev->yaw_speed = 180;
}

//=========================================================
// TakeDamage - any hit gibs the cricket
//=========================================================
int CCricket::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, DMG_ALWAYSGIB );
}

void CCricket::Precache( void )
{
	PRECACHE_MODEL( "models/cricket.mdl" );
	CBaseMonster::Precache();
}

void CCricket::Spawn( void )
{
	Precache();

	SET_MODEL( ENT(pev), "models/cricket.mdl" );
	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 24 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->health			= 1;
	pev->view_ofs		= Vector( 0, 0, 20 );
	m_afCapability		|= bits_CAP_MELEE_ATTACK1;
	m_flNextHopTime		= gpGlobals->time;
	m_fFirstHop			= TRUE;

	MonsterInit();

	SetTouch( &CCricket::SquashTouch );
}

//=========================================================
// RunAI - when the player comes within 128 units, hop:
// the first time maybe toward the player, then at random,
// every 4 to 6 seconds
//=========================================================
void CCricket::RunAI( void )
{
	CBaseMonster::RunAI();

	if ( gpGlobals->time < m_flNextHopTime )
		return;

	edict_t *pentPlayer = INDEXENT( 1 );
	if ( FNullEnt( pentPlayer ) )
		return;

	CBaseEntity *pPlayer = CBaseEntity::Instance( pentPlayer );
	if ( !pPlayer )
		return;

	if ( ( pev->origin - pPlayer->pev->origin ).Length() <= 128 )
	{
		// sic: whether the player faces the cricket is worked out and never used
		Vector vecDir = pPlayer->pev->origin - pev->origin;
		UTIL_MakeAimVectors( pPlayer->pev->angles );
		Vector vecForward = gpGlobals->v_forward;
		vecDir = vecDir.Normalize();
		vecForward = vecForward.Normalize();
		float flDot = DotProduct( vecDir, vecForward * -1 );

		Schedule_t *pSchedule;
		if ( m_fFirstHop && UTIL_PercentChance( 30 ) )
		{
			pSchedule = GetScheduleOfType( SCHED_FAIL );
			m_fFirstHop = FALSE;
		}
		else
		{
			pSchedule = GetScheduleOfType( SCHED_CRICKET_RANDOM_JUMP );
		}

		m_flNextHopTime = gpGlobals->time + RANDOM_LONG( 4, 6 );

		if ( pSchedule )
			ChangeSchedule( pSchedule );
	}
}

void CCricket::RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_MELEE_ATTACK1:
		if ( m_fSequenceFinished )
		{
			TaskComplete();
			m_IdealActivity = ACT_IDLE;
		}
		break;
	default:
		CBaseMonster::RunTask( pTask );
		break;
	}
}

//=========================================================
// PrescheduleThink - chirp a little in combat
//=========================================================
void CCricket::PrescheduleThink( void )
{
	if ( m_MonsterState == MONSTERSTATE_COMBAT && RANDOM_FLOAT( 0, 5 ) < 0.1 )
	{
		IdleSound();
	}
}

void CCricket::StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_MELEE_ATTACK1:
	case TASK_MELEE_ATTACK2:
		{
			switch ( RANDOM_LONG( 0, 2 ) )
			{
			case 0:
				EMIT_SOUND( ENT(pev), CHAN_WEAPON, "cricket/cricket_jump1.wav", 1, ATTN_IDLE );
				break;
			case 1:
				EMIT_SOUND( ENT(pev), CHAN_WEAPON, "cricket/cricket_jump2.wav", 1, ATTN_IDLE );
				break;
			case 2:
				EMIT_SOUND( ENT(pev), CHAN_WEAPON, "cricket/cricket_jump3.wav", 1, ATTN_IDLE );
				break;
			}

			m_IdealActivity = ACT_LEAP;

			// the headcrab's jump, without an enemy to aim at
			ClearBits( pev->flags, FL_ONGROUND );

			UTIL_SetOrigin( pev, pev->origin + Vector( 0, 0, 1 ) );// take him off ground so engine doesn't instantly reset onground
			UTIL_MakeVectors( pev->angles );

			Vector vecJumpDir = Vector( gpGlobals->v_forward.x, gpGlobals->v_forward.y, gpGlobals->v_up.z ) * 350;

			// sic: a second jump sound, two times in three
			int iSound = RANDOM_LONG( 0, 2 );
			if ( iSound != 0 )
			{
				switch ( RANDOM_LONG( 0, 2 ) )
				{
				case 0:
					EMIT_SOUND( ENT(pev), CHAN_WEAPON, "cricket/cricket_jump1.wav", 1, ATTN_IDLE );
					break;
				case 1:
					EMIT_SOUND( ENT(pev), CHAN_WEAPON, "cricket/cricket_jump2.wav", 1, ATTN_IDLE );
					break;
				case 2:
					EMIT_SOUND( ENT(pev), CHAN_WEAPON, "cricket/cricket_jump3.wav", 1, ATTN_IDLE );
					break;
				}
			}

			pev->velocity = vecJumpDir;
			m_flNextAttack = gpGlobals->time + 2;
			break;
		}
	default:
		CBaseMonster::StartTask( pTask );
		break;
	}
}

BOOL CCricket::CheckMeleeAttack1( float flDot, float flDist )
{
	if ( FBitSet( pev->flags, FL_ONGROUND ) && flDist <= 256 && flDot >= 0.65 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// SquashTouch - a player that touches the cricket while it
// is on the ground squashes it
//=========================================================
void CCricket::SquashTouch( CBaseEntity *pOther )
{
	if ( pOther && pOther->IsPlayer() && FBitSet( pev->flags, FL_ONGROUND ) )
	{
		TakeDamage( pOther->pev, pOther->pev, 100, DMG_CRUSH );
	}
}

Schedule_t *CCricket::GetScheduleOfType( int Type )
{
	switch ( Type )
	{
	case SCHED_MELEE_ATTACK1:
		return slCricketRAFast;

	case SCHED_FAIL:
		{
			// hop toward the player, turning 30 degrees too far
			edict_t *pentPlayer = INDEXENT( 1 );
			if ( FNullEnt( pentPlayer ) )
				return CBaseMonster::GetScheduleOfType( SCHED_FAIL );

			CBaseEntity *pPlayer = CBaseEntity::Instance( pentPlayer );
			if ( !pPlayer )
				return CBaseMonster::GetScheduleOfType( SCHED_FAIL );

			float flYaw = VecToYaw( pPlayer->pev->origin - pev->origin ) - pev->angles.y;

			if ( flYaw > 180 )
				flYaw -= 360;
			if ( flYaw < -180 )
				flYaw += 360;

			if ( flYaw < 0 )
			{
				flYaw -= 30;
				if ( flYaw >= 0 )
					flYaw = 0;
			}
			else
			{
				flYaw += 30;
				if ( flYaw <= 0 )
					flYaw = 0;
			}

			pev->ideal_yaw = flYaw + pev->angles.y;
			return slCricketRandomJump;
		}

	case SCHED_CRICKET_RANDOM_JUMP:
		pev->ideal_yaw = RANDOM_LONG( 0, 360 );
		return slCricketRandomJump;
	}

	return CBaseMonster::GetScheduleOfType( Type );
}
