/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// human scientist (passive lab worker)
//
// Gunman's scientist cowers until something calms him down:
// damage, a scary sound he can see the source of, combat or a
// level trigger. He then fires his trigger target and, armed
// with a knife, fights back. The level designer gives him his
// sentence groups; +use from anything but the player toggles
// his AI. He never follows.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"talkmonster.h"
#include	"schedule.h"
#include	"defaultai.h"
#include	"scripted.h"
#include	"animation.h"
#include	"soundent.h"


extern int g_iGibHumanSpr;
extern int g_iGoreHuman;

BOOL GetScriptedIdleName( const char *pszSequence, char *pszDest );

#define		NUM_SCIENTIST_HEADS		3 // three heads available for scientist model, and a mask
enum { HEAD_YOUNG = 0, HEAD_BALD = 1, HEAD_BEARD = 2, HEAD_MASK = 3 };

#define		SCIENTIST_KNIFE			32	// pev->weapons when he carries a knife

enum
{
	SCHED_HIDE = LAST_TALKMONSTER_SCHEDULE + 1,
	SCHED_PANIC,
	SCHED_STARTLE,
	SCHED_TARGET_FACE_SCARED,
};

enum
{
	TASK_RUN_PATH_SCARED = LAST_TALKMONSTER_TASK + 1,
	TASK_SCREAM,
	TASK_RANDOM_SCREAM,
	TASK_MOVE_TO_TARGET_RANGE_SCARED,
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		SCIENTIST_AE_STAB1		( 4 )
#define		SCIENTIST_AE_STAB2		( 5 )

// the scripts he plays break on damage and on sounds
#define		SCIENTIST_SCRIPT_BREAK_CONDITIONS	( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND )

//=======================================================
// Scientist
//=======================================================

class CScientist : public CTalkMonster
{
public:
	CScientist( void )
	{
		m_iIsScared = 1;
		m_iAIActive = 1;
		m_iShouldClearVoiceChannel = 0;
		m_iBlockSound = 0;

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

	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );

	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void RunTask( Task_t *pTask );
	void StartTask( Task_t *pTask );
	int	ObjectCaps( void ) { return CTalkMonster :: ObjectCaps() | FCAP_IMPULSE_USE; }
	int MonsterType( void ) { return 20; }
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	virtual int FriendNumber( int arrayNumber );
	void SetActivity ( Activity newActivity );
	int ISoundMask( void );
	void DeclineFollowing( void );

	float	CoverRadius( void ) { return 1200; }		// Need more room for cover because scientists want to get far away!
	float	HearingSensitivity( void ) { return 1.0; }

	BOOL	FCanCheckAttacks( void );
	BOOL	CheckMeleeAttack1( float flDot, float flDist );
	Vector	GetGunPosition( void );
	int		IgnoreConditions( void );

	void	Scream( void );

	// no longer scared: shut up if told to and tell the level
	void	CalmDown( void )
	{
		if ( m_iShouldClearVoiceChannel == 1 )
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "common/null.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		m_iIsScared = 0;
		FireTargets( STRING( m_iszTriggerTarget ), this, this, USE_OFF, 0 );
	}

	// Override these to set behavior
	Schedule_t *GetScheduleOfType ( int Type );
	Schedule_t *GetSchedule ( void );
	MONSTERSTATE GetIdealState ( void );

	void DeathSound( void );
	void PainSound( void );

	void TalkInit( void );

	void EXPORT ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

	float	m_fearTime;
	int		m_iIsScared;					// cowers until something calms him down
	int		m_iAIActive;					// "DefautAIState" 1 starts with the AI off
	int		m_iShouldClearVoiceChannel;		// "clearvoice"
	int		m_iBlockSound;					// "blocksound": ignore sounds
	int		m_iHead;						// the body keyvalue, kept for the head bodygroup

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
};

LINK_ENTITY_TO_CLASS( monster_scientist, CScientist );

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlFaceTargetScared[] =
{
	{ TASK_FACE_TARGET,			(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_CROUCHIDLE },
};

Schedule_t	slFaceTargetScared[] =
{
	{
		tlFaceTargetScared,
		ARRAYSIZE ( tlFaceTargetScared ),
		bits_COND_HEAR_SOUND |
		bits_COND_NEW_ENEMY,
		bits_SOUND_DANGER,
		"FaceTargetScared"
	},
};


Task_t	tlFaceTarget[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_TARGET,			(float)0		},
	{ TASK_SET_ACTIVITY_IDLE,	(float)0		},
};

Schedule_t	slFaceTarget[] =
{
	{
		tlFaceTarget,
		ARRAYSIZE ( tlFaceTarget ),
		bits_COND_CLIENT_PUSH |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_COMBAT |
		bits_SOUND_DANGER,
		"FaceTarget"
	},
};


Task_t	tlSciPanic[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_SCREAM,				(float)0		},
	{ TASK_SET_ACTIVITY_IDLE,	(float)0		},
};

Schedule_t	slSciPanic[] =
{
	{
		tlSciPanic,
		ARRAYSIZE ( tlSciPanic ),
		0,
		0,
		"SciPanic"
	},
};


Task_t	tlIdleSciStand[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY_IDLE,	(float)0		},
	{ TASK_WAIT,				(float)2		}, // repick IDLESTAND every two seconds.
	{ TASK_TLK_HEADRESET,		(float)0		}, // reset head position
};

