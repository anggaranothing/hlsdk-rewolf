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
// the friendly gunman: a talking cavalry trooper with a
// shotgun, Barney reworked; the level designer gives him
// his health, his sentences and his gun
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"talkmonster.h"
#include	"schedule.h"
#include	"defaultai.h"
#include	"scripted.h"
#include	"weapons.h"
#include	"soundent.h"

extern int g_iGibHumanSpr;
extern int g_iGoreHuman;

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		GUNMAN_AE_SHOOT		( 1 )

class CFriendlyGunman : public CTalkMonster
{
public:
	CFriendlyGunman( void );
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	void FireShotgun( void );
	void AlertSound( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );

	void EXPORT FollowerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void DeclineFollowing( void );

	BOOL CheckRangeAttack1 ( float flDot, float flDist );

	int	ObjectCaps( void ) { return CTalkMonster :: ObjectCaps() | FCAP_IMPULSE_USE; }
	int MonsterType( void ) { return 23; }
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	void Killed( entvars_t *pevAttacker, int iGib );

	Schedule_t *GetScheduleOfType ( int Type );
	Schedule_t *GetSchedule ( void );

	void PainSound( void );
	void DeathSound( void );
	void TalkInit( void );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	unsigned short m_usShotgun;
	BOOL	m_bHasAmmo;
	BOOL	m_bLastAttackCheck;
	float	m_painTime;
	float	m_checkAttackTime;
	int		m_iHealthValue;		// "healthvalue"
	int		m_iGunState;		// "gunstate": 2 starts with the gun out

	// the level designer's sentence groups
	string_t	m_iszTLK_ANSWER;
	string_t	m_iszTLK_QUESTION;
	string_t	m_iszTLK_IDLE;
	string_t	m_iszTLK_STARE;
	string_t	m_iszTLK_USE;
	string_t	m_iszTLK_UNUSE;
	string_t	m_iszTLK_STOP;
	string_t	m_iszTLK_NOSHOOT;
	string_t	m_iszTLK_HELLO;
	string_t	m_iszTLK_PLHURT1;
	string_t	m_iszTLK_PLHURT2;
	string_t	m_iszTLK_PLHURT3;
	string_t	m_iszTLK_PHELLO;
	string_t	m_iszTLK_PIDLE;
	string_t	m_iszTLK_PQUESTION;
	string_t	m_iszTLK_SMELL;
	string_t	m_iszTLK_WOUND;
	string_t	m_iszTLK_MORTAL;
	string_t	m_iszTLK_KILL;
	string_t	m_iszTLK_ATTACK;

	CUSTOM_SCHEDULES;
};

LINK_ENTITY_TO_CLASS( monster_human_gunman, CFriendlyGunman );

TYPEDESCRIPTION	CFriendlyGunman::m_SaveData[] =
{
	DEFINE_FIELD( CFriendlyGunman, m_painTime, FIELD_TIME ),
	DEFINE_FIELD( CFriendlyGunman, m_checkAttackTime, FIELD_TIME ),
	DEFINE_FIELD( CFriendlyGunman, m_bLastAttackCheck, FIELD_BOOLEAN ),
	DEFINE_FIELD( CFriendlyGunman, m_bHasAmmo, FIELD_BOOLEAN ),
	DEFINE_FIELD( CFriendlyGunman, m_iGunState, FIELD_INTEGER ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_ANSWER, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_QUESTION, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_IDLE, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_STARE, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_USE, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_UNUSE, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_STOP, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_NOSHOOT, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_HELLO, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_PLHURT1, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_PLHURT2, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_PLHURT3, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_PHELLO, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_PIDLE, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_PQUESTION, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_SMELL, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_WOUND, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_MORTAL, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_KILL, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyGunman, m_iszTLK_ATTACK, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CFriendlyGunman, CTalkMonster );

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlGnFollow[] =
{
	{ TASK_MOVE_TO_TARGET_RANGE,(float)128		},	// Move within 128 of target ent (client)
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_FACE },
};

