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
// monster_beak: a squad monster that bites, scrapes and
// leaps at its enemy, roars at it and eats what it smells
// when it is calm; the microraptor's code with a leap that
// leads the enemy. Each beak is born with its own strength
// and stamina: the strong ones have deeper voices, the
// others run faster at a near enemy.
// monster_beakbirther: the beak queen, a floating sack that
// bursts into beak embryos and leaves beakbirther_gib.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"squadmonster.h"
#include	"soundent.h"

// gib and gore spray sprites of the beaks and their queen
int g_iGibBirth;
int g_iGoreBeak;
int g_iGibQueen;	// sic: never precached; the queen's spray uses it
int g_iBirdGib;	// sprites/gibbeak.spr (name ours)
int g_iGoreBirth;

// sic: retail's .bss has 4 bytes after g_iGoreBirth, before critter.obj's,
// that nothing reads or writes (name ours)
static int s_iUnknown1;

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_BEAK_EAT = LAST_COMMON_SCHEDULE + 1,
	SCHED_BEAK_PRETEND_EAT,
};

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_BEAK_EAT = LAST_COMMON_TASK + 1,	// face the food and chew until interrupted
	TASK_BEAK_THREAT_DISPLAY,				// roar, if it hasn't for a while
	TASK_BEAK_UNUSED,						// no code handles it
	TASK_BEAK_JUMP_MELEE,					// leap at the enemy, scraping
	TASK_BEAK_GET_TO_LEADER,				// walk up to the squad leader
	TASK_BEAK_LEAP_AWAY,					// leap off in a random direction
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define BEAK_AE_BITE		( 4 )
#define BEAK_AE_BITE2		( 5 )
#define BEAK_AE_BITE3		( 6 )
#define BEAK_AE_SCRAPE		( 7 )
#define BEAK_AE_SCRAPE2		( 9 )

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

// leap at the enemy, then chase it
Task_t	tlBeakJumpMelee[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_CHASE_ENEMY_FAILED	},
	{ TASK_FACE_ENEMY,			(float)0						},
	{ TASK_STOP_MOVING,			(float)0						},
	{ TASK_BEAK_JUMP_MELEE,		(float)0						},
	{ TASK_SET_SCHEDULE,		(float)SCHED_CHASE_ENEMY		},
};

Schedule_t	slBeakJumpMelee[] =
{
	{
		tlBeakJumpMelee,
		ARRAYSIZE ( tlBeakJumpMelee ),
		0,
		0,
		"Beak Secondary Jump-Melee"
	},
};

// roar, then chase the enemy
Task_t	tlBeakRoar[] =
{
	{ TASK_STOP_MOVING,			(float)0				},
	{ TASK_BEAK_THREAT_DISPLAY,	(float)0				},
	{ TASK_SET_SCHEDULE,		(float)SCHED_CHASE_ENEMY	},
};

Schedule_t	slBeakRoar[] =
{
	{
		tlBeakRoar,
		ARRAYSIZE ( tlBeakRoar ),
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE,
		0,
		"Beak Roaring"
	},
};

// leap away from a dangerous sound, then take cover
Task_t	tlBeakUrgent[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_CHASE_ENEMY_FAILED		},
	{ TASK_BEAK_LEAP_AWAY,		(float)0							},
	{ TASK_SET_SCHEDULE,		(float)SCHED_TAKE_COVER_FROM_ORIGIN	},
};

Schedule_t	slBeakUrgent[] =
{
	{
		tlBeakUrgent,
		ARRAYSIZE ( tlBeakUrgent ),
		0,
		0,
		"Urgent"
	},
};

// walk to the food and eat it
Task_t	tlBeakEat[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_BEAK_PRETEND_EAT	},
	{ TASK_STOP_MOVING,				(float)0						},
	{ TASK_STORE_LASTPOSITION,		(float)0						},
	{ TASK_GET_PATH_TO_BESTSCENT,	(float)0						},
	{ TASK_WALK_PATH,				(float)0						},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0						},
	{ TASK_BEAK_EAT,				(float)0						},
};

