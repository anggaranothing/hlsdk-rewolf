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
// random_speaker: plays a sound now and then
// targ_speaker: plays a sound when triggered
// random_trigger: fires its target now and then
// player_speaker: plays a sentence on the player when triggered
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "gamerules.h"

//=========================================================
// random_speaker
//=========================================================
class CRand_Speaker : public CBaseEntity
{
public:
	CRand_Speaker( void );
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT Play( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t	m_Sound;
	float		m_fBaseTime;	// wait: the least time between plays
	float		m_fMaxAddTime;	// random: up to this much more
	float		m_fVolume;
};

LINK_ENTITY_TO_CLASS( random_speaker, CRand_Speaker );

TYPEDESCRIPTION	CRand_Speaker::m_SaveData[] =
{
	DEFINE_FIELD( CRand_Speaker, m_Sound, FIELD_STRING ),
	DEFINE_FIELD( CRand_Speaker, m_fVolume, FIELD_FLOAT ),
	DEFINE_FIELD( CRand_Speaker, m_fMaxAddTime, FIELD_FLOAT ),
	DEFINE_FIELD( CRand_Speaker, m_fBaseTime, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CRand_Speaker, CBaseEntity );

CRand_Speaker::CRand_Speaker( void )
{
	m_fBaseTime = 1;
	m_fMaxAddTime = 0;
	m_fVolume = 1;
	m_Sound = 0;
}

void CRand_Speaker::Precache( void )
{
	PRECACHE_SOUND( (char *)STRING( m_Sound ) );
}

void CRand_Speaker::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "niose" ) )	// sic
	{
		m_Sound = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "rsnoise" ) )
	{
		m_Sound = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "volume" ) )
	{
		m_fVolume = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "wait" ) )
	{
		m_fBaseTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "random" ) )
	{
		m_fMaxAddTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CRand_Speaker::Spawn( void )
{
	if ( !m_Sound )
	{
		UTIL_Remove( this );
		return;
	}

	Precache();

	if ( m_fVolume > 1 || m_fVolume < 0 )
		m_fVolume = 1;

	SetThink( &CRand_Speaker::Play );
	pev->nextthink = gpGlobals->time + m_fBaseTime + RANDOM_FLOAT( 0, m_fMaxAddTime );
}

void CRand_Speaker::Play( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, STRING( m_Sound ), m_fVolume, 0.8, 0, 100 + RANDOM_LONG( -3, 3 ) );
	pev->nextthink = gpGlobals->time + m_fBaseTime + RANDOM_FLOAT( 0, m_fMaxAddTime );
}

//=========================================================
// targ_speaker
//=========================================================
class CTarg_Speaker : public CBaseEntity
{
public:
	CTarg_Speaker( void );
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	virtual void EXPORT Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t	m_Sound;
	float		m_fBaseTime;	// unused, as in random_speaker
	float		m_fMaxAddTime;	// unused
	float		m_fVolume;		// sic: no key sets it, so it stays 1
};

LINK_ENTITY_TO_CLASS( targ_speaker, CTarg_Speaker );

TYPEDESCRIPTION	CTarg_Speaker::m_SaveData[] =
{
	DEFINE_FIELD( CTarg_Speaker, m_Sound, FIELD_STRING ),
	DEFINE_FIELD( CTarg_Speaker, m_fVolume, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CTarg_Speaker, CBaseEntity );

CTarg_Speaker::CTarg_Speaker( void )
{
	m_fVolume = 1;
	m_Sound = 0;
}

void CTarg_Speaker::Precache( void )
{
	PRECACHE_SOUND( (char *)STRING( m_Sound ) );
}

void CTarg_Speaker::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "tsnoise" ) )
	{
		m_Sound = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CTarg_Speaker::Spawn( void )
{
	if ( !m_Sound )
	{
		ALERT( at_console, "NO SOUND ASSIGNED TO TARG_SPEAKER!!\n" );
		UTIL_Remove( this );
		return;
	}

	Precache();

	if ( m_fVolume > 1 || m_fVolume < 0 )
		m_fVolume = 1;

	SetThink( NULL );
}

void CTarg_Speaker::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, STRING( m_Sound ), m_fVolume, 0.8, 0, 100 + RANDOM_LONG( -3, 3 ) );
	SetThink( NULL );
}

