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
// monster_xenome: the xenome, a squad monster that fades in
// and out of sight, spits darts from either side of its head,
// claws, and leaps at its enemy. Its schedules are the beak's.
// xenomeshot: the xenome's dart, which sticks in the world.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "squadmonster.h"
#include "soundent.h"

// gib and gore spray sprites of the xenome
int g_iGoreXeno;
int g_iGibXeno;

//=========================================================
// CXenomeShot - the xenome's dart
//=========================================================
class CXenomeShot : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );

	static void Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );

	void EXPORT XenomeShotThink( void );
	void EXPORT TouchShot( CBaseEntity *pOther );
};

LINK_ENTITY_TO_CLASS( xenomeshot, CXenomeShot );

void CXenomeShot::Precache( void )
{
	PRECACHE_MODEL( "models/spike.mdl" );
}

void CXenomeShot::Spawn( void )
{
	Precache( );

	pev->classname = MAKE_STRING( "xenomeshot" );
	SET_MODEL( ENT(pev), "models/spike.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
}

void CXenomeShot::Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity )
{
	CXenomeShot *pShot = GetClassPtr( (CXenomeShot *)NULL );
	pShot->Spawn( );

	UTIL_SetOrigin( pShot->pev, vecStart );
	pShot->pev->owner = ENT( pevOwner );
	pShot->pev->velocity = vecVelocity;
	pShot->pev->angles = UTIL_VecToAngles( vecVelocity );

	pShot->SetThink( &CXenomeShot::XenomeShotThink );
	pShot->SetTouch( &CXenomeShot::TouchShot );
	pShot->pev->nextthink = gpGlobals->time;
}

void CXenomeShot::XenomeShotThink( void )
{
	pev->nextthink = gpGlobals->time;
}

//=========================================================
// TouchShot - hurts what can be hurt; sticks in the world
// for ten seconds
//=========================================================
void CXenomeShot::TouchShot( CBaseEntity *pOther )
{
	pev->nextthink = gpGlobals->time;
	SetThink( &CBaseEntity::SUB_Remove );

	if ( pOther->pev->takedamage == DAMAGE_NO )
	{
		if ( FClassnameIs( pOther->pev, "worldspawn" ) )
		{
			// stick it in the wall
			Vector vecDir = pev->velocity.Normalize( );
			UTIL_SetOrigin( pev, pev->origin - vecDir * 12 );
			pev->angles = UTIL_VecToAngles( vecDir );
			pev->solid = SOLID_NOT;
			pev->movetype = MOVETYPE_FLY;
			pev->velocity = Vector( 0, 0, 0 );
			pev->nextthink = gpGlobals->time + 10.0;

			UTIL_Sparks( pev->origin );
			UTIL_Sparks( pev->origin );
		}
	}
	else
	{
		pOther->TakeDamage( pev, pev, gSkillData.xenomeDmgDart, DMG_GENERIC );
	}
}

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_XENOME_ROAR = LAST_COMMON_TASK + 1,	// threat display, at most every 9 seconds
	TASK_XENOME_LEAP_AWAY,						// a short random hop
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define XENOME_AE_SHOOT_LEFT	( 2 )
#define XENOME_AE_SHOOT_RIGHT	( 3 )
#define XENOME_AE_CLAW			( 4 )
#define XENOME_AE_CLAW2			( 5 )
#define XENOME_AE_LEAP			( 6 )
#define XENOME_AE_LEAP2			( 7 )

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

// leap at the enemy, then claw
Task_t	tlXenomeJumpMelee[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_CHASE_ENEMY_FAILED	},
	{ TASK_FACE_ENEMY,			(float)0						},
	{ TASK_STOP_MOVING,			(float)0						},
	{ TASK_SET_SCHEDULE,		(float)SCHED_JUMP_AIR			},
	{ TASK_SET_SCHEDULE,		(float)SCHED_MELEE_ATTACK1		},
};

Schedule_t	slXenomeJumpMelee[] =
{
	{
		tlXenomeJumpMelee,
		ARRAYSIZE ( tlXenomeJumpMelee ),
		0,
		0,
		"Beak Secondary Jump-Melee"
	},
};