Schedule_t	slBeakEat[] =
{
	{
		tlBeakEat,
		ARRAYSIZE ( tlBeakEat ),
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
Task_t	tlBeakPretendEat[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_IDLE_STAND			},
	{ TASK_STOP_MOVING,				(float)0						},
	{ TASK_STORE_LASTPOSITION,		(float)0						},
	{ TASK_BEAK_GET_TO_LEADER,		(float)50						},
	{ TASK_BEAK_EAT,				(float)0						},
};

Schedule_t	slBeakPretendEat[] =
{
	{
		tlBeakPretendEat,
		ARRAYSIZE ( tlBeakPretendEat ),
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

// the squad leader stands and looks about
Task_t	tlBeakLookAround[] =
{
	{ TASK_STOP_MOVING,			(float)0						},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_SPECIAL_ATTACK1		},
	{ TASK_FACE_ENEMY,			(float)0						},
};

Schedule_t	slBeakLookAround[] =
{
	{
		tlBeakLookAround,
		ARRAYSIZE ( tlBeakLookAround ),
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_PROVOKED			|
		bits_COND_NEW_ENEMY			|
		bits_COND_HEAR_SOUND,
		bits_SOUND_COMBAT			|
		bits_SOUND_PLAYER			|
		bits_SOUND_DANGER,
		"Looking Around"
	},
};

// failed with an enemy in view: face it and roar
Task_t	tlBeakCombatFail[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_BEAK_THREAT_DISPLAY,	(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
};

Schedule_t	slBeakCombatFail[] =
{
	{
		tlBeakCombatFail,
		ARRAYSIZE ( tlBeakCombatFail ),
		bits_COND_NEW_ENEMY			|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK2	|
		bits_COND_HEAVY_DAMAGE,
		bits_SOUND_DANGER			|
		bits_SOUND_PLAYER,
		"Failed with PVS"
	},
};

// failed: roar, then wait for a player to come into view
Task_t	tlBeakFail[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_BEAK_THREAT_DISPLAY,	(float)0		},
	{ TASK_WAIT_FACE_ENEMY,		(float)1		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slBeakFail[] =
{
	{
		tlBeakFail,
		ARRAYSIZE ( tlBeakFail ),
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

class CBeak : public CSquadMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void ) { return CLASS_ALIEN_BEAK; }
	int  MonsterType( void ) { return 15; }
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
	void PainSound( void );
	void DeathSound( void );
	void MiscSound( int iSound );
	void Attack( int iEvent );
	void EXPORT LeapTouch( CBaseEntity *pOther );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

	static const char *pAttackSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pDeathSounds[];
	static const char *pMiscSounds[];

	int		m_iVoicePitch;
	int		m_iRelaxed;			// no enemy since the last one died: it may go after food
	float	m_tNextSoundTime;	// pain, chewing and roars wait until then
	float	m_tNextMeleeTime;	// the leap waits until then
	float	m_fStrength;		// 0 to 1: deepens the voice
	float	m_fStam;			// 1 - m_fStrength: runs faster at a near enemy
};

LINK_ENTITY_TO_CLASS( monster_beak, CBeak );

TYPEDESCRIPTION	CBeak::m_SaveData[] =
{
	DEFINE_FIELD( CBeak, m_tNextMeleeTime, FIELD_TIME ),
	DEFINE_FIELD( CBeak, m_tNextSoundTime, FIELD_TIME ),
	DEFINE_FIELD( CBeak, m_iVoicePitch, FIELD_INTEGER ),
	DEFINE_FIELD( CBeak, m_iRelaxed, FIELD_INTEGER ),
	DEFINE_FIELD( CBeak, m_fStrength, FIELD_FLOAT ),
	DEFINE_FIELD( CBeak, m_fStam, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CBeak, CSquadMonster );

DEFINE_CUSTOM_SCHEDULES( CBeak )
{
	slBeakJumpMelee,
	slBeakEat,
	slBeakPretendEat,
	slBeakLookAround,
	slBeakUrgent,
	slBeakCombatFail,
	slBeakFail,
};

IMPLEMENT_CUSTOM_SCHEDULES( CBeak, CSquadMonster );

const char *CBeak::pAttackSounds[] =
{
	"beak/attack1.wav",
	"beak/attack2.wav",
};

const char *CBeak::pIdleSounds[] =
{
	"beak/suffer.wav",
};

const char *CBeak::pAlertSounds[] =
{
	"beak/alert1.wav",
};

const char *CBeak::pPainSounds[] =
{
	"beak/pain1.wav",
	"beak/pain2.wav",
};

const char *CBeak::pDeathSounds[] =
{
	"beak/die1.wav",
	"beak/die2.wav",
};

const char *CBeak::pMiscSounds[] =
{
	"beak/eating.wav",
	"beak/roar.wav",
};

//=========================================================
// IdleSound - one time in six
//=========================================================
void CBeak::IdleSound( void )
{
	if ( !RANDOM_LONG( 0, 5 ) )
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pIdleSounds[ RANDOM_LONG( 0, ARRAYSIZE( pIdleSounds ) - 1 ) ], 1.0, ATTN_NORM, 0, m_iVoicePitch );
}

void CBeak::AlertSound( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pAlertSounds[ RANDOM_LONG( 0, ARRAYSIZE( pAlertSounds ) - 1 ) ], 1.0, ATTN_NORM, 0, m_iVoicePitch );
}

//=========================================================
// PainSound - at most once a second
//=========================================================
void CBeak::PainSound( void )
{
	if ( m_tNextSoundTime < gpGlobals->time )
	{
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pPainSounds[ RANDOM_LONG( 0, ARRAYSIZE( pPainSounds ) - 1 ) ], 1.0, ATTN_NORM, 0, m_iVoicePitch );
		m_tNextSoundTime = gpGlobals->time + 1;
	}
}

void CBeak::DeathSound( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pDeathSounds[ RANDOM_LONG( 0, ARRAYSIZE( pDeathSounds ) - 1 ) ], 1.0, ATTN_NORM, 0, m_iVoicePitch );
}

//=========================================================
// MiscSound - 0 chews, 1 roars
//=========================================================
void CBeak::MiscSound( int iSound )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pMiscSounds[ iSound ], 1.0, ATTN_NORM, 0, m_iVoicePitch );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CBeak::SetYawSpeed( void )
{
	int ys = 120;

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
// TakeDamage - beaks don't hurt each other
//=========================================================
int CBeak::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( FClassnameIs( pevAttacker, "monster_beak" ) )
		return 0;

	return CSquadMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// LeapTouch - the leap scrapes what it hits on the way
//=========================================================
void CBeak::LeapTouch( CBaseEntity *pOther )
{
	// sic: these three keep the touch function
	if ( pOther->pev->takedamage == DAMAGE_NO )
		return;

	if ( pOther->Classify() == Classify() )
		return;

	if ( FBitSet( pev->flags, FL_ONGROUND ) )
		return;

	pOther->TakeDamage( pev, pev, gSkillData.beakDmgScrape, DMG_SLASH );

	CBaseMonster *pMonster = pOther->MyMonsterPointer();
	if ( pMonster )
	{
		TraceResult tr;

		UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
		pMonster->TraceBleed( gSkillData.beakDmgScrape, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

		if ( pMonster->pev->flags & ( FL_MONSTER | FL_CLIENT ) )
			AttackHitSound( pMonster );
	}

	SetTouch( NULL );
}

//=========================================================
// Attack - the bites and the scrapes; a scrape also hurts
// what the beak touches until it lands
//=========================================================
void CBeak::Attack( int iEvent )
{
	int iDamage;
	float flRange;

	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.2 );

	switch ( iEvent )
	{
	case BEAK_AE_BITE:
		flRange = 60;
		iDamage = gSkillData.beakDmgBite;
		break;

	case BEAK_AE_BITE2:
	case BEAK_AE_BITE3:
		flRange = 70;
		iDamage = gSkillData.beakDmgBite;
		break;

	case BEAK_AE_SCRAPE:
	case BEAK_AE_SCRAPE2:
		flRange = 60;
		iDamage = gSkillData.beakDmgScrape;
		SetTouch( &CBeak::LeapTouch );
		break;

	default:
		return;
	}

	CBaseEntity *pHurt = CheckTraceHullAttack( flRange, iDamage, DMG_SLASH );
	if ( pHurt )
	{
		pHurt->pev->punchangle.z = RANDOM_FLOAT( -5, 5 );
		pHurt->pev->punchangle.x = RANDOM_FLOAT( -5, 5 );

		CBaseMonster *pMonster = pHurt->MyMonsterPointer();
		if ( pMonster )
		{
			TraceResult tr;

			UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
			pMonster->TraceBleed( iDamage, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );

			if ( pMonster->pev->flags & ( FL_MONSTER | FL_CLIENT ) )
				AttackHitSound( pMonster );
		}
	}
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CBeak::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case BEAK_AE_BITE:
		Attack( BEAK_AE_BITE );
		break;

	case BEAK_AE_BITE2:
		Attack( BEAK_AE_BITE2 );
		break;

	case BEAK_AE_BITE3:
		Attack( BEAK_AE_BITE3 );
		break;

	case BEAK_AE_SCRAPE:
		Attack( BEAK_AE_SCRAPE );
		break;

	case BEAK_AE_SCRAPE2:
		Attack( BEAK_AE_SCRAPE2 );
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
int CBeak::ISoundMask( void )
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
// CheckMeleeAttack1 - the bite, unless it is leaping or
// standing on its enemy
//=========================================================
BOOL CBeak::CheckMeleeAttack1( float flDot, float flDist )
{
	if ( m_IdealActivity != ACT_MELEE_ATTACK2 && m_hEnemy->pev->groundentity != ENT(pev) && flDist <= 48 && m_hEnemy != NULL )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckMeleeAttack2 - the leap, every 3 seconds
//=========================================================
BOOL CBeak::CheckMeleeAttack2( float flDot, float flDist )
{
	if ( m_tNextMeleeTime <= gpGlobals->time && flDist <= 240 && m_hEnemy != NULL )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// RunAI - running at a near enemy, the beaks with stamina
// speed up
//=========================================================
void CBeak::RunAI( void )
{
	CSquadMonster::RunAI();

	if ( m_hEnemy != NULL && m_Activity == ACT_RUN )
	{
		float flDist = ( pev->origin - m_hEnemy->Center() ).Length2D();
		float flRange = m_fStam * 400;
		int iRange = flRange + 400;

		if ( flDist < iRange )
			pev->framerate = ( iRange - flDist ) / iRange + 1;
	}
}

// sic: StartTask reads vecEnemyVelocity.y through this inline function; the
// compiler then numbers y in an earlier 32-entity word than z, which gives
// retail's x87 operand orders in the leap's inlined Length() and Normalize()
inline float VecY( const Vector &vec ) { return vec.y; }

void CBeak::StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_DIE:
		RouteClear();
		m_IdealActivity = ACT_DIEVIOLENT;
		pev->deadflag = DEAD_DYING;
		pev->solid = SOLID_NOT;
		break;

	case TASK_BEAK_EAT:
		MakeIdealYaw( m_vecEnemyLKP );
		ChangeYaw( pev->yaw_speed );
		SetActivity( ACT_EAT );
		break;

	case TASK_BEAK_THREAT_DISPLAY:
		if ( m_tNextSoundTime < gpGlobals->time )
		{
			m_IdealActivity = ACT_THREAT_DISPLAY;
			m_tNextSoundTime = gpGlobals->time + 9;
			MiscSound( 1 );
		}
		else
		{
			TaskFail();
		}
		break;

	case TASK_BEAK_GET_TO_LEADER:
		{
			CSquadMonster *pLeader = MySquadLeader();
			if ( pLeader )
				m_hTargetEnt = pLeader;
			else
				TaskFail();
		}
		break;

	case TASK_BEAK_JUMP_MELEE:
		{
			m_tNextMeleeTime = gpGlobals->time + 3;
			m_IdealActivity = ACT_MELEE_ATTACK2;
			ClearBits( pev->flags, FL_ONGROUND );

			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "beak/attack2.wav", 0.7, ATTN_NORM, 0, m_iVoicePitch );

			if ( m_hEnemy != NULL )
			{
				Vector vecDir;
				vecDir = m_hEnemy->BodyTarget( pev->origin ) - pev->origin;
				Vector vecDist = vecDir;
				float flLead = vecDist.Length();
				Vector vecEnemyVelocity = m_hEnemy->pev->velocity;
				float flEnemyY = VecY( vecEnemyVelocity );	// sic: unused, see VecY

				// don't lead a far enemy that isn't running the same way
				if ( DotProduct( vecEnemyVelocity.Normalize(), vecDir.Normalize() ) < 0.4 && flLead > 100 )
					flLead = 0;

				// sic: leads the enemy by its velocity times its distance
				Vector vecLead = m_hEnemy->pev->velocity * flLead;
				Vector vecJump;
				vecJump = vecDir + vecLead;
				// sic: z scaled by 1, a no-op; the named component gives retail's x87
				// operand order in vecJump.Length()
				vecJump.z = vecJump.z * 1;

				float flSpeed = vecJump.Length() * 2.5;
				if ( flSpeed > 500 )
					flSpeed = 500;

				pev->velocity = vecJump.Normalize() * flSpeed;
				float flGravity = CVAR_GET_FLOAT( "sv_gravity" );
				pev->velocity.z += ( flGravity * 0.625 ) * 0.3;
			}
			else
			{
				float flGravity = CVAR_GET_FLOAT( "sv_gravity" );

				pev->velocity = gpGlobals->v_forward * 500;
				pev->velocity.z += ( flGravity * 0.625 ) * 0.4;
			}
		}
		break;

	case TASK_BEAK_LEAP_AWAY:
		{
			pev->ideal_yaw += RANDOM_FLOAT( -20, 20 );

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

	// sic: no break, so the bite starts the base task twice
	case TASK_MELEE_ATTACK1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "beak/attack1.wav", 1, ATTN_NORM, 0, m_iVoicePitch );
		CSquadMonster::StartTask( pTask );

	default:
		CSquadMonster::StartTask( pTask );
		break;
	}
}

void CBeak::RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_BEAK_THREAT_DISPLAY:
		if ( m_fSequenceFinished )
			TaskComplete();
		break;

	case TASK_BEAK_EAT:
		// sic: never completes; it eats until something interrupts it
		if ( m_fSequenceFinished && m_tNextSoundTime < gpGlobals->time )
		{
			m_tNextSoundTime = gpGlobals->time + 9;
			MiscSound( 0 );
		}
		break;

	case TASK_BEAK_JUMP_MELEE:
		if ( FBitSet( pev->flags, FL_ONGROUND ) || pev->velocity.Length() == 0 )
			TaskComplete();
		break;

	case TASK_BEAK_GET_TO_LEADER:
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
			m_vecMoveGoal = m_hTargetEnt->pev->origin;
			if ( !MoveToTarget( ACT_WALK, 2 ) )
				TaskFail();
		}
		break;

	case TASK_BEAK_LEAP_AWAY:
		ChangeYaw( pev->yaw_speed );

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
Schedule_t *CBeak::GetSchedule( void )
{
	switch ( m_MonsterState )
	{
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
					return &slBeakUrgent[ 0 ];
				}
			}

			if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			{
				m_iRelaxed = FALSE;
				if ( UTIL_PercentChance( 10 ) )
				{
					if ( UTIL_PercentChance( 30 ) )
						return GetScheduleOfType( SCHED_SMALL_FLINCH );
					return GetScheduleOfType( SCHED_BIG_FLINCH );
				}
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ORIGIN );
			}

			if ( HasConditions( bits_COND_SMELL_FOOD ) && m_iRelaxed == TRUE )
				return GetScheduleOfType( SCHED_BEAK_EAT );
		}
		break;
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
				{
					if ( UTIL_PercentChance( 30 ) )
						return GetScheduleOfType( SCHED_SMALL_FLINCH );
					return GetScheduleOfType( SCHED_BIG_FLINCH );
				}
			}

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK2 );
		}
		break;

	}

	return CSquadMonster::GetSchedule();
}