//=========================================================
// random_trigger
//=========================================================
class CRand_Trigger : public CBaseEntity
{
public:
	CRand_Trigger( void );
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT Rand_TriggerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT Rand_TriggerThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL	m_bState;		// on: firing
	float	m_fBaseTime;	// wait
	float	m_fMinAddTime;	// random_min
	float	m_fMaxAddTime;	// random_max
};

LINK_ENTITY_TO_CLASS( random_trigger, CRand_Trigger );

TYPEDESCRIPTION	CRand_Trigger::m_SaveData[] =
{
	DEFINE_FIELD( CRand_Trigger, m_fMaxAddTime, FIELD_FLOAT ),
	DEFINE_FIELD( CRand_Trigger, m_fMinAddTime, FIELD_FLOAT ),
	DEFINE_FIELD( CRand_Trigger, m_fBaseTime, FIELD_FLOAT ),
	DEFINE_FIELD( CRand_Trigger, m_bState, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CRand_Trigger, CBaseEntity );

CRand_Trigger::CRand_Trigger( void )
{
	m_fBaseTime = 5;
	m_fMinAddTime = 5;
	m_fMaxAddTime = 10;
	m_bState = FALSE;
}

void CRand_Trigger::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "start_state" ) )
	{
		m_bState = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "wait" ) )
	{
		m_fBaseTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "random_max" ) )
	{
		m_fMaxAddTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "random_min" ) )
	{
		m_fMinAddTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CRand_Trigger::Spawn( void )
{
	SetUse( &CRand_Trigger::Rand_TriggerUse );
	SetThink( &CRand_Trigger::Rand_TriggerThink );
	pev->solid = SOLID_NOT;
	pev->nextthink = gpGlobals->time + m_fBaseTime + RANDOM_FLOAT( m_fMinAddTime, m_fMaxAddTime );
}

void CRand_Trigger::Rand_TriggerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_bState == TRUE )
		m_bState = FALSE;
	else
		m_bState = TRUE;
}

void CRand_Trigger::Rand_TriggerThink( void )
{
	if ( !m_bState )
	{
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
	pev->nextthink = gpGlobals->time + m_fBaseTime + RANDOM_FLOAT( m_fMinAddTime, m_fMaxAddTime );
}

//=========================================================
// player_speaker
//=========================================================
class CDirectSentence : public CBaseEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

private:
	void EXPORT PlaySound( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	string_t	m_iszSentence;
	float		m_flLength;		// sentencelength: read, never used
};

LINK_ENTITY_TO_CLASS( player_speaker, CDirectSentence );

TYPEDESCRIPTION	CDirectSentence::m_SaveData[] =
{
	DEFINE_FIELD( CDirectSentence, m_iszSentence, FIELD_STRING ),
	DEFINE_FIELD( CDirectSentence, m_flLength, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CDirectSentence, CBaseEntity );

void CDirectSentence::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "sentencegroup" ) )
	{
		m_iszSentence = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sentencelength" ) )
	{
		m_flLength = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CDirectSentence::Spawn( void )
{
	// single player only
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	pev->solid = SOLID_NOT;
	SetThink( NULL );
	SetTouch( NULL );
	SetUse( &CDirectSentence::PlaySound );
	pev->classname = ALLOC_STRING( "player_speaker" );
}

void CDirectSentence::PlaySound( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pPlayer = CBaseEntity::Instance( INDEXENT( 1 ) );

	EMIT_SOUND_DYN( pPlayer->edict(), CHAN_STATIC,
 STRING( m_iszSentence ), 1.0, 0.8, 0, PITCH_NORM );
}