// the squad leader's roar before a melee attack
Task_t	tlXenomeRoar[] =
{
	{ TASK_STOP_MOVING,			(float)0					},
	{ TASK_XENOME_ROAR,			(float)0					},
	{ TASK_SET_SCHEDULE,		(float)SCHED_CHASE_ENEMY	},
};

Schedule_t	slXenomeRoar[] =
{
	{
		tlXenomeRoar,
		ARRAYSIZE ( tlXenomeRoar ),
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE,
		0,
		"Beak Roaring"
	},
};

// hop away from a dangerous sound, then take cover
Task_t	tlXenomeUrgent[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_CHASE_ENEMY_FAILED		},
	{ TASK_XENOME_LEAP_AWAY,	(float)0							},
	{ TASK_SET_SCHEDULE,		(float)SCHED_TAKE_COVER_FROM_ORIGIN	},
};

Schedule_t	slXenomeUrgent[] =
{
	{
		tlXenomeUrgent,
		ARRAYSIZE ( tlXenomeUrgent ),
		0,
		0,
		"Urgent"
	},
};

// a player can see it: face the enemy and roar
Task_t	tlXenomeCombatFail[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_XENOME_ROAR,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
};

Schedule_t	slXenomeCombatFail[] =
{
	{
		tlXenomeCombatFail,
		ARRAYSIZE ( tlXenomeCombatFail ),
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK2	|
		bits_COND_NEW_ENEMY,
		bits_SOUND_PLAYER			|
		bits_SOUND_DANGER,
		"Failed with PVS"
	},
};

Task_t	tlXenomeFail[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_XENOME_ROAR,			(float)0		},
	{ TASK_WAIT_FACE_ENEMY,		(float)1		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slXenomeFail[] =
{
	{
		tlXenomeFail,
		ARRAYSIZE ( tlXenomeFail ),
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK2	|
		bits_COND_NEW_ENEMY			|
		bits_COND_HEAR_SOUND,
		bits_SOUND_PLAYER			|
		bits_SOUND_DANGER,
		"Failed with PVS"
	},
};

class CXenome : public CSquadMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void PrecacheSounds( void );
	int  MonsterType( void ) { return 19; }
	int  Classify( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void RunAI( void );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckMeleeAttack2( float flDot, float flDist );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	void RangeAttackSound( void );
	void MeleeAttackSound( void );
	void PainSound( void );
	void AlertSound( void );
	void AttackHitSound( CBaseMonster *pVictim );

	Vector GetShootPosition( int iRight );
	void ShootDart( int iRight );

	void EXPORT LeapTouch( CBaseEntity *pOther );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

	static const char *pXenomeSounds[];

	int		m_iVoicePitch;
	int		m_iTargetAlpha;		// how visible it wants to be
	float	m_tNextSoundTime;	// the next roar
	float	m_flNextJump;
};

LINK_ENTITY_TO_CLASS( monster_xenome, CXenome );

TYPEDESCRIPTION	CXenome::m_SaveData[] =
{
	DEFINE_FIELD( CXenome, m_flNextJump, FIELD_TIME ),
	DEFINE_FIELD( CXenome, m_tNextSoundTime, FIELD_TIME ),
	DEFINE_FIELD( CXenome, m_iVoicePitch, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CXenome, CSquadMonster );

DEFINE_CUSTOM_SCHEDULES( CXenome )
{
	slXenomeJumpMelee,
	slXenomeUrgent,
	slXenomeCombatFail,
	slXenomeFail,
};

IMPLEMENT_CUSTOM_SCHEDULES( CXenome, CSquadMonster );

const char *CXenome::pXenomeSounds[] =
{
	"xenome/xenome_call.wav",
	"xenome/xenome_clawattack_1.wav",
	"xenome/xenome_clawattack_2.wav",
	"xenome/xenome_diesimple.wav",
	"xenome/xenome_dieviolent_1.wav",
	"xenome/xenome_dieviolent_2.wav",
	"xenome/xenome_foot1.wav",
	"xenome/xenome_foot1a.wav",
	"xenome/xenome_foot2.wav",
	"xenome/xenome_foot2a.wav",
	"xenome/xenome_pain1.wav",
	"xenome/xenome_pain2.wav",
	"xenome/xenome_shoot.wav",
	"xenome/xenome_sight1.wav",
	"xenome/xenome_sight2.wav",
};

void CXenome::PrecacheSounds( void )
{
	for ( int i = 0; i < ARRAYSIZE( pXenomeSounds ); i++ )
		PRECACHE_SOUND( (char *)pXenomeSounds[i] );
}

void CXenome::RangeAttackSound( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "xenome/xenome_shoot.wav", 0.5, 0.8, 0, m_iVoicePitch );
}

void CXenome::MeleeAttackSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "xenome/xenome_clawattack_1.wav", 1, 0.8, 0, 80 );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "xenome/xenome_clawattack_2.wav", 1, 0.8, 0, 80 );
		break;
	}
}