//=========================================================
// GetScheduleOfType
//=========================================================
Schedule_t *CBeak::GetScheduleOfType( int Type )
{
	switch ( Type )
	{
	case SCHED_IDLE_STAND:
		if ( IsLeader() && InSquad() )
			return &slBeakLookAround[ 0 ];
		break;

	case SCHED_CHASE_ENEMY_FAILED:
		return &slBeakLookAround[ 0 ];

	case SCHED_BEAK_EAT:
		return &slBeakEat[ 0 ];

	case SCHED_BEAK_PRETEND_EAT:
		return &slBeakPretendEat[ 0 ];

	case SCHED_MELEE_ATTACK1:
		// the squad leader roars now and then
		if ( IsLeader() && InSquad() && UTIL_PercentChance( 5 ) )
			return &slBeakRoar[ 0 ];
		break;

	case SCHED_MELEE_ATTACK2:
		return &slBeakJumpMelee[ 0 ];

	case SCHED_FAIL:
		if ( m_MonsterState == MONSTERSTATE_COMBAT )
		{
			if ( !FNullEnt( FIND_CLIENT_IN_PVS( edict() ) ) )
				return &slBeakCombatFail[ 0 ];
		}
		else
		{
			return &slBeakFail[ 0 ];
		}
		break;
	}

	return CSquadMonster::GetScheduleOfType( Type );
}