Schedule_t	slGnFollow[] =
{
	{
		tlGnFollow,
		ARRAYSIZE ( tlGnFollow ),
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED,
		bits_SOUND_DANGER,
		"Follow"
	},
};

Task_t	tlIdleGnStand[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		}, // repick IDLESTAND every two seconds.
	{ TASK_TLK_HEADRESET,		(float)0		}, // reset head position
};

Schedule_t	slIdleGnStand[] =
{
	{
		tlIdleGnStand,
		ARRAYSIZE ( tlIdleGnStand ),
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND	|
		bits_COND_SMELL			|
		bits_COND_PROVOKED,

		bits_SOUND_COMBAT		|// sound flags - change these, and you'll break the talking code.
		//bits_SOUND_PLAYER		|
		//bits_SOUND_WORLD		|

		bits_SOUND_DANGER		|
		bits_SOUND_MEAT			|// scents
		bits_SOUND_CARCASS		|
		bits_SOUND_GARBAGE,
		"IdleStand"
	},
};

DEFINE_CUSTOM_SCHEDULES( CFriendlyGunman )
{
	slGnFollow,
	slIdleGnStand,
};


IMPLEMENT_CUSTOM_SCHEDULES( CFriendlyGunman, CTalkMonster );

//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards.
//=========================================================
int CFriendlyGunman :: ISoundMask ( void)
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
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CFriendlyGunman :: Classify ( void )
{
	return	CLASS_PLAYER_ALLY;
}

//=========================================================
// ALertSound - barney says "Freeze!"
//=========================================================
void CFriendlyGunman :: AlertSound( void )
{
	if ( m_hEnemy != NULL )
	{
		if ( FOkToSpeak() )
		{
			PlaySentence( m_szGrp[TLK_ATTACK], RANDOM_FLOAT(2.8, 3.2), VOL_NORM, ATTN_IDLE );
		}
	}

}
//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CFriendlyGunman :: SetYawSpeed ( void )
{
	int ys;

	ys = 0;

	switch ( m_Activity )
	{
	case ACT_IDLE:
		ys = 70;
		break;
	case ACT_WALK:
		ys = 70;
		break;
	case ACT_RUN:
		ys = 90;
		break;
	default:
		ys = 70;
		break;
	}

	pev->yaw_speed = ys;
}


//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL CFriendlyGunman :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( flDist <= 1024 && flDot >= 0.5 )
	{
		if ( gpGlobals->time > m_checkAttackTime )
		{
			TraceResult tr;

			Vector shootOrigin = pev->origin + Vector( 0, 0, 55 );
			CBaseEntity *pEnemy = m_hEnemy;
			Vector shootTarget = ( (pEnemy->BodyTarget( shootOrigin ) - pEnemy->pev->origin) + m_vecEnemyLKP );
			UTIL_TraceLine( shootOrigin, shootTarget, dont_ignore_monsters, ENT(pev), &tr );
			m_checkAttackTime = gpGlobals->time + 1;
			if ( tr.flFraction == 1.0 || (tr.pHit != NULL && CBaseEntity::Instance(tr.pHit) == pEnemy) )
				m_bLastAttackCheck = TRUE;
			else
				m_bLastAttackCheck = FALSE;
			m_checkAttackTime = gpGlobals->time + 1.5;
		}
		return m_bLastAttackCheck;
	}
	return FALSE;
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CFriendlyGunman :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case GUNMAN_AE_SHOOT:
		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/sbarrel1.wav", 1, ATTN_NORM );
		FireShotgun();
		break;

	default:
		CTalkMonster::HandleAnimEvent( pEvent );
	}
}