Schedule_t	slIdleSciStand[] =
{
	{
		tlIdleSciStand,
		ARRAYSIZE ( tlIdleSciStand ),
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND	|
		bits_COND_SMELL			|
		bits_COND_CLIENT_PUSH	|
		bits_COND_PROVOKED,

		bits_SOUND_COMBAT		|// sound flags
		//bits_SOUND_PLAYER		|
		//bits_SOUND_WORLD		|
		bits_SOUND_DANGER		|
		bits_SOUND_MEAT			|// scents
		bits_SOUND_CARCASS		|
		bits_SOUND_GARBAGE,
		"IdleSciStand"

	},
};


Task_t	tlScientistCover[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_HIDE },		// If you fail, hide
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0					},
	{ TASK_RUN_PATH_SCARED,			(float)0					},
	{ TASK_TURN_LEFT,				(float)179					},
};

Schedule_t	slScientistCover[] =
{
	{
		tlScientistCover,
		ARRAYSIZE ( tlScientistCover ),
		bits_COND_NEW_ENEMY,
		0,
		"ScientistCover"
	},
};



Task_t	tlScientistHide[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_PANIC },		// If you fail, just panic!
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SET_ACTIVITY,			(float)ACT_CROUCH			},
	{ TASK_SET_ACTIVITY,			(float)ACT_CROUCHIDLE		},
	{ TASK_WAIT_RANDOM,				(float)20.0					},
};

Schedule_t	slScientistHide[] =
{
	{
		tlScientistHide,
		ARRAYSIZE ( tlScientistHide ),
		0,
		0,
		"ScientistHide"
	},
};


Task_t	tlScientistStartle[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_PANIC },		// If you fail, just panic!
	{ TASK_RANDOM_SCREAM,			(float)0.3 },				// Scream 30% of the time
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SET_ACTIVITY,			(float)ACT_CROUCH			},
	{ TASK_SET_ACTIVITY,			(float)ACT_CROUCHIDLE		},
	{ TASK_WAIT_RANDOM,				(float)10.0					},
};

Schedule_t	slScientistStartle[] =
{
	{
		tlScientistStartle,
		ARRAYSIZE ( tlScientistStartle ),
		0,
		0,
		"ScientistStartle"
	},
};


//=========================================================
// scripted sequences that sounds can break
//=========================================================
Task_t	tlScientistWalkToScript[] =
{
	{ TASK_WALK_TO_TARGET,		(float)TARGET_MOVE_SCRIPTED },
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0		},
	{ TASK_PLANT_ON_SCRIPT,		(float)0		},
	{ TASK_FACE_SCRIPT,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_ENABLE_SCRIPT,		(float)0		},
	{ TASK_WAIT_FOR_SCRIPT,		(float)0		},
	{ TASK_PLAY_SCRIPT,			(float)0		},
};

Schedule_t	slScientistWalkToScript[] =
{
	{
		tlScientistWalkToScript,
		ARRAYSIZE ( tlScientistWalkToScript ),
		SCIENTIST_SCRIPT_BREAK_CONDITIONS,
		bits_SOUND_COMBAT |
		bits_SOUND_DANGER,
		"ScientistWalkToScript"
	},
};


Task_t	tlScientistRunToScript[] =
{
	{ TASK_RUN_TO_TARGET,		(float)TARGET_MOVE_SCRIPTED },
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0		},
	{ TASK_PLANT_ON_SCRIPT,		(float)0		},
	{ TASK_FACE_SCRIPT,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_ENABLE_SCRIPT,		(float)0		},
	{ TASK_WAIT_FOR_SCRIPT,		(float)0		},
	{ TASK_PLAY_SCRIPT,			(float)0		},
};

Schedule_t	slScientistRunToScript[] =
{
	{
		tlScientistRunToScript,
		ARRAYSIZE ( tlScientistRunToScript ),
		SCIENTIST_SCRIPT_BREAK_CONDITIONS,
		bits_SOUND_COMBAT |
		bits_SOUND_DANGER,
		"ScientistRunToScript"
	},
};


Task_t	tlScientistWaitForScript[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_WAIT_FOR_SCRIPT,		(float)0		},
	{ TASK_PLAY_SCRIPT,			(float)0		},
};

Schedule_t	slScientistWaitForScript[] =
{
	{
		tlScientistWaitForScript,
		ARRAYSIZE ( tlScientistWaitForScript ),
		SCIENTIST_SCRIPT_BREAK_CONDITIONS,
		bits_SOUND_COMBAT |
		bits_SOUND_DANGER,
		"ScientistWaitForScript"
	},
};


Task_t	tlScientistFaceScript[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_SCRIPT,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_WAIT_FOR_SCRIPT,		(float)0		},
	{ TASK_PLAY_SCRIPT,			(float)0		},
};

Schedule_t	slScientistFaceScript[] =
{
	{
		tlScientistFaceScript,
		ARRAYSIZE ( tlScientistFaceScript ),
		SCIENTIST_SCRIPT_BREAK_CONDITIONS,
		bits_SOUND_COMBAT |
		bits_SOUND_DANGER,
		"ScientistFaceScript"
	},
};