void CXenome::PainSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "xenome/xenome_pain1.wav", 1, 0.8, 0, m_iVoicePitch );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "xenome/xenome_pain2.wav", 1, 0.8, 0, m_iVoicePitch );
		break;
	}
}

void CXenome::AlertSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "xenome/xenome_sight1.wav", 1, 0.8, 0, m_iVoicePitch );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "xenome/xenome_sight2.wav", 1, 0.8, 0, m_iVoicePitch );
		break;
	}
}

void CXenome::AttackHitSound( CBaseMonster *pVictim )
{
	// sic: the large scorpion's sound
	EMIT_SOUND_DYN( pVictim->edict( ), CHAN_VOICE, "lg_scorp/lg_scorp_attackb.wav", 1, 2.0, 0, 140 );
}

//=========================================================
// Classify - a faded xenome passes for a critter
//=========================================================
int CXenome::Classify( void )
{
	if ( pev->renderamt < 55 )
		return CLASS_ALIEN_CRITTER;

	return CLASS_ALIEN_TUBE;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CXenome::SetYawSpeed( void )
{
	int ys;

	ys = 120;

	switch ( m_Activity )
	{
	case ACT_CROUCHIDLE:
		ys = 0;
		break;
	case ACT_RUN:
		ys = 120;
		break;
	case ACT_IDLE:
	case ACT_WALK:
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 90;
		break;
	case ACT_LEAP:
		ys = 30;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// LeapTouch - the leap claws what it lands on
//=========================================================
void CXenome::LeapTouch( CBaseEntity *pOther )
{
	if ( !pOther->pev->takedamage )
		return;

	if ( pOther->Classify( ) == Classify( ) )
		return;

	// still in the air
	if ( pev->flags & FL_ONGROUND )
		return;

	pOther->TakeDamage( pev, pev, gSkillData.xenomeDmgMelee, DMG_SLASH );

	CBaseMonster *pMonster = pOther->MyMonsterPointer( );
	if ( pMonster )
	{
		TraceResult tr;

		UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
		// sic: it bleeds by the beak's scrape damage
		pMonster->TraceBleed( gSkillData.beakDmgScrape, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize( ), &tr, DMG_SLASH, tr.vecEndPos );

		if ( pMonster->pev->flags & ( FL_CLIENT | FL_MONSTER ) )
			AttackHitSound( pMonster );
	}

	SetTouch( NULL );
}

//=========================================================
// GetShootPosition - a dart comes from either side of the
// head
//=========================================================
Vector CXenome::GetShootPosition( int iRight )
{
	if ( iRight )
		return pev->origin + gpGlobals->v_right * 25 + gpGlobals->v_up * 20;

	return pev->origin + gpGlobals->v_right * -25 + gpGlobals->v_up * 20;
}

void CXenome::ShootDart( int iRight )
{
	if ( m_hEnemy )
	{
		Vector vecSrc = GetShootPosition( iRight );
		Vector vecDir = ShootAtEnemy( vecSrc );

		vecDir = vecDir.Normalize( ) * 2000;

		CXenomeShot::Shoot( pev, vecSrc, vecDir );
		RangeAttackSound( );
	}
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CXenome::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case XENOME_AE_SHOOT_LEFT:
		ShootDart( 0 );
		break;

	case XENOME_AE_SHOOT_RIGHT:
		ShootDart( 1 );
		break;

	case XENOME_AE_CLAW:
	case XENOME_AE_CLAW2:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 64, gSkillData.xenomeDmgMelee, DMG_SLASH );

			if ( pHurt )
			{
				MeleeAttackSound( );
				pHurt->pev->punchangle.z = RANDOM_LONG( -10, 10 );
				pHurt->pev->punchangle.x = RANDOM_LONG( -10, 10 );

				CBaseMonster *pMonster = pHurt->MyMonsterPointer( );
				if ( pMonster )
				{
					TraceResult tr;

					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					pMonster->TraceBleed( gSkillData.xenomeDmgMelee, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize( ), &tr, DMG_SLASH, tr.vecEndPos );

					if ( pMonster->IsPlayer( ) )
						AttackHitSound( pMonster );
				}
			}
		}
		break;

	case XENOME_AE_LEAP:
	case XENOME_AE_LEAP2:
		{
			UTIL_MakeAimVectors( pev->angles );
			pev->flags &= ~FL_ONGROUND;

			float flGravity = CVAR_GET_FLOAT( "sv_gravity" );

			pev->velocity = gpGlobals->v_forward * 500;
			pev->velocity.z += ( 0.625 * flGravity ) * 0.7;

			m_flNextJump = gpGlobals->time + 3.0;
			SetTouch( &CXenome::LeapTouch );
		}
		break;

	case 8:
		break;

	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards.
//=========================================================
int CXenome::ISoundMask( void )
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_PLAYER	|
			bits_SOUND_CARCASS	|
			bits_SOUND_MEAT		|
			bits_SOUND_DANGER	|
			bits_SOUND_GARBAGE;
}

BOOL CXenome::CheckMeleeAttack1( float flDot, float flDist )
{
	if ( m_IdealActivity != ACT_MELEE_ATTACK2 && flDist <= 48 && m_hEnemy != NULL )
		return TRUE;

	return FALSE;
}

//=========================================================
// CheckMeleeAttack2 - the leap, at an enemy that faces it
//=========================================================
BOOL CXenome::CheckMeleeAttack2( float flDot, float flDist )
{
	if ( m_flNextJump <= gpGlobals->time && HasConditions( bits_COND_SEE_ENEMY ) && HasConditions( bits_COND_ENEMY_FACING_ME ) )
		return TRUE;

	return FALSE;
}

//=========================================================
// CheckRangeAttack1 - a dart from the right side of the
// head or else from the left must reach the enemy
//=========================================================
BOOL CXenome::CheckRangeAttack1( float flDot, float flDist )
{
	if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) || flDist <= 84 || flDist > 2048 )
		return FALSE;

	TraceResult tr;
	Vector vecSrc;

	vecSrc = GetShootPosition( 1 );

	UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget( vecSrc ), dont_ignore_monsters, ENT(pev), &tr );

	if ( tr.flFraction == 1 || tr.pHit == m_hEnemy->edict( ) )
		return TRUE;

	vecSrc = GetShootPosition( 0 );

	UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget( vecSrc ), dont_ignore_monsters, ENT(pev), &tr );

	if ( tr.flFraction == 1 || tr.pHit == m_hEnemy->edict( ) )
		return TRUE;

	return FALSE;
}

