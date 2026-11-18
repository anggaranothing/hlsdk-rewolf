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
// monster_scorpion and monster_largescorpion: the mayan
// scorpions, written from the headcrab. They charge at
// their enemy, claw and sting it, and tilt to the slope
// they walk on. A player squashes the small one by
// touching it.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "vehicletank.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define SCORPION_AE_CLAW		( 1 )
#define SCORPION_AE_STING		( 2 )

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlHCMeleeAttack1[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_MELEE_ATTACK1,		(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
};

Schedule_t	slHCMeleeAttack1[] =
{
	{
		tlHCMeleeAttack1,
		ARRAYSIZE ( tlHCMeleeAttack1 ),
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED,
		0,
		"HCMeleeAttack1"
	},
};

// the scorpion's range attack is a run at its enemy
Task_t	tlScorpionCharge[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_COMBAT_FACE	},
	{ TASK_GET_PATH_TO_ENEMY,	(float)0					},
	{ TASK_RUN_PATH,			(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0					},
};

Schedule_t	slScorpionCharge[] =
{
	{
		tlScorpionCharge,
		ARRAYSIZE ( tlScorpionCharge ),
		bits_COND_NEW_ENEMY			|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_CAN_MELEE_ATTACK2	|
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED,
		0,
		"ScorpionCharge"
	},
};

class CScorpion : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void PrecacheSounds( void );
	void KeyValue( KeyValueData *pkvd );
	int  Classify( void ) { return CLASS_ALIEN_PREDATOR; }
	int  MonsterType( void ) { return 10; }
	void SetYawSpeed( void );
	void EXPORT SquashTouch( CBaseEntity *pOther );
	Vector Center( void );
	Vector BodyTarget( const Vector &posSrc );
	void PainSound( void );
	void DeathSound( void );
	void IdleSound( void );
	void GibSound( void );
	void AttackHitSound( CBaseMonster *pVictim );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void RunAI( void );
	void AlignToGround( void );
	void ScamperSound( void );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	BOOL CheckMeleeAttack2( float flDot, float flDist );

	// the headcrab's; nothing calls the first two
	virtual float GetDamageAmount( void ) { return gSkillData.smlscorpDmgSting; }
	virtual int GetVoicePitch( void ) { return 100; }
	virtual float GetSoundVolue( void ) { return 1.0; }
	Schedule_t *GetScheduleOfType( int Type );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

	static const char *pSmallSounds[];
	static const char *pLargeSounds[];

	float	m_flUnused;		// never used
	int		m_iSkin;		// the "_skin" keyvalue, for a mapper's skin 0
};

class CLargeScorpion : public CScorpion
{
public:
	void Spawn( void );
	void Precache( void );
	void PrecacheSounds( void );
	int  MonsterType( void ) { return 11; }
	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin - Vector( 24, 24, 0 );
		pev->absmax = pev->origin + Vector( 24, 24, 36 );
	}
	int  Classify( void ) { return CLASS_ALIEN_PREDATOR; }
	void SetYawSpeed( void );
	void PainSound( void );
	void DeathSound( void );
	void IdleSound( void );
	void AttackHitSound( CBaseMonster *pVictim );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void RunAI( void );
	void ScamperSound( void );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckMeleeAttack2( float flDot, float flDist );

	float	m_flNextMeleeTime;	// the claw waits 2 to 2.5 seconds between swipes
};

LINK_ENTITY_TO_CLASS( monster_scorpion, CScorpion );

TYPEDESCRIPTION	CScorpion::m_SaveData[] =
{
	DEFINE_FIELD( CScorpion, m_iSkin, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CScorpion, CBaseMonster );

DEFINE_CUSTOM_SCHEDULES( CScorpion )
{
	slHCMeleeAttack1,
	slScorpionCharge,
};

IMPLEMENT_CUSTOM_SCHEDULES( CScorpion, CBaseMonster );

const char *CScorpion::pSmallSounds[] =
{
	"sm_scorp/sm_scorp_attacka.wav",
	"sm_scorp/sm_scorp_attackb.wav",
	"sm_scorp/sm_scorp_die.wav",
	"sm_scorp/sm_scorp_flinch.wav",
	"sm_scorp/sm_scorp_idle1.wav",
	"sm_scorp/sm_scorp_idle2.wav",
	"sm_scorp/sm_scorp_scamper.wav",
	"sm_scorp/sm_scorp_touchdown.wav",
	"sm_scorp/sm_scorp_jump.wav",
};

const char *CScorpion::pLargeSounds[] =
{
	"lg_scorp/lg_scorp_alert.wav",
	"lg_scorp/lg_scorp_attacka.wav",
	"lg_scorp/lg_scorp_attackb.wav",
	"lg_scorp/lg_scorp_die.wav",
	"lg_scorp/lg_scorp_flinch.wav",
	"lg_scorp/lg_scorp_idle1.wav",
	"lg_scorp/lg_scorp_idle2.wav",
	"lg_scorp/lg_scorp_idle3.wav",
	"lg_scorp/lg_scorp_scamper.wav",
};

void CScorpion::PrecacheSounds( void )
{
	for ( int i = 0; i < ARRAYSIZE( pSmallSounds ); i++ )
		PRECACHE_SOUND( (char *)pSmallSounds[i] );
}

void CScorpion::PainSound( void )
{
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "sm_scorp/sm_scorp_flinch.wav", GetSoundVolue(), ATTN_IDLE );
}