TYPEDESCRIPTION	CScientist::m_SaveData[] =
{
	DEFINE_FIELD( CScientist, m_fearTime, FIELD_TIME ),
	DEFINE_FIELD( CScientist, m_iIsScared, FIELD_INTEGER ),
	DEFINE_FIELD( CScientist, m_iAIActive, FIELD_INTEGER ),
	DEFINE_FIELD( CScientist, m_iShouldClearVoiceChannel, FIELD_INTEGER ),
	DEFINE_FIELD( CScientist, m_iBlockSound, FIELD_INTEGER ),
	DEFINE_FIELD( CScientist, m_iszTLK_ANSWER, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_QUESTION, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_IDLE, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_STARE, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_USE, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_UNUSE, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_STOP, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_NOSHOOT, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_HELLO, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_PLHURT1, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_PLHURT2, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_PLHURT3, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_PHELLO, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_PIDLE, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_PQUESTION, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_SMELL, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_WOUND, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_MORTAL, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_KILL, FIELD_STRING ),
	DEFINE_FIELD( CScientist, m_iszTLK_ATTACK, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CScientist, CTalkMonster );


DEFINE_CUSTOM_SCHEDULES( CScientist )
{
	slFaceTarget,
	slIdleSciStand,
	slScientistCover,
	slScientistHide,
	slScientistStartle,
	slSciPanic,
	slFaceTargetScared,
	slScientistWalkToScript,
	slScientistRunToScript,
	slScientistWaitForScript,
	slScientistFaceScript,
};


IMPLEMENT_CUSTOM_SCHEDULES( CScientist, CTalkMonster );


//=========================================================
// Spawn
//=========================================================
void CScientist :: Spawn( void )
{
	Precache( );

	SET_MODEL(ENT(pev), "models/scientist.mdl");

	pev->solid			= SOLID_SLIDEBOX;
	pev->health			= gSkillData.scientistHealth;
	m_MonsterState		= MONSTERSTATE_NONE;
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	m_hTalkTarget = NULL;

	m_iHead = pev->body;
	switch ( m_iHead )
	{
	case -1:// -1 chooses a random head
		SetBodygroup( 1, RANDOM_LONG( 0, NUM_SCIENTIST_HEADS - 1 ) );
		break;
	case HEAD_YOUNG:
		SetBodygroup( 1, 0 );
		break;
	case HEAD_BALD:
		SetBodygroup( 1, 1 );
		break;
	case HEAD_BEARD:
		SetBodygroup( 1, 2 );
		break;
	case HEAD_MASK:
		SetBodygroup( 1, 3 );
		break;
	}

	// no knives in the low violence version
	if ( g_Language == LANGUAGE_GERMAN || CVAR_GET_FLOAT( "violence_germany" ) != 0 )
		pev->weapons = 0;

	if ( pev->weapons == SCIENTIST_KNIFE )
	{
		// a scientist with a knife isn't scared
		SetBodygroup( 2, 1 );
		CalmDown();
	}

	MonsterInit();

	m_afCapability |= bits_CAP_TURN_HEAD;
	SetUse( &CScientist::ToggleUse );
	pev->view_ofs = Vector ( 0, 0, 50 );// position of the eyes relative to monster's origin.
}


void CScientist::DeclineFollowing( void )
{
	PlaySentence( m_szGrp[TLK_USE], 2, VOL_NORM, ATTN_NORM );
}


//=========================================================
// ToggleUse - the player gets a sentence, anything else
// turns the AI on or off
//=========================================================
void CScientist::ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pCaller && pCaller->IsPlayer() )
	{
		DeclineFollowing();
		return;
	}

	if ( !m_iAIActive )
	{
		ALERT( at_aiconsole, "NOT Player called scientist use, toggling AI to ON\n" );
		m_iAIActive = 1;
	}
	else
	{
		ALERT( at_aiconsole, "NOT Player called scientist use, toggling AI to OFF\n" );
		m_iAIActive = 0;
	}
}


//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CScientist :: Precache( void )
{
	g_iGoreHuman = PRECACHE_MODEL( "sprites/gorehuman.spr" );
	g_iGibHumanSpr = PRECACHE_MODEL( "sprites/gibhuman.spr" );

	PRECACHE_MODEL("models/scientist.mdl");

	PRECACHE_SOUND("gscientist/scientist_diebackwards.wav");
	PRECACHE_SOUND("gscientist/scientist_dieforwards.wav");
	PRECACHE_SOUND("gscientist/scientist_dieforwards2.wav");
	PRECACHE_SOUND("gscientist/scientist_diesimple.wav");
	PRECACHE_SOUND("gscientist/scientist_dieviolent.wav");
	PRECACHE_SOUND("gscientist/scientist_dieheadshot.wav");
	PRECACHE_SOUND("gscientist/hurt1.wav");
	PRECACHE_SOUND("gscientist/hurt5.wav");
	PRECACHE_SOUND("gscientist/hurt6.wav");

	// every new scientist must call this, otherwise
	// when a level is loaded, nobody will talk (time is reset to 0)
	TalkInit();

	CTalkMonster::Precache();
}