//=========================================================
// RunAI - it fades out while it stalks and fades in when it
// attacks, is in the air, has no enemy or is dying
//=========================================================
void CXenome::RunAI( void )
{
	int iAlpha;

	CBaseMonster::RunAI( );

	switch ( m_Activity )
	{
	case ACT_LAND:
		pev->framerate = 1.8;
		break;
	case ACT_RANGE_ATTACK1:
	case ACT_MELEE_ATTACK1:
		pev->framerate = 1.4;
		break;
	case ACT_WALK:
	case ACT_RUN:
		pev->framerate = 1.4;
		break;
	default:
		pev->framerate = 1.3;
		break;
	}

	switch ( g_iSkillLevel )
	{
	case SKILL_MEDIUM:
		iAlpha = 15;
		break;
	case SKILL_HARD:
		iAlpha = 0;
		break;
	default:
		iAlpha = 45;
		break;
	}

	if ( HasConditions( bits_COND_ENEMY_DEAD ) )
	{
		iAlpha = 255;
	}
	else
	{
		switch ( m_Activity )
		{
		case ACT_TURN_LEFT:
		case ACT_TURN_RIGHT:
			iAlpha = 10;	// sic: falls through and adds 10 more
		case ACT_WALK:
		case ACT_RUN:
			iAlpha += 10;
			break;
		case ACT_RANGE_ATTACK1:
		case ACT_MELEE_ATTACK1:
			iAlpha += 255;
			break;
		default:
			iAlpha += 10;
			break;
		}
	}

	if ( m_hEnemy == NULL )
		iAlpha += 90;

	if ( !( pev->flags & FL_ONGROUND ) )
		iAlpha += 125;

	if ( pev->deadflag )
		iAlpha = 255;

	m_iTargetAlpha = iAlpha;
	if ( iAlpha > 255 )
		m_iTargetAlpha = 255;
	else if ( iAlpha < 0 )
		m_iTargetAlpha = 0;

	float flTargetAlpha = m_iTargetAlpha;

	if ( pev->renderamt > flTargetAlpha )
	{
		pev->renderamt = max( pev->renderamt - 30, flTargetAlpha );
		pev->rendermode = kRenderTransTexture;
	}
	else if ( pev->renderamt < flTargetAlpha )
	{
		pev->renderamt = min( pev->renderamt + 50, flTargetAlpha );

		if ( pev->renderamt == 255 )
		{
			pev->rendermode = kRenderNormal;
			pev->renderfx = kRenderFxNone;
		}
	}

	// hard to aim at while it is faded
	if ( pev->renderamt > 140 )
		pev->takedamage = DAMAGE_AIM;
	else
		pev->takedamage = DAMAGE_YES;
}