void CScorpion::DeathSound( void )
{
	EMIT_SOUND( ENT(pev), CHAN_BODY, "sm_scorp/sm_scorp_die.wav", GetSoundVolue(), ATTN_IDLE );
}

void CScorpion::IdleSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "sm_scorp/sm_scorp_idle1.wav", 1, ATTN_IDLE, 0, 100 );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "sm_scorp/sm_scorp_idle2.wav", 1, ATTN_IDLE, 0, 100 );
		break;
	}
}

//=========================================================
// Center - returns the real center of the scorpion.  The
// bounding box is much larger than the actual creature so
// this is needed for targeting
//=========================================================
Vector CScorpion::Center( void )
{
	return Vector( pev->origin.x, pev->origin.y, pev->origin.z + 6 );
}

Vector CScorpion::BodyTarget( const Vector &posSrc )
{
	return Center();
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CScorpion::SetYawSpeed( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
		ys = 50;
		break;
	case ACT_RUN:
	case ACT_WALK:
		ys = 70;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
	case ACT_MELEE_ATTACK1:
	case ACT_MELEE_ATTACK2:
		ys = 90;
		break;
	default:
		ys = 30;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// SquashTouch - a player that touches the scorpion
// squashes it
//=========================================================
void CScorpion::SquashTouch( CBaseEntity *pOther )
{
	if ( pOther && pOther->IsPlayer() )
	{
		TakeDamage( pOther->pev, pOther->pev, 100, DMG_CRUSH );
	}
}

void CScorpion::AttackHitSound( CBaseMonster *pVictim )
{
	EMIT_SOUND_DYN( ENT(pVictim->pev), CHAN_VOICE, "lg_scorp/lg_scorp_attackb.wav", 0.1, ATTN_IDLE, 0, 70 );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CScorpion::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case SCORPION_AE_CLAW:
		{
			CBaseEntity *pHurt = CheckEnemyMeleeHit( 64, gSkillData.smlscorpDmgClaw, DMG_SLASH, 3, 0 );
			if ( pHurt )
			{
				CBaseMonster *pMonster = pHurt->MyMonsterPointer();
				if ( pMonster )
				{
					TraceResult tr;

					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					pMonster->TraceBleed( gSkillData.smlscorpDmgClaw, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

					if ( pHurt->IsPlayer() )
						AttackHitSound( pMonster );
				}
			}
		}
		break;

	case SCORPION_AE_STING:
		{
			CBaseEntity *pHurt = CheckEnemyMeleeHit( 64, gSkillData.smlscorpDmgSting, DMG_SLASH, 3, 0 );
			if ( pHurt )
			{
				CBaseMonster *pMonster = pHurt->MyMonsterPointer();
				if ( pMonster )
				{
					TraceResult tr;

					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					// sic: bleeds as much as the claw
					pMonster->TraceBleed( gSkillData.smlscorpDmgClaw, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

					if ( pHurt->IsPlayer() )
						AttackHitSound( pMonster );
				}
			}
		}
		break;

	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

void CScorpion::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "_skin" ) )
	{
		m_iSkin = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}

void CScorpion::Precache( void )
{
	PRECACHE_MODEL( "models/scorpion_small.mdl" );

	CBaseMonster::Precache();
}

void CScorpion::Spawn( void )
{
	Precache();

	SET_MODEL( ENT(pev), "models/scorpion_small.mdl" );
	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 24 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->health			= UTIL_RandomizeHealth( gSkillData.smlscorpHealth );
	m_flFieldOfView		= 0.5;

	if ( !pev->skin )
		pev->skin = m_iSkin;

	SetTouch( &CScorpion::SquashTouch );

	MonsterInit();

	pev->view_ofs		= Vector( 0, 0, 20 );
	m_afCapability		&= ~bits_CAP_MELEE_ATTACK1;
	m_afCapability		|= bits_CAP_RANGE_ATTACK1;
}

void CScorpion::RunAI( void )
{
	CBaseMonster::RunAI();

	AlignToGround();
	ScamperSound();
}

//=========================================================
// AlignToGround - tilt to the slope under the scorpion:
// pitch by how much it faces along the slope, roll by how
// much it faces across it
//=========================================================
void CScorpion::AlignToGround( void )
{
	TraceResult tr;
	Vector vecAngles, vecUp;

	UTIL_TraceLine( pev->origin + Vector( 0, 0, 8 ), pev->origin - Vector( 0, 0, 2048 ), ignore_monsters, ENT(pev), &tr );

	vecAngles = UTIL_VecToAngles( tr.vecPlaneNormal );
	UTIL_MakeVectorsPrivate( vecAngles, NULL, NULL, vecUp );

	vecUp = UTIL_VecToAngles( vecUp );
	vecUp.x = vecUp.x * -1;
	vecUp.z = pev->origin.z - tr.vecEndPos.z;	// sic: the height above the ground, never used

	vecUp.x = UTIL_AngleNormalize( vecUp.x );
	vecUp.y = UTIL_AngleNormalize( vecUp.y );

	float flYaw = UTIL_AngleDiff( pev->angles.y, vecUp.y );

	float flFront = flYaw / 180;
	if ( flFront < 0 )
		flFront = flFront * -1;
	float flPitch = ( 2 - flFront * 2 - 1 ) * vecUp.x;

	float flSide = UTIL_AngleNormalize( flYaw - 90 ) / 180;
	if ( flSide < 0 )
		flSide = flSide * -1;

	float flRoll = ( 2 - flSide * 2 - 1 ) * vecUp.x;

	pev->angles.x = flPitch;
	pev->angles.z = -flRoll;
}

//=========================================================
// ScamperSound - every other think while it walks or runs
//=========================================================
void CScorpion::ScamperSound( void )
{
	static BOOL fScamper = FALSE;

	if ( m_Activity == ACT_RUN || m_Activity == ACT_WALK )
	{
		fScamper = !fScamper;
		if ( fScamper )
			EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "sm_scorp/sm_scorp_scamper.wav", 0.5, ATTN_NORM, 0, 100 );
	}
}

void CScorpion::GibSound( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "common/smallsplat.wav", 0.3, ATTN_NORM, 0, 100 );
}

//=========================================================
// CheckRangeAttack1 - charge from 64 to 256 units away
//=========================================================
BOOL CScorpion::CheckRangeAttack1( float flDot, float flDist )
{
	if ( flDist < 256 && flDist > 64 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckMeleeAttack2 - the headcrab's leap test, as the
// sting
//=========================================================
BOOL CScorpion::CheckMeleeAttack2( float flDot, float flDist )
{
	if ( FBitSet( pev->flags, FL_ONGROUND ) && flDist <= 64 && flDot >= 0.65 )
	{
		return TRUE;
	}
	return FALSE;
}

Schedule_t *CScorpion::GetScheduleOfType( int Type )
{
	switch	( Type )
	{
		case SCHED_RANGE_ATTACK1:
		{
			return &slScorpionCharge[ 0 ];
		}
		break;
	}

	return CBaseMonster::GetScheduleOfType( Type );
}

LINK_ENTITY_TO_CLASS( monster_largescorpion, CLargeScorpion );

void CLargeScorpion::PrecacheSounds( void )
{
	for ( int i = 0; i < ARRAYSIZE( pLargeSounds ); i++ )
		PRECACHE_SOUND( (char *)pLargeSounds[i] );
}

void CLargeScorpion::PainSound( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "lg_scorp/lg_scorp_flinch.wav", 1, ATTN_IDLE, 0, 100 );
}

void CLargeScorpion::DeathSound( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "lg_scorp/lg_scorp_die.wav", 1, ATTN_IDLE, 0, 100 );
}

void CLargeScorpion::IdleSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "lg_scorp/lg_scorp_idle1.wav", 1, ATTN_IDLE, 0, 100 );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "lg_scorp/lg_scorp_idle2.wav", 1, ATTN_IDLE, 0, 100 );
		break;
	case 2:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "lg_scorp/lg_scorp_idle3.wav", 1, ATTN_IDLE, 0, 100 );
		break;
	}
}

