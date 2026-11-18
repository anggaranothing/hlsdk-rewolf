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
// the unarmed gunman: a friendly gunman without a gun, who
// talks and gets out of the way
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"talkmonster.h"
#include	"schedule.h"
#include	"defaultai.h"
#include	"scripted.h"

class CFriendlyUnarmed : public CTalkMonster
{
public:
	CFriendlyUnarmed( void );
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void SetYawSpeed( void );
	int  Classify ( void );
	void TalkInit( void );

	void EXPORT FollowerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void DeclineFollowing( void );

	int	ObjectCaps( void ) { return CTalkMonster :: ObjectCaps() | FCAP_IMPULSE_USE; }
	void Killed( entvars_t *pevAttacker, int iGib );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

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
};

LINK_ENTITY_TO_CLASS( monster_human_unarmed, CFriendlyUnarmed );

TYPEDESCRIPTION	CFriendlyUnarmed::m_SaveData[] = 
{
	DEFINE_FIELD( CFriendlyUnarmed, m_iGunState, FIELD_INTEGER ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_ANSWER, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_QUESTION, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_IDLE, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_STARE, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_USE, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_UNUSE, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_STOP, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_NOSHOOT, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_HELLO, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_PLHURT1, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_PLHURT2, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_PLHURT3, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_PHELLO, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_PIDLE, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_PQUESTION, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_SMELL, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_WOUND, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_MORTAL, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_KILL, FIELD_STRING ),
	DEFINE_FIELD( CFriendlyUnarmed, m_iszTLK_ATTACK, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CFriendlyUnarmed, CTalkMonster );

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CFriendlyUnarmed :: Classify ( void )
{
	return	CLASS_PLAYER_ALLY;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CFriendlyUnarmed :: SetYawSpeed ( void )
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
// Spawn
//=========================================================
void CFriendlyUnarmed :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/gunmantrooper_ng.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->health			= 80;
	pev->view_ofs		= Vector ( 0, 0, 50 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so npc will notice player and say hello
	m_MonsterState		= MONSTERSTATE_NONE;

	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	MonsterInit();

	SetBodygroup( 0, 0 );
	if ( m_iGunState == 2 )
		SetBodygroup( 1, 2 );
	else
		SetBodygroup( 1, 0 );
	SetBodygroup( 2, 0 );

	SetUse( &CFriendlyUnarmed::FollowerUse );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CFriendlyUnarmed :: Precache()
{
	PRECACHE_MODEL("models/gunmantrooper_ng.mdl");

	// every new barney must call this, otherwise
	// when a level is loaded, nobody will talk (time is reset to now)
	TalkInit();
	CTalkMonster::Precache();
}	

void CFriendlyUnarmed :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "gn_answer" ) )
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
		m_iszTLK_PLHURT1 = ALLOC_STRING( pkvd->szValue );	// sic: all three set the first
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gn_plhurt3" ) )
	{
		m_iszTLK_PLHURT1 = ALLOC_STRING( pkvd->szValue );
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

CFriendlyUnarmed :: CFriendlyUnarmed( void )
{
	m_iGunState = 0;

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
void CFriendlyUnarmed :: TalkInit()
{
	CTalkMonster::TalkInit();

	// get voice for head - just one barney voice for now
	m_voicePitch = 100;

	m_szFriends[0] = "monster_human_unarmed";
	m_szFriends[1] = "monster_human_gunman";

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
}

void CFriendlyUnarmed::Killed( entvars_t *pevAttacker, int iGib )
{
	SetUse( NULL );	
	CTalkMonster::Killed( pevAttacker, iGib );
}

//=========================================================
// FollowerUse - they never follow, they only answer
//=========================================================
void CFriendlyUnarmed::FollowerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Don't allow use during a scripted_sentence
	if ( m_useTime > gpGlobals->time )
		return;

	DeclineFollowing();
}

void CFriendlyUnarmed::DeclineFollowing( void )
{
	PlaySentence( m_szGrp[TLK_USE], 2, VOL_NORM, ATTN_NORM );
}