//=========================================================
// StartTask
//=========================================================
void CXenome::StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_MELEE_ATTACK1:
		MeleeAttackSound( );
		CBaseMonster::StartTask( pTask );
		// sic: falls through and starts the task again
	default:
		CBaseMonster::StartTask( pTask );
		break;

	case TASK_XENOME_ROAR:
		if ( m_tNextSoundTime < gpGlobals->time )
		{
			m_IdealActivity = ACT_THREAT_DISPLAY;
			m_tNextSoundTime = gpGlobals->time + 9;
		}
		else
		{
			TaskFail( );
		}
		break;

	case TASK_XENOME_LEAP_AWAY:
		{
			pev->ideal_yaw += RANDOM_FLOAT( -20, 20 );

			if ( HasConditions( bits_COND_SEE_ENEMY ) )
			{
				TaskFail( );
				break;
			}

			m_IdealActivity = ACT_LEAP;

			float flGravity = CVAR_GET_FLOAT( "sv_gravity" );
			pev->flags &= ~FL_ONGROUND;

			pev->velocity = gpGlobals->v_forward * 100 * RANDOM_FLOAT( -1, 1 );
			pev->velocity.z += RANDOM_FLOAT( 0.1, 0.4 ) * ( flGravity * 0.5 );
			pev->velocity.x += RANDOM_FLOAT( -0.4, 0.4 ) * ( flGravity * 0.5 );
		}
		break;
	}
}

//=========================================================
// RunTask
//=========================================================
void CXenome::RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_XENOME_ROAR:
		if ( m_fSequenceFinished )
			TaskComplete( );
		break;

	case TASK_XENOME_LEAP_AWAY:
		ChangeYaw( pev->yaw_speed );

		if ( pev->flags & FL_ONGROUND )
			TaskComplete( );

		if ( m_fSequenceFinished )
			TaskComplete( );
		break;

	default:
		CBaseMonster::RunTask( pTask );
		break;
	}
}