void CLargeScorpion::SetYawSpeed( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
	case ACT_RUN:
	case ACT_WALK:
		ys = 120;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 180;
		break;
	case ACT_MELEE_ATTACK1:
	case ACT_MELEE_ATTACK2:
		ys = 90;
		break;
	default:
		ys = 120;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - the sting also greys the victim's view
//=========================================================
void CLargeScorpion::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case SCORPION_AE_CLAW:
		{
			CBaseEntity *pHurt = CheckEnemyMeleeHit( 64, gSkillData.lrgscorpDmgClaw, DMG_SLASH, 20, 10 );
			if ( pHurt )
			{
				CBaseMonster *pMonster = pHurt->MyMonsterPointer();
				if ( pMonster )
				{
					TraceResult tr;

					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					pMonster->TraceBleed( gSkillData.lrgscorpDmgClaw, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

					if ( pHurt->IsPlayer() )
						AttackHitSound( pMonster );
				}
			}
		}
		break;

	case SCORPION_AE_STING:
		{
			CBaseEntity *pHurt = CheckEnemyMeleeHit( 64, gSkillData.lrgscorpDmgSting, DMG_SLASH, 20, 10 );
			if ( pHurt )
			{
				UTIL_ScreenFade( pHurt, Vector( 40, 40, 40 ), 2, 0.5, 150, 0 );

				CBaseMonster *pMonster = pHurt->MyMonsterPointer();
				if ( pMonster )
				{
					TraceResult tr;

					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					pMonster->TraceBleed( gSkillData.lrgscorpDmgSting, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

					if ( pHurt->IsPlayer() )
						AttackHitSound( pMonster );
				}
			}
		}
		break;

	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

void CLargeScorpion::RunAI( void )
{
	CBaseMonster::RunAI();

	// sic: no breaks, so it always ends up at 1.5
	switch ( m_Activity )
	{
	case ACT_WALK:
	case ACT_RUN:
		pev->framerate = 1.7;
	case ACT_MELEE_ATTACK1:
	case ACT_MELEE_ATTACK2:
		pev->framerate = 1.5;
	default:
		pev->framerate = 1.5;
	}

	// sic: ScamperSound is never called
	AlignToGround();
}

void CLargeScorpion::Precache( void )
{
	PRECACHE_MODEL( "models/scorpion_large.mdl" );

	CScorpion::Precache();
}

void CLargeScorpion::ScamperSound( void )
{
	static BOOL fScamper = FALSE;

	if ( m_Activity == ACT_RUN || m_Activity == ACT_WALK )
	{
		fScamper = !fScamper;
		if ( fScamper )
			EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "lg_scorp/lg_scorp_scamper.wav", 0.5, ATTN_NORM, 0, 100 );
	}
}

void CLargeScorpion::Spawn( void )
{
	Precache();

	SET_MODEL( ENT(pev), "models/scorpion_large.mdl" );
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 36 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->health			= 100;	// sic: gSkillData.lrgscorpHealth goes unused
	m_flFieldOfView		= -0.7;

	if ( !pev->skin )
		pev->skin = m_iSkin;

	MonsterInit();

	pev->view_ofs		= Vector( 0, 0, 20 );
	m_afCapability		|= bits_CAP_RANGE_ATTACK1;
}

void CLargeScorpion::AttackHitSound( CBaseMonster *pVictim )
{
	EMIT_SOUND_DYN( ENT(pVictim->pev), CHAN_VOICE, "lg_scorp/lg_scorp_attackb.wav", 0.5, ATTN_IDLE, 0, 60 );
}

//=========================================================
// CheckRangeAttack1 - charge from 64 to 512 units away
//=========================================================
BOOL CLargeScorpion::CheckRangeAttack1( float flDot, float flDist )
{
	if ( flDist < 512 && flDist > 64 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckMeleeAttack1 - the claw, every 2 to 2.5 seconds
//=========================================================
BOOL CLargeScorpion::CheckMeleeAttack1( float flDot, float flDist )
{
	if ( m_flNextMeleeTime < gpGlobals->time && FBitSet( pev->flags, FL_ONGROUND ) && flDist <= 64 && flDot >= 0.65 )
	{
		m_flNextMeleeTime = gpGlobals->time + RANDOM_FLOAT( 2, 2.5 );
		return TRUE;
	}
	return FALSE;
}

BOOL CLargeScorpion::CheckMeleeAttack2( float flDot, float flDist )
{
	if ( FBitSet( pev->flags, FL_ONGROUND ) && flDist <= 64 && flDot >= 0.65 )
	{
		return TRUE;
	}
	return FALSE;
}