//=========================================================
// FireShotgun - the bandit's shotgun
//=========================================================
void CFriendlyGunman :: FireShotgun ( void )
{
	if (m_hEnemy == NULL)
	{
		return;
	}

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	UTIL_MakeVectors ( pev->angles );

	Vector vecAngles = pev->angles;
	vecAngles.x += 4;

	Vector vecForward;
	g_engfuncs.pfnAngleVectors( vecAngles, vecForward, NULL, NULL );

	Vector vecMuzzle;
	GET_ATTACHMENT( ENT(pev), 0, vecMuzzle, NULL );

	PLAYBACK_EVENT_FULL( FEV_RELIABLE, ENT(pev), m_usShotgun, 0.0, (float *)&vecMuzzle, (float *)&vecShootDir, 0.0, 0.0, 0, 0, 0, 0 );	// sic: vecForward isn't used

	FireBullets( gSkillData.plrDmgBuckshot, vecShootOrigin, vecShootDir, VECTOR_CONE_15DEGREES, 4096, BULLET_PLAYER_BUCKSHOT, 0 );

	pev->effects |= EF_MUZZLEFLASH;

	m_cAmmoLoaded--;// take away a bullet!

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, 0 );	// sic: angDir isn't used
}

//=========================================================
// Spawn
//=========================================================
void CFriendlyGunman :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/gunmantrooper.mdl");

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->health			= m_iHealthValue;
	pev->view_ofs		= Vector ( 0, 0, 50 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so npc will notice player and say hello
	m_MonsterState		= MONSTERSTATE_NONE;

	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	MonsterInit();

	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;
	SetUse( &CFriendlyGunman::FollowerUse );

	if ( m_iGunState == 2 )
		SetBodygroup( 1, 1 );
	else
		SetBodygroup( 1, 0 );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CFriendlyGunman :: Precache()
{
	PRECACHE_MODEL("models/gunmantrooper.mdl");

	m_usShotgun = PRECACHE_EVENT( 1, "events/monstershotgun.sc" );

	g_iGoreHuman = PRECACHE_MODEL( "sprites/gorehuman.spr" );
	g_iGibHumanSpr = PRECACHE_MODEL( "sprites/gibhuman.spr" );

	// every new barney must call this, otherwise
	// when a level is loaded, nobody will talk (time is reset to now)
	TalkInit();
	CTalkMonster::Precache();
}

CFriendlyGunman :: CFriendlyGunman( void )
{
	m_iHealthValue = 80;
	m_iGunState = 1;

	m_iszTLK_ANSWER = 0;
	m_iszTLK_QUESTION = 0;
	m_iszTLK_IDLE = 0;
	m_iszTLK_STARE = 0;
	m_iszTLK_USE = 0;
	m_iszTLK_UNUSE = 0;
	m_iszTLK_STOP = 0;
	m_iszTLK_NOSHOOT = 0;
	m_iszTLK_HELLO = 0;
	m_iszTLK_PLHURT1 = 0;
	m_iszTLK_PLHURT2 = 0;
	m_iszTLK_PLHURT3 = 0;
	m_iszTLK_PHELLO = 0;
	m_iszTLK_PIDLE = 0;
	m_iszTLK_PQUESTION = 0;
	m_iszTLK_SMELL = 0;
	m_iszTLK_WOUND = 0;
	m_iszTLK_MORTAL = 0;
	m_iszTLK_ATTACK = 0;
	m_iszTLK_KILL = 0;
}

// Init talk data
void CFriendlyGunman :: TalkInit()
{
	CTalkMonster::TalkInit();

	m_szFriends[0] = "monster_human_gunman";
	m_szFriends[1] = "monster_human_unarmed";

	// the sentence groups come from the level
	if ( m_iszTLK_ANSWER )
		m_szGrp[TLK_ANSWER] = STRING( m_iszTLK_ANSWER );
	else
		m_szGrp[TLK_ANSWER] = NULL;
	if ( m_iszTLK_QUESTION )
		m_szGrp[TLK_QUESTION] = STRING( m_iszTLK_QUESTION );
	else
		m_szGrp[TLK_QUESTION] = NULL;
	if ( m_iszTLK_IDLE )
		m_szGrp[TLK_IDLE] = STRING( m_iszTLK_IDLE );
	else
		m_szGrp[TLK_IDLE] = NULL;
	if ( m_iszTLK_STARE )
		m_szGrp[TLK_STARE] = STRING( m_iszTLK_STARE );
	else
		m_szGrp[TLK_STARE] = NULL;
	if ( m_iszTLK_USE )
		m_szGrp[TLK_USE] = STRING( m_iszTLK_USE );
	else
		m_szGrp[TLK_USE] = NULL;
	if ( m_iszTLK_UNUSE )
		m_szGrp[TLK_UNUSE] = STRING( m_iszTLK_UNUSE );
	else
		m_szGrp[TLK_UNUSE] = NULL;
	if ( m_iszTLK_STOP )
		m_szGrp[TLK_STOP] = STRING( m_iszTLK_STOP );
	else
		m_szGrp[TLK_STOP] = NULL;

	if ( m_iszTLK_NOSHOOT )
		m_szGrp[TLK_NOSHOOT] = STRING( m_iszTLK_NOSHOOT );
	else
		m_szGrp[TLK_NOSHOOT] = NULL;
	if ( m_iszTLK_HELLO )
		m_szGrp[TLK_HELLO] = STRING( m_iszTLK_HELLO );
	else
		m_szGrp[TLK_HELLO] = NULL;

	if ( m_iszTLK_PLHURT1 )
		m_szGrp[TLK_PLHURT1] = STRING( m_iszTLK_PLHURT1 );
	else
		m_szGrp[TLK_PLHURT1] = NULL;
	if ( m_iszTLK_PLHURT2 )
		m_szGrp[TLK_PLHURT2] = STRING( m_iszTLK_PLHURT2 );
	else
		m_szGrp[TLK_PLHURT2] = NULL;
	if ( m_iszTLK_PLHURT3 )
		m_szGrp[TLK_PLHURT3] = STRING( m_iszTLK_PLHURT3 );
	else
		m_szGrp[TLK_PLHURT3] = NULL;

	if ( m_iszTLK_PHELLO )
		m_szGrp[TLK_PHELLO] = STRING( m_iszTLK_PHELLO );
	else
		m_szGrp[TLK_PHELLO] = NULL;
	if ( m_iszTLK_PIDLE )
		m_szGrp[TLK_PIDLE] = STRING( m_iszTLK_PIDLE );
	else
		m_szGrp[TLK_PIDLE] = NULL;
	if ( m_iszTLK_PQUESTION )
		m_szGrp[TLK_PQUESTION] = STRING( m_iszTLK_PQUESTION );
	else
		m_szGrp[TLK_PQUESTION] = NULL;

	if ( m_iszTLK_SMELL )
		m_szGrp[TLK_SMELL] = STRING( m_iszTLK_SMELL );
	else
		m_szGrp[TLK_SMELL] = NULL;

	if ( m_iszTLK_WOUND )
		m_szGrp[TLK_WOUND] = STRING( m_iszTLK_WOUND );
	else
		m_szGrp[TLK_WOUND] = NULL;
	if ( m_iszTLK_MORTAL )
		m_szGrp[TLK_MORTAL] = STRING( m_iszTLK_MORTAL );
	else
		m_szGrp[TLK_MORTAL] = NULL;

	if ( m_iszTLK_ATTACK )
		m_szGrp[TLK_ATTACK] = STRING( m_iszTLK_ATTACK );
	else
		m_szGrp[TLK_ATTACK] = NULL;
	if ( m_iszTLK_KILL )
		m_szGrp[TLK_KILL] = STRING( m_iszTLK_KILL );
	else
		m_szGrp[TLK_KILL] = NULL;

	// get voice for head - just one barney voice for now
	m_voicePitch = 100;
}

void CFriendlyGunman :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "healthvalue" ) )
	{
		m_iHealthValue = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_answer" ) )
	{
		m_iszTLK_ANSWER = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_question" ) )
	{
		m_iszTLK_QUESTION = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_idle" ) )
	{
		m_iszTLK_IDLE = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_stare" ) )
	{
		m_iszTLK_IDLE = ALLOC_STRING( pkvd->szValue );	// sic: the stare key sets the idle group
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_use" ) )
	{
		m_iszTLK_USE = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_unuse" ) )
	{
		m_iszTLK_UNUSE = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_stop" ) )
	{
		m_iszTLK_STOP = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_noshoot" ) )
	{
		m_iszTLK_NOSHOOT = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_hello" ) )
	{
		m_iszTLK_HELLO = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_plhurt1" ) )
	{
		m_iszTLK_PLHURT1 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_plhurt2" ) )
	{
		m_iszTLK_PLHURT2 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_plhurt3" ) )
	{
		m_iszTLK_PLHURT3 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_phello" ) )
	{
		m_iszTLK_PHELLO = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_pidle" ) )
	{
		m_iszTLK_PIDLE = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_pquestion" ) )
	{
		m_iszTLK_PQUESTION = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_smell" ) )
	{
		m_iszTLK_SMELL = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_wound" ) )
	{
		m_iszTLK_WOUND = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_mortal" ) )
	{
		m_iszTLK_MORTAL = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_kill" ) )
	{
		m_iszTLK_KILL = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_attack" ) )
	{
		m_iszTLK_ATTACK = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gunstate" ) )
	{
		m_iGunState = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CTalkMonster::KeyValue( pkvd );
}

int CFriendlyGunman :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// make sure friends talk about it if player hurts talkmonsters...
	int ret = CTalkMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
	if ( !IsAlive() || pev->deadflag == DEAD_DYING )
		return ret;

	if ( m_MonsterState != MONSTERSTATE_PRONE && (pevAttacker->flags & FL_CLIENT) )
	{
		// This is a heurstic to determine if the player intended to harm me
		// If I have an enemy, we can't establish intent (may just be crossfire)
		if ( m_hEnemy == NULL )
		{
			// If I'm already suspicious, get mad
			if ( m_afMemory & bits_MEMORY_SUSPICIOUS )
			{
				// Alright, now I'm pissed!
				PlaySentence( m_szGrp[TLK_NOSHOOT], 4, VOL_NORM, ATTN_NORM );

				Remember( bits_MEMORY_PROVOKED );
				StopFollowing( TRUE );
			}
			else
			{
				// Hey, be careful with that
				PlaySentence( m_szGrp[TLK_NOSHOOT], 4, VOL_NORM, ATTN_NORM );
				Remember( bits_MEMORY_SUSPICIOUS );
			}
		}
		else if ( !(m_hEnemy->IsPlayer()) && pev->deadflag == DEAD_NO )
		{
			PlaySentence( m_szGrp[TLK_NOSHOOT], 4, VOL_NORM, ATTN_NORM );
		}
	}

	return ret;
}


//=========================================================
// PainSound
//=========================================================
void CFriendlyGunman :: PainSound ( void )
{
	if (gpGlobals->time < m_painTime)
		return;

	m_painTime = gpGlobals->time + RANDOM_FLOAT(0.5, 0.75);

	switch (RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_pain1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_pain2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_pain3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}

//=========================================================
// DeathSound
//=========================================================
void CFriendlyGunman :: DeathSound ( void )
{
	switch (RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_die1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_die2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_die3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}


void CFriendlyGunman::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	switch( ptr->iHitgroup)
	{
	case HITGROUP_CHEST:
	case HITGROUP_STOMACH:
		if (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_BLAST))
		{
			flDamage = flDamage / 2;
		}
		break;
	case 10:
		if (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_CLUB))
		{
			flDamage -= 20;
			if (flDamage <= 0)
			{
				UTIL_Ricochet( ptr->vecEndPos, 1.0 );
				flDamage = 0.01;
			}
		}
		// always a head shot
		ptr->iHitgroup = HITGROUP_HEAD;
		break;
	}

	CTalkMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}


void CFriendlyGunman::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( m_bHasAmmo )
	{// drop the gun!
		Vector vecGunAngles;

		m_bHasAmmo = FALSE;
		SetBodygroup( 1, 1 );

		CBaseEntity *pGun = DropItem( "weapon_shotgun", GetGunPosition(), vecGunAngles );	// sic: the angles are never set
		if ( pGun )
		{
			pGun->pev->velocity = Vector( RANDOM_FLOAT( -48, 48 ), RANDOM_FLOAT( -48, 48 ), RANDOM_FLOAT( 100, 200 ) );
			pGun->pev->avelocity = Vector( 0, RANDOM_FLOAT( 100, 200 ), 0 );
		}
	}

	SetUse( NULL );
	CTalkMonster::Killed( pevAttacker, iGib );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

Schedule_t* CFriendlyGunman :: GetScheduleOfType ( int Type )
{
	Schedule_t *psched;

	switch( Type )
	{
	case SCHED_IDLE_STAND:
		// call base class default so that the gunman will talk
		// when standing during idle
		psched = CTalkMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
		{
			// just look straight ahead.
			return slIdleGnStand;
		}
		else
			return psched;

	// Hook these to make a looping schedule
	case SCHED_TARGET_FACE:
		// call base class default so that the gunman will talk
		// when 'used'
		psched = CTalkMonster::GetScheduleOfType(Type);
		return psched;

	case SCHED_TARGET_CHASE:
		return slGnFollow;
	}

	return CTalkMonster::GetScheduleOfType( Type );
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CFriendlyGunman :: GetSchedule ( void )
{
	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *pSound;
		pSound = PBestSound();

		ASSERT( pSound != NULL );
		if ( pSound && (pSound->m_iType & bits_SOUND_DANGER) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
	}
	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
	{
		PlaySentence( m_szGrp[TLK_KILL], 4, VOL_NORM, ATTN_NORM );
	}

	switch( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		{
// dead enemy
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			{
				// call base class, all code to handle dead enemies is centralized there.
				return CBaseMonster :: GetSchedule();
			}

			// always act surprized with a new enemy
			if ( HasConditions( bits_COND_NEW_ENEMY ) && HasConditions( bits_COND_LIGHT_DAMAGE) )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );

			if ( HasConditions( bits_COND_HEAVY_DAMAGE ) )
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
		}
		break;

	case MONSTERSTATE_ALERT:
	case MONSTERSTATE_IDLE:
		if ( HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
		{
			// flinch if hurt
			return GetScheduleOfType( SCHED_SMALL_FLINCH );
		}

		if ( m_hEnemy == NULL && IsFollowing() )
		{
			if ( !m_hTargetEnt->IsAlive() )
			{
				// UNDONE: Comment about the recently dead player here?
				StopFollowing( FALSE );
				break;
			}
			else
			{
				if ( HasConditions( bits_COND_CLIENT_PUSH ) )
				{
					return GetScheduleOfType( SCHED_MOVE_AWAY_FOLLOW );
				}
				return GetScheduleOfType( SCHED_TARGET_FACE );
			}
		}

		if ( HasConditions( bits_COND_CLIENT_PUSH ) )
		{
			return GetScheduleOfType( SCHED_MOVE_AWAY );
		}

		// try to say something about smells
		TrySmellTalk();
		break;
	}

	return CTalkMonster::GetSchedule();
}

//=========================================================
// FollowerUse - the gunmen never follow, they only answer
//=========================================================
void CFriendlyGunman::FollowerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Don't allow use during a scripted_sentence
	if ( m_useTime > gpGlobals->time )
		return;

	DeclineFollowing();
}

void CFriendlyGunman::DeclineFollowing( void )
{
	PlaySentence( m_szGrp[TLK_USE], 2, VOL_NORM, ATTN_NORM );
}