void CBeak::Precache( void )
{
	PRECACHE_MODEL( "models/beak.mdl" );

	PRECACHE_SOUND_ARRAY( pAttackSounds );
	PRECACHE_SOUND_ARRAY( pIdleSounds );
	PRECACHE_SOUND_ARRAY( pAlertSounds );
	PRECACHE_SOUND_ARRAY( pPainSounds );
	PRECACHE_SOUND_ARRAY( pDeathSounds );
	PRECACHE_SOUND_ARRAY( pMiscSounds );

	g_iBirdGib = PRECACHE_MODEL( "sprites/gibbeak.spr" );
	g_iGoreBeak = PRECACHE_MODEL( "sprites/gorebeak.spr" );

	CSquadMonster::Precache();
}

void CBeak::Spawn( void )
{
	Precache();

	SET_MODEL( ENT(pev), "models/beak.mdl" );
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 40 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->health			= UTIL_RandomizeHealth( gSkillData.beakHealth );
	m_tNextSoundTime	= gpGlobals->time;
	m_tNextMeleeTime	= gpGlobals->time;
	m_iRelaxed			= TRUE;

	// the strong ones have deeper voices
	m_fStrength			= RANDOM_FLOAT( 0, 1 );
	m_fStam				= 1 - m_fStrength;
	m_iVoicePitch		= 60 - (int)( m_fStrength * -15 );

	MonsterInit();

	pev->view_ofs		= Vector( 0, 0, 40 );
}