//=========================================================
// GetSchedule
//=========================================================
Schedule_t *CXenome::GetSchedule( void )
{
	switch ( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		{
			// dead enemy
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
				break;

			if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			{
				if ( UTIL_PercentChance( 10 ) )
				{
					if ( UTIL_PercentChance( 30 ) )
						return GetScheduleOfType( SCHED_SMALL_FLINCH );

					return GetScheduleOfType( SCHED_BIG_FLINCH );
				}

				if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
					return GetScheduleOfType( SCHED_MELEE_ATTACK2 );
			}

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK2 );
		}
		break;

	case MONSTERSTATE_ALERT:
		{
			if ( HasConditions( bits_COND_NEW_ENEMY | bits_COND_SEE_ENEMY ) )
				m_IdealMonsterState = MONSTERSTATE_COMBAT;

			if ( HasConditions( bits_COND_HEAR_SOUND ) )
			{
				CSound *pSound;
				pSound = PBestSound( );

				if ( pSound && ( pSound->m_iType & ( bits_SOUND_DANGER | bits_SOUND_COMBAT ) ) )
				{
					MakeIdealYaw( pSound->m_vecOrigin );
					return &slXenomeUrgent[ 0 ];
				}
			}

			if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			{
				if ( UTIL_PercentChance( 10 ) )
				{
					if ( UTIL_PercentChance( 30 ) )
						return GetScheduleOfType( SCHED_SMALL_FLINCH );
					return GetScheduleOfType( SCHED_BIG_FLINCH );
				}
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ORIGIN );
			}
		}
		break;
	}

	return CBaseMonster::GetSchedule( );
}

//=========================================================
// GetScheduleOfType
//=========================================================
Schedule_t *CXenome::GetScheduleOfType( int Type )
{
	switch ( Type )
	{
	case SCHED_MELEE_ATTACK1:
		// a squad leader roars now and then
		if ( IsLeader( ) && InSquad( ) && UTIL_PercentChance( 5 ) )
			return &slXenomeRoar[ 0 ];
		break;

	case SCHED_MELEE_ATTACK2:
		return &slXenomeJumpMelee[ 0 ];

	case SCHED_FAIL:
		if ( m_MonsterState == MONSTERSTATE_COMBAT )
		{
			// a player can see it
			if ( !FNullEnt( FIND_CLIENT_IN_PVS( edict( ) ) ) )
				return &slXenomeCombatFail[ 0 ];
		}
		else
		{
			return &slXenomeFail[ 0 ];
		}
		break;
	}

	return CSquadMonster::GetScheduleOfType( Type );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CXenome::Precache( void )
{
	PRECACHE_MODEL( "models/xenome.mdl" );
	UTIL_PrecacheOther( "xenomeshot" );

	g_iGibXeno = PRECACHE_MODEL( "sprites/gibxeno.spr" );
	g_iGoreXeno = PRECACHE_MODEL( "sprites/gorexeno.spr" );

	CBaseMonster::Precache( );
}

//=========================================================
// Spawn
//=========================================================
void CXenome::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT(pev), "models/xenome.mdl" );
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 48 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->health			= gSkillData.xenomeHealth;
	m_flFieldOfView		= -0.7;
	m_iVoicePitch		= RANDOM_LONG( -10, 10 ) + 90;
	m_HackedGunPos		= Vector( 0, 24, 48 );

	// it starts out of sight
	m_iTargetAlpha		= 0;
	pev->renderamt		= 0;
	pev->rendermode		= kRenderTransTexture;

	m_flNextJump		= gpGlobals->time;
	m_afCapability		|= bits_CAP_SQUAD;

	MonsterInit( );

	pev->view_ofs		= Vector( 0, 0, 40 );
	m_afCapability		|= bits_CAP_MELEE_ATTACK2;
}