void CScientist :: KeyValue( KeyValueData *pkvd )
{
	// sic: no else after these four; they go on to CTalkMonster::KeyValue too
	if ( FStrEq( pkvd->szKeyName, "knife" ) )
	{
		if ( atoi( pkvd->szValue ) )
			pev->weapons = SCIENTIST_KNIFE;
		pkvd->fHandled = TRUE;
	}

	if ( FStrEq( pkvd->szKeyName, "IsScared" ) )
	{
		if ( atoi( pkvd->szValue ) )
			CalmDown();		// sic: a non-zero "IsScared" calms him down
		pkvd->fHandled = TRUE;
	}

	if ( FStrEq( pkvd->szKeyName, "clearvoice" ) )
	{
		if ( atoi( pkvd->szValue ) )
			m_iShouldClearVoiceChannel = 1;
		pkvd->fHandled = TRUE;
	}

	if ( FStrEq( pkvd->szKeyName, "blocksound" ) )
	{
		if ( atoi( pkvd->szValue ) )
			m_iBlockSound = 1;
		pkvd->fHandled = TRUE;
	}

	if ( FStrEq( pkvd->szKeyName, "DefautAIState" ) )
	{
		if ( atoi( pkvd->szValue ) == 1 )
			m_iAIActive = 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_answer" ) )
	{
		m_iszTLK_ANSWER = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_question" ) )
	{
		m_iszTLK_QUESTION = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_idle" ) )
	{
		m_iszTLK_IDLE = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_stare" ) )
	{
		m_iszTLK_IDLE = ALLOC_STRING( pkvd->szValue );	// sic: the stare key sets the idle group
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_use" ) )
	{
		m_iszTLK_USE = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_unuse" ) )
	{
		m_iszTLK_UNUSE = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_stop" ) )
	{
		m_iszTLK_STOP = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_noshoot" ) )
	{
		m_iszTLK_NOSHOOT = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_hello" ) )
	{
		m_iszTLK_HELLO = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_plhurt1" ) )
	{
		m_iszTLK_PLHURT1 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_plhurt2" ) )
	{
		m_iszTLK_PLHURT2 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_plhurt3" ) )
	{
		m_iszTLK_PLHURT3 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_phello" ) )
	{
		m_iszTLK_PHELLO = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_pidle" ) )
	{
		m_iszTLK_PIDLE = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_pquestion" ) )
	{
		m_iszTLK_PQUESTION = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_smell" ) )
	{
		m_iszTLK_SMELL = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_wound" ) )
	{
		m_iszTLK_WOUND = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_mortal" ) )
	{
		m_iszTLK_MORTAL = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_kill" ) )
	{
		m_iszTLK_KILL = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sc_attack" ) )
	{
		m_iszTLK_ATTACK = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CTalkMonster::KeyValue( pkvd );
}


// Init talk data
void CScientist :: TalkInit()
{

	CTalkMonster::TalkInit();

	// sic: the friend list is shared by every talking monster; this
	// replaces the sitting scientist in it, and sets no other friend
	m_szFriends[2] = "monster_scientist";

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

	// get voice for head
	switch (pev->body % 3)
	{
	case HEAD_YOUNG:	m_voicePitch = 105; break;
	case HEAD_BALD:		m_voicePitch = 100; break;
	case HEAD_BEARD:	m_voicePitch = 95;  break;
	case HEAD_MASK:		m_voicePitch = 100; break;
	default:			m_voicePitch = 90;  break;
	}
}


void CScientist :: Scream( void )
{
	if ( FOkToSpeak() )
	{
		Talk( 10 );
		m_hTalkTarget = m_hEnemy;
		PlaySentence( "SC_SCREAM", RANDOM_FLOAT(3, 6), VOL_NORM, ATTN_NORM );
	}
}


BOOL CScientist :: FCanCheckAttacks( void )
{
	if ( HasConditions( bits_COND_ENEMY_TOOFAR ) )
		return FALSE;

	return TRUE;
}


//=========================================================
// CheckMeleeAttack1 - only with the knife
//=========================================================
BOOL CScientist :: CheckMeleeAttack1( float flDot, float flDist )
{
	CBaseMonster *pEnemy;

	if ( pev->weapons == SCIENTIST_KNIFE )
	{
		if ( m_hEnemy != NULL )
		{
			pEnemy = m_hEnemy->MyMonsterPointer();

			if ( !pEnemy )
			{
				return FALSE;
			}
		}

		if ( flDist <= 64 && flDot >= 0.7	&&
			 pEnemy->Classify() != CLASS_ALIEN_BIOWEAPON &&
			 pEnemy->Classify() != CLASS_PLAYER_BIOWEAPON )
		{
			return TRUE;
		}
	}
	return FALSE;
}


Vector CScientist :: GetGunPosition( void )
{
	Vector vecGunPos;
	Vector vecGunAngles;

	GetAttachment( 0, vecGunPos, vecGunAngles );
	return vecGunPos;
}


void CScientist :: StartTask( Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_SCREAM:
		Scream();
		TaskComplete();
		break;

	case TASK_RANDOM_SCREAM:
		if ( RANDOM_FLOAT( 0, 1 ) < pTask->flData )
			Scream();
		TaskComplete();
		break;

	case TASK_RUN_PATH_SCARED:
		m_IdealActivity = ACT_RUN;
		break;

	case TASK_MOVE_TO_TARGET_RANGE_SCARED:
		{
			if ( (m_hTargetEnt->pev->origin - pev->origin).Length() < 1 )
				TaskComplete();
			else
			{
				m_vecMoveGoal = m_hTargetEnt->pev->origin;
				if ( !MoveToTarget( ACT_WALK, 0.5 ) )
					TaskFail();
			}
		}
		break;

	default:
		CTalkMonster::StartTask( pTask );
		break;
	}
}

void CScientist :: RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RUN_PATH_SCARED:
		if ( MovementIsComplete() )
			TaskComplete();
		if ( UTIL_PercentChance( 10 ) )
			Scream();
		break;

	case TASK_MOVE_TO_TARGET_RANGE_SCARED:
		{
			if ( RANDOM_LONG(0,63)< 8 )
				Scream();

			if ( m_hEnemy == NULL )
			{
				TaskFail();
			}
			else
			{
				float distance;

				distance = ( m_vecMoveGoal - pev->origin ).Length2D();
				// Re-evaluate when you think your finished, or the target has moved too far
				if ( (distance < pTask->flData) || (m_vecMoveGoal - m_hTargetEnt->pev->origin).Length() > pTask->flData * 0.5 )
				{
					m_vecMoveGoal = m_hTargetEnt->pev->origin;
					distance = ( m_vecMoveGoal - pev->origin ).Length2D();
					FRefreshRoute();
				}

				// Set the appropriate activity based on an overlapping range
				// overlap the range to prevent oscillation
				if ( distance < pTask->flData )
				{
					TaskComplete();
					RouteClear();		// Stop moving
				}
				else if ( distance < 190 && m_movementActivity != ACT_WALK )
					m_movementActivity = ACT_WALK;
				else if ( distance >= 270 && m_movementActivity != ACT_RUN )
					m_movementActivity = ACT_RUN;
			}
		}
		break;

	default:
		CTalkMonster::RunTask( pTask );
		break;
	}
}

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CScientist :: Classify ( void )
{
	// once he isn't scared any more, he is an enemy like the bandits
	if ( g_Language != LANGUAGE_GERMAN && CVAR_GET_FLOAT( "violence_germany" ) == 0 )
	{
		if ( m_iIsScared )
			return CLASS_HUMAN_PASSIVE;
		return CLASS_HUMAN_MILITARY;
	}

	return	CLASS_HUMAN_PASSIVE;
}


//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CScientist :: SetYawSpeed ( void )
{
	int ys;

	ys = 90;

	switch ( m_Activity )
	{
	case ACT_IDLE:
		ys = 120;
		break;
	case ACT_WALK:
		ys = 180;
		break;
	case ACT_RUN:
		ys = 150;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 140;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CScientist :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case SCIENTIST_AE_STAB1:
	case SCIENTIST_AE_STAB2:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 64, 15, DMG_SLASH );

			if ( pHurt )
			{
				pHurt->pev->punchangle.z = rand() % 8 - 4;
				pHurt->pev->punchangle.x = rand() % 8 - 4;

				CBaseMonster *pMonster = pHurt->MyMonsterPointer();
				if ( pMonster )
				{
					TraceResult tr;
					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
					pMonster->TraceBleed( gSkillData.xenomeDmgMelee, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize(), &tr, DMG_SLASH, tr.vecEndPos );	// sic: the xenome's melee damage

					if ( pMonster->pev->flags & ( FL_CLIENT | FL_MONSTER ) )
						AttackHitSound( pMonster );
				}
			}
		}
		break;

	default:
		CTalkMonster::HandleAnimEvent( pEvent );
	}
}


//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. In the base class implementation,
// monsters care about all sounds, but no scents.
//=========================================================
int CScientist :: ISoundMask ( void )
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_DANGER	|
			bits_SOUND_PLAYER;
}

//=========================================================
// PainSound
//=========================================================
void CScientist :: PainSound ( void )
{
	if ( !IsAlive() )
		return;

	// sic: abs() takes an int, so this waits a whole second
	if ( fabs( gpGlobals->time - m_tLastTimeHurt ) < 0.7 )
		return;

	if ( m_MonsterState == MONSTERSTATE_SCRIPT || m_MonsterState == MONSTERSTATE_PRONE || m_MonsterState == MONSTERSTATE_PLAYDEAD || m_MonsterState == MONSTERSTATE_NONE )
		return;

	m_tLastTimeHurt = gpGlobals->time;

	switch (RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gscientist/hurt1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gscientist/hurt5.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gscientist/hurt6.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}

//=========================================================
// DeathSound
//=========================================================
void CScientist :: DeathSound ( void )
{
}


void CScientist :: SetActivity ( Activity NewActivity )
{
	int	iSequence;

	// a scripted scientist with no activity yet idles in the
	// "<script prefix>#idle" sequence when his model has one
	if ( m_MonsterState == MONSTERSTATE_SCRIPT && m_Activity == ACT_RESET )
	{
		char szIdle[64];

		if ( GetScriptedIdleName( GetSequenceName( pev->sequence ), szIdle ) )
		{
			strcat( szIdle, "idle" );
			iSequence = LookupSequence( szIdle );
			if ( iSequence != -1 )
			{
				m_IdealActivity = m_Activity = (Activity)GetSequenceActivity( iSequence );
				pev->sequence = iSequence;
				ResetSequenceInfo( );
				SetYawSpeed();
				return;
			}
		}
	}

	switch ( NewActivity )
	{
	case ACT_RUN:
		if ( pev->health <= 10 && pev->weapons != SCIENTIST_KNIFE )
		{
			// limp!
			iSequence = LookupActivity ( ACT_WALK_HURT );
		}
		else
		{
			iSequence = LookupSequence( "run" );
		}
		break;
	case ACT_WALK:
		if ( pev->health <= 10 )
		{
			// limp!
			if ( pev->weapons != SCIENTIST_KNIFE )
				iSequence = LookupActivity ( ACT_WALK_HURT );
			else
				iSequence = LookupActivity ( NewActivity );
		}
		iSequence = LookupSequence( "walk" );	// sic: always walks normally
		break;
	case ACT_IDLE:
	case ACT_IDLE_ANGRY:
	case ACT_COMBAT_IDLE:
		if ( pev->weapons == SCIENTIST_KNIFE )
			iSequence = LookupSequence( "idlenormal" );
		else
			iSequence = LookupActivity ( NewActivity );
		break;
	case ACT_SIGNAL1:
	case ACT_SIGNAL2:
	case ACT_SIGNAL3:
		iSequence = LookupActivity ( NewActivity );
		if ( iSequence == ACTIVITY_NOT_AVAILABLE )
			iSequence = LookupActivity ( ACT_IDLE );
		break;
	default:
		iSequence = LookupActivity ( NewActivity );
		CTalkMonster::SetActivity( NewActivity );
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


Schedule_t *CScientist :: GetSchedule ( void )
{
	if ( !m_iAIActive )
		return GetScheduleOfType( SCHED_IDLE_STAND );

	switch( m_MonsterState )
	{
	case MONSTERSTATE_ALERT:
	case MONSTERSTATE_IDLE:
		if ( HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
		{
			// flinch if hurt
			CalmDown();
			return GetScheduleOfType( SCHED_SMALL_FLINCH );
		}

		// Cower when you hear something scary
		if ( HasConditions( bits_COND_HEAR_SOUND ) )
		{
			CSound *pSound;
			pSound = PBestSound();

			if ( pSound )
			{
				if ( pev->weapons )
					return GetScheduleOfType( SCHED_ALERT_FACE );

				if ( pSound->m_iType & (bits_SOUND_DANGER | bits_SOUND_COMBAT) )
				{
					TraceResult tr;

					// only when he can see where it came from
					UTIL_TraceLine( pev->origin, pSound->m_vecOrigin, ignore_monsters, ENT(pev), &tr );
					if ( tr.flFraction == 1.0 )
					{
						CalmDown();
						if ( gpGlobals->time - m_fearTime > 3 )	// Only cower every 3 seconds or so
						{
							m_fearTime = gpGlobals->time;		// Update last fear
							return GetScheduleOfType( SCHED_STARTLE );	// This will just duck for a second
						}
					}
				}
			}
		}

		if ( HasConditions( bits_COND_CLIENT_PUSH ) )	// Player wants me to move
			return GetScheduleOfType( SCHED_MOVE_AWAY );

		// try to say something about smells
		TrySmellTalk();
		break;
	case MONSTERSTATE_COMBAT:
		CalmDown();

		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
		if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

		if ( pev->weapons != SCIENTIST_KNIFE )
		{
			if ( HasConditions( bits_COND_SEE_ENEMY ) )
				return slScientistCover;		// Take Cover

			if ( HasConditions( bits_COND_HEAR_SOUND ) )
				return slTakeCoverFromBestSound;	// Cower and panic from the scary sound!
		}
		break;
	}

	return CTalkMonster::GetSchedule();
}


Schedule_t* CScientist :: GetScheduleOfType ( int Type )
{
	Schedule_t *psched;

	switch( Type )
	{
	case SCHED_AISCRIPT:
		{
			if ( !m_pCine )
			{
				ALERT( at_aiconsole, "Script failed for %s\n", STRING(pev->classname) );
				CineCleanup();
				return GetScheduleOfType( SCHED_IDLE_STAND );
			}

			switch ( m_pCine->m_fMoveTo )
			{
				case 0:
				case 4:
					return slScientistWaitForScript;
				case 1:
					return slScientistWalkToScript;
				case 2:
					return slScientistRunToScript;
				case 5:
					return slScientistFaceScript;
			}
			break;
		}

	// Hook these to make a looping schedule
	case SCHED_TARGET_FACE:
		// call base class default so that scientist will talk
		// when 'used'
		psched = CTalkMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
			return slFaceTarget;	// override this for different target face behavior
		else
			return psched;

	case SCHED_PANIC:
		return slScientistCover;

	case SCHED_TARGET_FACE_SCARED:
		return slFaceTargetScared;

	case SCHED_IDLE_STAND:
		// call base class default so that scientist will talk
		// when standing during idle
		psched = CTalkMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
			return slIdleSciStand;
		else
			return psched;

	case SCHED_HIDE:
		return slScientistHide;

	case SCHED_STARTLE:
		return slScientistStartle;

	case SCHED_CHASE_ENEMY:
		// without a knife, run away instead
		if ( pev->weapons != SCIENTIST_KNIFE )
			return slScientistCover;
		break;
	}

	return CTalkMonster::GetScheduleOfType( Type );
}


int CScientist :: IgnoreConditions ( void )
{
	int iIgnoreConditions = 0;

	if ( !m_iAIActive )
		return bits_COND_SEE_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND;

	if ( m_iBlockSound )
		iIgnoreConditions = bits_COND_HEAR_SOUND;

	return CTalkMonster::IgnoreConditions() | iIgnoreConditions;
}


int CScientist :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
{
	PainSound();

	return CTalkMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_BULLET);	// sic: every hit counts as a bullet
}


MONSTERSTATE CScientist :: GetIdealState ( void )
{
	if ( m_iIsScared == 1 && m_iAIActive == 1 )
	{
		// a scared scientist's script breaks on damage and sounds
		if ( IScheduleFlags() & SCIENTIST_SCRIPT_BREAK_CONDITIONS )
		{
			ALERT( at_console, "Now only interupted by interupt tagged scripts\n" );
			CalmDown();

			if ( m_MonsterState == MONSTERSTATE_SCRIPT )
			{
				ExitScriptedSequence();
				m_IdealMonsterState = MONSTERSTATE_COMBAT;
			}
		}
	}

	// with the AI off, he stays in his script
	if ( m_MonsterState == MONSTERSTATE_SCRIPT && !m_iAIActive )
		return m_IdealMonsterState;

	return CTalkMonster::GetIdealState();
}


int CScientist::FriendNumber( int arrayNumber )
{
	static int array[4] = { 1, 2, 3, 0 };	// sic: 3 is past the end of m_szFriends
	if ( arrayNumber < 4 )
		return array[ arrayNumber ];
	return arrayNumber;
}


//=========================================================
// Dead Scientist PROP
//=========================================================
class CDeadScientist : public CBaseMonster
{
public:
	void Spawn( void );
	int	Classify ( void ) { return	CLASS_HUMAN_PASSIVE; }

	void KeyValue( KeyValueData *pkvd );
	int	m_iPose;// which sequence to display
	static char *m_szPoses[7];
};
char *CDeadScientist::m_szPoses[] = { "lying_on_back", "lying_on_stomach", "dead_sitting", "dead_hang", "dead_table1", "dead_table2", "dead_table3" };

void CDeadScientist::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}
LINK_ENTITY_TO_CLASS( monster_scientist_dead, CDeadScientist );

//
// ********** DeadScientist SPAWN **********
//
void CDeadScientist :: Spawn( )
{
	PRECACHE_MODEL("models/scientist.mdl");
	SET_MODEL(ENT(pev), "models/scientist.mdl");

	pev->effects		= 0;
	pev->sequence		= 0;
	// Corpses have less health
	pev->health			= 8;//gSkillData.scientistHealth;

	m_bloodColor = BLOOD_COLOR_RED;

	if ( pev->body == -1 )
	{// -1 chooses a random head
		pev->body = RANDOM_LONG(0, NUM_SCIENTIST_HEADS-1);// pick a head, any head
	}

	pev->sequence = LookupSequence( m_szPoses[m_iPose] );
	if (pev->sequence == -1)
	{
		ALERT ( at_console, "Dead scientist with bad pose\n" );
	}

	//	pev->skin += 2; // use bloody skin -- UNDONE: Turn this back on when we have a bloody skin again!
	MonsterInitDead( FALSE );
}


//=========================================================
// Sitting Scientist PROP
//=========================================================

class CSittingScientist : public CScientist // kdb: changed from public CBaseMonster so he can speak
{
public:
	void Spawn( void );
	void  Precache( void );

	void EXPORT SittingThink( void );
	int	Classify ( void );
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	virtual void SetAnswerQuestion( CTalkMonster *pSpeaker );
	int FriendNumber( int arrayNumber );

	int FIdleSpeak ( void );
	int		m_baseSequence;
	int		m_headTurn;
	float	m_flResponseDelay;
};

LINK_ENTITY_TO_CLASS( monster_sitting_scientist, CSittingScientist );
TYPEDESCRIPTION	CSittingScientist::m_SaveData[] =
{
	// Don't need to save/restore m_baseSequence (recalced)
	DEFINE_FIELD( CSittingScientist, m_headTurn, FIELD_INTEGER ),
	DEFINE_FIELD( CSittingScientist, m_flResponseDelay, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CSittingScientist, CScientist );

// animation sequence aliases
typedef enum
{
SITTING_ANIM_sitlookleft,
SITTING_ANIM_sitlookright,
SITTING_ANIM_sitscared,
SITTING_ANIM_sitting2,
SITTING_ANIM_sitting3
} SITTING_ANIM;


//
// ********** Scientist SPAWN **********
//
void CSittingScientist :: Spawn( )
{
	PRECACHE_MODEL("models/scientist.mdl");
	SET_MODEL(ENT(pev), "models/scientist.mdl");
	Precache();
	InitBoneControllers();

	UTIL_SetSize(pev, Vector(-14, -14, 0), Vector(14, 14, 36));

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	pev->effects		= 0;
	pev->health			= 50;

	m_bloodColor = BLOOD_COLOR_RED;
	m_flFieldOfView		= VIEW_FIELD_WIDE; // indicates the width of this monster's forward view cone ( as a dotproduct result )

	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD;

	SetBits(pev->spawnflags, SF_MONSTER_PREDISASTER); // predisaster only!

	// sic: m_iHead is only set by CScientist::Spawn, so it is always 0 here
	switch ( m_iHead )
	{
	case HEAD_YOUNG:
		SetBodygroup( 1, 0 );
		break;
	case HEAD_BALD:
		SetBodygroup( 1, 1 );
		break;
	case HEAD_BEARD:
		SetBodygroup( 1, 2 );
		break;
	}

	if ( pev->body == -1 )
	{// -1 chooses a random head
		pev->body = RANDOM_LONG(0, NUM_SCIENTIST_HEADS-1);// pick a head, any head
	}

	m_baseSequence = LookupSequence( "sitlookleft" );
	pev->sequence = m_baseSequence + RANDOM_LONG(0,4);
	ResetSequenceInfo( );

	SetThink (&CSittingScientist::SittingThink);
	pev->nextthink = gpGlobals->time + 0.1;

	DROP_TO_FLOOR ( ENT(pev) );
}

void CSittingScientist :: Precache( void )
{
	m_baseSequence = LookupSequence( "sitlookleft" );
	TalkInit();
}

int CSittingScientist :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
{
	return CTalkMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_BULLET);
}

//=========================================================
// ID as a passive human
//=========================================================
int	CSittingScientist :: Classify ( void )
{
	return	CLASS_HUMAN_PASSIVE;
}


int CSittingScientist::FriendNumber( int arrayNumber )
{
	static int array[3] = { 2, 1, 0 };
	if ( arrayNumber < 3 )
		return array[ arrayNumber ];
	return arrayNumber;
}



//=========================================================
// sit, do stuff
//=========================================================
void CSittingScientist :: SittingThink( void )
{
	CBaseEntity *pent;

	StudioFrameAdvance( );

	// try to greet player
	if (FIdleHello())
	{
		pent = FindNearestFriend(TRUE);
		if (pent)
		{
			float yaw = VecToYaw(pent->pev->origin - pev->origin) - pev->angles.y;

			if (yaw > 180) yaw -= 360;
			if (yaw < -180) yaw += 360;

			if (yaw > 0)
				pev->sequence = m_baseSequence + SITTING_ANIM_sitlookleft;
			else
				pev->sequence = m_baseSequence + SITTING_ANIM_sitlookright;

		ResetSequenceInfo( );
		pev->frame = 0;
		SetBoneController( 0, 0 );
		}
	}
	else if (m_fSequenceFinished)
	{
		int i = RANDOM_LONG(0,99);
		m_headTurn = 0;

		if (m_flResponseDelay && gpGlobals->time > m_flResponseDelay)
		{
			// respond to question
			IdleRespond();
			pev->sequence = m_baseSequence + SITTING_ANIM_sitscared;
			m_flResponseDelay = 0;
		}
		else if (i < 30)
		{
			pev->sequence = m_baseSequence + SITTING_ANIM_sitting3;

			// turn towards player or nearest friend and speak

			if (!FBitSet(m_bitsSaid, bit_saidHelloPlayer))
				pent = FindNearestFriend(TRUE);
			else
				pent = FindNearestFriend(FALSE);

			if (!FIdleSpeak() || !pent)
			{
				m_headTurn = RANDOM_LONG(0,8) * 10 - 40;
				pev->sequence = m_baseSequence + SITTING_ANIM_sitting3;
			}
			else
			{
				// only turn head if we spoke
				float yaw = VecToYaw(pent->pev->origin - pev->origin) - pev->angles.y;

				if (yaw > 180) yaw -= 360;
				if (yaw < -180) yaw += 360;

				if (yaw > 0)
					pev->sequence = m_baseSequence + SITTING_ANIM_sitlookleft;
				else
					pev->sequence = m_baseSequence + SITTING_ANIM_sitlookright;

				//ALERT(at_console, "sitting speak\n");
			}
		}
		else if (i < 60)
		{
			pev->sequence = m_baseSequence + SITTING_ANIM_sitting3;
			m_headTurn = RANDOM_LONG(0,8) * 10 - 40;
			if ( UTIL_PercentChance( 5 ) )
			{
				//ALERT(at_console, "sitting speak2\n");
				FIdleSpeak();
			}
		}
		else if (i < 80)
		{
			pev->sequence = m_baseSequence + SITTING_ANIM_sitting2;
		}
		else if (i < 100)
		{
			pev->sequence = m_baseSequence + SITTING_ANIM_sitscared;
		}

		ResetSequenceInfo( );
		pev->frame = 0;
		SetBoneController( 0, m_headTurn );
	}
	pev->nextthink = gpGlobals->time + 0.1;
}

// prepare sitting scientist to answer a question
void CSittingScientist :: SetAnswerQuestion( CTalkMonster *pSpeaker )
{
	m_flResponseDelay = gpGlobals->time + RANDOM_FLOAT(3, 4);
	m_hTalkTarget = (CBaseMonster *)pSpeaker;
}


//=========================================================
// FIdleSpeak
// ask question of nearby friend, or make statement
//=========================================================
int CSittingScientist :: FIdleSpeak ( void )
{
	// try to start a conversation, or make statement
	int pitch = GetVoicePitch();

	if (!FOkToSpeak())
		return FALSE;

	// set global min delay for next conversation
	CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT(4.8, 5.2);

	// if there is a friend nearby to speak to, play sentence, set friend's response time, return

	// try to talk to any standing or sitting scientists nearby
	CBaseEntity *pentFriend = FindNearestFriend(FALSE);

	if (pentFriend && RANDOM_LONG(0,1))
	{
		CTalkMonster *pTalkMonster = GetClassPtr((CTalkMonster *)pentFriend->pev);
		pTalkMonster->SetAnswerQuestion( this );

		IdleHeadTurn(pentFriend->pev->origin);
		SENTENCEG_PlayRndSz( ENT(pev), m_szGrp[TLK_PQUESTION], 1.0, ATTN_IDLE, 0, pitch );
		// set global min delay for next conversation
		CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT(4.8, 5.2);
		return TRUE;
	}

	// otherwise, play an idle statement
	if (RANDOM_LONG(0,1))
	{
		SENTENCEG_PlayRndSz( ENT(pev), m_szGrp[TLK_PIDLE], 1.0, ATTN_IDLE, 0, pitch );
		// set global min delay for next conversation
		CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT(4.8, 5.2);
		return TRUE;
	}

	// never spoke
	CTalkMonster::g_talkWaitTime = 0;
	return FALSE;
}