//=========================================================
// beakbirther_gib - what is left of the beak queen
//=========================================================
class CBeakBirtherGib : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void );
	void EXPORT FlopAnim( void )
	{
		StudioFrameAdvance();

		if ( m_fSequenceFinished )
			SetSequenceByName( "bodygib#idle" );
		else
			pev->nextthink = gpGlobals->time + 0.1;
	}
};

LINK_ENTITY_TO_CLASS( beakbirther_gib, CBeakBirtherGib );

void CBeakBirtherGib::Precache( void )
{
	PRECACHE_MODEL( "models/beakqueengib.mdl" );
}

int CBeakBirtherGib::Classify( void )
{
	return CLASS_ALIEN_TUBE;
}

void CBeakBirtherGib::Spawn( void )
{
	pev->model = MAKE_STRING( "models/beakqueengib.mdl" );
	pev->classname = MAKE_STRING( "beakbirther_gib" );
	SET_MODEL( ENT(pev), "models/beakqueengib.mdl" );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	pev->effects = 0;
	pev->takedamage = DAMAGE_NO;

	SetThink( &CBeakBirtherGib::FlopAnim );
	SetSequenceByName( "bodygib#start" );
	SetTouch( NULL );
	SetUse( NULL );
	SetBlocked( NULL );

	pev->nextthink = gpGlobals->time;
}

//=========================================================
// monster_beakbirther - the beak queen
//=========================================================
#define BIRTHER_AE_ROAR		( 1 )

class CBeakBirther : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int  MonsterType( void ) { return 16; }
	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin - Vector( 140, 140, 0 );
		pev->absmax = pev->origin + Vector( 140, 140, 300 );
	}
	void DeathSound( void )
	{
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "beak/qbdie.wav", 1, ATTN_NORM, 0, PITCH_NORM );
	}
	void IdleSound( void ) {}
	int  Classify( void );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	BOOL CanBirth( void );
	void GibMonster( void );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
};

LINK_ENTITY_TO_CLASS( monster_beakbirther, CBeakBirther );

void CBeakBirther::Precache( void )
{
	PRECACHE_MODEL( "models/beakqueen.mdl" );
	PRECACHE_MODEL( "models/beakqueengib.mdl" );
	PRECACHE_MODEL( "models/beakbryo.mdl" );

	PRECACHE_SOUND( "beak/qbroar1.wav" );
	PRECACHE_SOUND( "beak/qbroar2.wav" );
	PRECACHE_SOUND( "beak/qbroar3.wav" );

	UTIL_PrecacheOther( "beakbirther_gib" );
	PRECACHE_SOUND( "beak/qbdie.wav" );
	UTIL_PrecacheOther( "monster_beak" );

	g_iGoreBirth = PRECACHE_MODEL( "sprites/gorebirth.spr" );
	g_iGibBirth = PRECACHE_MODEL( "sprites/gibbirth.spr" );
}

//=========================================================
// HandleAnimEvent - the queen roars
//=========================================================
void CBeakBirther::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case BIRTHER_AE_ROAR:
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "beak/qbroar1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 1:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "beak/qbroar2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 2:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "beak/qbroar3.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		}
		// sic: no break; the base class gets the event too

	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// CanBirth - nothing calls it
//=========================================================
BOOL CBeakBirther::CanBirth( void )
{
	return FALSE;
}

//=========================================================
// GibMonster - a spray of gore, beak embryos and what is
// left of the queen
//=========================================================
void CBeakBirther::GibMonster( void )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE_SPRAY );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_COORD( 1 );
		WRITE_SHORT( g_iGibQueen );
		WRITE_BYTE( 24 );	// count
		WRITE_BYTE( 100 );	// speed
		WRITE_BYTE( 200 );	// noise
	MESSAGE_END();

	int cEmbryos = RANDOM_LONG( 4, 8 );
	for ( int i = 0; i < cEmbryos; i++ )
		SpawnGib( "models/beakbryo.mdl", 1.2, 0, 0 );

	CBeakBirtherGib *pGib = GetClassPtr( (CBeakBirtherGib *)NULL );
	pGib->Spawn();
	pGib->pev->origin = pev->origin;
	pGib->pev->angles.x = pev->angles.x;
	pGib->pev->angles.y = pev->angles.y;
	pGib->pev->angles.z = pev->angles.z;
	pGib->Use( NULL, NULL, USE_TOGGLE, 1 );

	SUB_UseTargets( this, USE_TOGGLE, 0 );

	CBaseMonster::GibMonster();
}

//=========================================================
// TakeDamage - every hit can gib the queen
//=========================================================
int CBeakBirther::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_ALWAYSGIB );
}

//=========================================================
// Classify - sic: the bandits' class
//=========================================================
int CBeakBirther::Classify( void )
{
	return CLASS_HUMAN_MILITARY;
}

void CBeakBirther::Spawn( void )
{
	Precache();

	SET_MODEL( ENT(pev), "models/beakqueen.mdl" );
	UTIL_SetSize( pev, Vector( -10, -10, -10 ), Vector( 10, 10, 10 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_FLY;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= 0;
	pev->health			= 150;
	m_flFieldOfView		= -0.7;
	m_MonsterState		= MONSTERSTATE_NONE;

	MonsterInit();
}

//=========================================================
// GetSchedule - the queen only flinches, stands off from
// what it hears or idles
//=========================================================
Schedule_t *CBeakBirther::GetSchedule( void )
{
	if ( !HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
	{
		if ( HasConditions( bits_COND_HEAR_SOUND ) )
			return GetScheduleOfType( SCHED_STANDOFF );

		return GetScheduleOfType( SCHED_IDLE_STAND );
	}

	return GetScheduleOfType( SCHED_ALERT_SMALL_FLINCH );
}

Schedule_t *CBeakBirther::GetScheduleOfType( int Type )
{
	return CBaseMonster::GetScheduleOfType( Type );
}
