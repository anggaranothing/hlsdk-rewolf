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
// the training room: the digits that count the score, the
// digit god that drives them, and the holograms to shoot at
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "effects.h"

//=========================================================
// entity_digit - one digit of the score, its number is the
// model's skin
//=========================================================
class CEntityDigit : public CBaseMonster
{
public:
	void Spawn( void );
	BOOL IsAlive( void ) { return FALSE; }
	int BloodColor( void ) { return DONT_BLEED; }
};
LINK_ENTITY_TO_CLASS( entity_digit, CEntityDigit );

void CEntityDigit::Spawn( void )
{
	pev->classname = MAKE_STRING( "entity_digit" );
	PRECACHE_MODEL( "models/digits.mdl" );
	SET_MODEL( ENT( pev ), "models/digits.mdl" );
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	pev->effects = 0;
	pev->health = 60;
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	SetThink( NULL );
	SetTouch( NULL );
}

//=========================================================
// entity_digitgod - shows a number of up to three digits
// and fires its targets once the number reaches maxdamage
//=========================================================
class CDigitGod : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	BOOL IsAlive( void ) { return FALSE; }
	int BloodColor( void ) { return DONT_BLEED; }

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT OutputThink( void );
	void SetNumber( int iNumber );

	int m_iNumber;
	int m_iMaxDamage;
	BOOL m_fHaveSucceeded;
	CBaseEntity *m_pDigits0;	// hundreds
	CBaseEntity *m_pDigits1;	// tens
	CBaseEntity *m_pDigits2;	// units
};
LINK_ENTITY_TO_CLASS( entity_digitgod, CDigitGod );

TYPEDESCRIPTION CDigitGod::m_SaveData[] =
{
	DEFINE_FIELD( CDigitGod, m_iNumber, FIELD_INTEGER ),
	DEFINE_FIELD( CDigitGod, m_iMaxDamage, FIELD_INTEGER ),
	DEFINE_FIELD( CDigitGod, m_fHaveSucceeded, FIELD_INTEGER ),
	DEFINE_FIELD( CDigitGod, m_pDigits0, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDigitGod, m_pDigits1, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDigitGod, m_pDigits2, FIELD_CLASSPTR ),
};

IMPLEMENT_SAVERESTORE( CDigitGod, CBaseEntity );	// sic: skips CBaseMonster's fields

void CDigitGod::Spawn( void )
{
	pev->classname = MAKE_STRING( "entity_digitgod" );
	SET_MODEL( ENT( pev ), "models/null.mdl" );
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	pev->effects = EF_NODRAW;
	pev->health = 60;
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	m_flFrameRate = 0;
	m_flGroundSpeed = 0;
	m_fHaveSucceeded = FALSE;

	// three digits in a row along the entity's right
	UTIL_MakeVectors( pev->angles );
	m_pDigits0 = CBaseEntity::Create( "entity_digit", pev->origin + gpGlobals->v_right * 16, pev->angles, edict() );
	m_pDigits1 = CBaseEntity::Create( "entity_digit", pev->origin, pev->angles, edict() );
	m_pDigits2 = CBaseEntity::Create( "entity_digit", pev->origin + gpGlobals->v_right * -16, pev->angles, edict() );

	SetThink( &CDigitGod::OutputThink );
	pev->nextthink = gpGlobals->time;
	m_iNumber = 0;
	SetNumber( 0 );
}

void CDigitGod::Precache( void )
{
	PRECACHE_MODEL( "models/null.mdl" );
	UTIL_PrecacheOther( "entity_digit" );
}

void CDigitGod::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "maxdamage" ) )
	{
		m_iMaxDamage = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		pkvd->fHandled = FALSE;
}

void CDigitGod::OutputThink( void )
{
	pev->nextthink = gpGlobals->time + 0.5;
}

// Shows the number (at most 999) and fires the targets the first time it
// reaches maxdamage.
void CDigitGod::SetNumber( int iNumber )
{
	if ( m_pDigits0 && m_pDigits1 && m_pDigits2 )
	{
		m_iNumber = iNumber;
		if ( iNumber > 999 )
			m_iNumber = 999;

		int iValue = m_iNumber;
		if ( iValue < 10 )
		{
			// sic: a number under 10 never reaches maxdamage
			m_pDigits0->pev->skin = 0;
			m_pDigits1->pev->skin = 0;
			m_pDigits2->pev->skin = iValue;
			return;
		}

		int iUnits = iValue % 10;
		iValue -= iUnits;
		int iTens = iValue / 10;
		if ( iTens > 9 )
			iTens = iTens % 10;

		m_pDigits0->pev->skin = ( iValue - iTens * 10 ) / 100;
		m_pDigits1->pev->skin = iTens;
		m_pDigits2->pev->skin = iUnits;

		if ( m_iNumber >= m_iMaxDamage && !m_fHaveSucceeded )
		{
			SUB_UseTargets( this, USE_ON, 0 );
			m_fHaveSucceeded = TRUE;
		}
	}
}

//=========================================================
// hologram_beak - a hologram of a beak, a tube or a raptor
// that the player shoots at; the damage goes to the digit
// god named by its target
//=========================================================
#define HOLOGRAM_BEAK		0
#define HOLOGRAM_TUBE		1
#define HOLOGRAM_RAPTOR		2

class CHologram : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	BOOL IsAlive( void ) { return FALSE; }
	int Classify( void ) { return CLASS_ALIEN_BEAK; }
	int BloodColor( void ) { return DONT_BLEED; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT SpawnThink( void );
	void EXPORT HologramThink( void );
	void EXPORT DieThink( void );
	void EXPORT RespawnHologram( void );
	void EXPORT UseToggle( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void PlayIdle( void );
	void PlayFlinch( void );
	void PlayDeath( void );

	float m_flNextAnimationSwitch;
	float m_flNextFlinchAnimation;
	BOOL m_bDying;
	int m_iDamageCount;
	CDigitGod *m_pDigitGod;
	float m_flDieTime;
	int m_iBlast;
	int m_iState;			// shown and shootable
	int m_iCreature;		// HOLOGRAM_BEAK, _TUBE or _RAPTOR
	string_t m_iFailName;
	BOOL m_bFailOnDeath;
	float m_flNextIdleSoundTime;
	CBaseEntity *m_pTargetFail;
};
LINK_ENTITY_TO_CLASS( hologram_beak, CHologram );

TYPEDESCRIPTION CHologram::m_SaveData[] =
{
	DEFINE_FIELD( CHologram, m_flNextAnimationSwitch, FIELD_TIME ),
	DEFINE_FIELD( CHologram, m_flNextFlinchAnimation, FIELD_TIME ),
	DEFINE_FIELD( CHologram, m_bDying, FIELD_INTEGER ),
	DEFINE_FIELD( CHologram, m_iDamageCount, FIELD_INTEGER ),
	DEFINE_FIELD( CHologram, m_iBlast, FIELD_INTEGER ),
	DEFINE_FIELD( CHologram, m_flDieTime, FIELD_FLOAT ),
	DEFINE_FIELD( CHologram, m_pDigitGod, FIELD_CLASSPTR ),
	DEFINE_FIELD( CHologram, m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( CHologram, m_iCreature, FIELD_INTEGER ),
	DEFINE_FIELD( CHologram, m_pTargetFail, FIELD_CLASSPTR ),
	DEFINE_FIELD( CHologram, m_iFailName, FIELD_STRING ),
	DEFINE_FIELD( CHologram, m_bFailOnDeath, FIELD_INTEGER ),
	DEFINE_FIELD( CHologram, m_flNextIdleSoundTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CHologram, CBaseEntity );	// sic: skips CBaseMonster's fields

void CHologram::Spawn( void )
{
	pev->classname = MAKE_STRING( "hologram_beak" );
	Precache( );

	if ( m_iCreature == HOLOGRAM_BEAK )
		SET_MODEL( ENT( pev ), "models/beak.mdl" );
	else if ( m_iCreature == HOLOGRAM_TUBE )
		SET_MODEL( ENT( pev ), "models/tube.mdl" );
	else
		SET_MODEL( ENT( pev ), "models/raptor.mdl" );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	pev->effects = EF_NODRAW;
	pev->health = 64;
	pev->yaw_speed = 5;
	pev->ideal_yaw = pev->angles.y;
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 40 ) );
	InitBoneControllers( );
	ChangeYaw( 360 );

	m_flFrameRate = 75;
	m_flGroundSpeed = 0;
	m_iDamageCount = 0;
	m_bFailOnDeath = TRUE;
	ResetSequenceInfo( );

	pev->renderfx = kRenderFxDistort;
	pev->renderamt = 150;
	pev->rendermode = kRenderTransAdd;

	SetThink( &CHologram::HologramThink );
	SetUse( &CHologram::UseToggle );
	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_iCreature == HOLOGRAM_BEAK )
		SetSequenceByName( "sleeping" );
	else if ( m_iCreature == HOLOGRAM_TUBE )
		SetSequenceByName( "idle" );
	else
		SetSequenceByName( "idlenormal" );
	ResetSequenceInfo( );

	m_flNextAnimationSwitch = RANDOM_FLOAT( 5, 10 ) + gpGlobals->time;
	m_flNextFlinchAnimation = 0;
	m_bDying = FALSE;
	m_flDieTime = 0;
	m_iState = 0;
}

// Waits until no player or monster stands where the hologram appears.
void CHologram::SpawnThink( void )
{
	CBaseEntity *pList[2];
	Vector vecMins = Vector( pev->origin.x - 34, pev->origin.y - 34, pev->origin.z );
	Vector vecMaxs = Vector( pev->origin.x + 34, pev->origin.y + 34, pev->origin.z );

	if ( UTIL_EntitiesInBox( pList, 2, vecMins, vecMaxs, FL_CLIENT | FL_MONSTER ) )
	{
		pev->nextthink = gpGlobals->time + 0.2;
		return;
	}

	pev->effects = 0;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_AIM;
	m_iState = 1;

	Vector vecCenter = Center( );
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecCenter );
		WRITE_BYTE( TE_IMPLOSION );
		WRITE_COORD( vecCenter.x );
		WRITE_COORD( vecCenter.y );
		WRITE_COORD( vecCenter.z );
		WRITE_BYTE( 50 );	// radius
		WRITE_BYTE( 100 );	// count
		WRITE_BYTE( 10 );	// life
	MESSAGE_END( );

	SetThink( &CHologram::HologramThink );
	pev->nextthink = gpGlobals->time + 0.1;
	UTIL_SetOrigin( pev, pev->origin );
}

void CHologram::Precache( void )
{
	PRECACHE_MODEL( "models/beak.mdl" );
	m_iBlast = PRECACHE_MODEL( "sprites/white.spr" );
	PRECACHE_MODEL( "models/tube.mdl" );
	PRECACHE_SOUND( "ambience/holowarpin.wav" );
	PRECACHE_MODEL( "models/raptor.mdl" );
	PRECACHE_SOUND( "ambience/holodisplay.wav" );
}

void CHologram::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "creaturetype" ) )
	{
		m_iCreature = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "targetfail" ) )
	{
		m_iFailName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		pkvd->fHandled = FALSE;	// sic: forgets creaturetype was handled
}

void CHologram::HologramThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_flNextIdleSoundTime < gpGlobals->time )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "ambience/holodisplay.wav", 0.3, ATTN_NORM, 0, PITCH_NORM );
		m_flNextIdleSoundTime = gpGlobals->time + 2.63;
	}

	if ( m_flFrameRate == 0.0 )
	{
		pev->sequence = 0;
		ResetSequenceInfo( );
	}
	else
		StudioFrameAdvance( );

	if ( m_flDieTime != 0 && m_flDieTime < gpGlobals->time )
	{
		SetThink( &CHologram::DieThink );
		pev->nextthink = gpGlobals->time;
	}

	if ( m_fSequenceFinished && m_flNextAnimationSwitch < gpGlobals->time )
	{
		PlayIdle( );
		m_flNextAnimationSwitch = RANDOM_FLOAT( 3, 12 ) + gpGlobals->time;
		return;
	}

	if ( m_fSequenceFinished && !m_fSequenceLoops )
	{
		pev->animtime = gpGlobals->time;
		pev->framerate = 1.0;
		m_fSequenceFinished = FALSE;
		m_flLastEventCheck = gpGlobals->time;
		pev->frame = 0;
	}
}

int CHologram::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( !m_bDying )
	{
		// the digit god counts the damage
		edict_t *pentDigitGod = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) );
		if ( !FNullEnt( pentDigitGod ) )
		{
			m_pDigitGod = (CDigitGod *)CBaseEntity::Instance( pentDigitGod );
			m_iDamageCount += (int)flDamage;
			if ( m_pDigitGod )
				m_pDigitGod->SetNumber( m_iDamageCount );
			m_flDieTime = gpGlobals->time + 3.0;
		}

		if ( m_flNextFlinchAnimation < gpGlobals->time )
		{
			m_flNextFlinchAnimation = gpGlobals->time + 0.25;
			PlayFlinch( );
			m_flNextAnimationSwitch = gpGlobals->time;
		}
	}

	return 1;
}

// Fades out; when the score fell short, fires targetfail first.
void CHologram::DieThink( void )
{
	if ( !m_bDying )
	{
		if ( !m_pDigitGod || m_iDamageCount >= m_pDigitGod->m_iMaxDamage )
			m_bFailOnDeath = FALSE;
		else
			m_bFailOnDeath = TRUE;

		if ( m_pDigitGod && m_pDigitGod->m_fHaveSucceeded )
			m_bFailOnDeath = FALSE;

		if ( m_bFailOnDeath )
		{
			if ( !m_pTargetFail && m_iFailName )
			{
				edict_t *pentFail = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( m_iFailName ) );
				if ( !FNullEnt( pentFail ) )
					m_pTargetFail = CBaseEntity::Instance( pentFail );
			}

			if ( m_pTargetFail )
				m_pTargetFail->Use( this, this, USE_ON, 0 );

			m_bFailOnDeath = FALSE;
		}

		m_bDying = TRUE;
		PlayDeath( );
	}

	pev->renderfx &= ~kRenderFxDistort;
	pev->renderamt -= 5;

	if ( pev->renderamt <= 0 )
	{
		Vector vecCenter = Center( );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecCenter );
			WRITE_BYTE( TE_IMPLOSION );
			WRITE_COORD( vecCenter.x );
			WRITE_COORD( vecCenter.y );
			WRITE_COORD( vecCenter.z );
			WRITE_BYTE( 100 );	// radius
			WRITE_BYTE( 100 );	// count
			WRITE_BYTE( 20 );	// life
		MESSAGE_END( );

		pev->renderamt = 0;
		SetThink( &CHologram::RespawnHologram );
		pev->nextthink = gpGlobals->time + 2.0;
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

void CHologram::RespawnHologram( void )
{
	pev->renderfx = kRenderFxDistort;
	pev->effects |= EF_NOINTERP;
	PlayIdle( );
	ResetSequenceInfo( );

	m_flNextAnimationSwitch = RANDOM_FLOAT( 5, 10 ) + gpGlobals->time;
	m_flNextFlinchAnimation = 0;
	m_bDying = FALSE;
	pev->health = 60;
	pev->renderamt = 150;
	SetThink( &CHologram::HologramThink );
	m_iDamageCount = 0;
	m_flDieTime = 0;

	if ( m_pDigitGod )
		m_pDigitGod->SetNumber( 0 );

	EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "ambience/holodisplay.wav", 0.3, ATTN_NORM, 0, PITCH_NORM );
	m_flNextIdleSoundTime = gpGlobals->time + 2.63;
	pev->nextthink = gpGlobals->time;
}

// Shows the hologram (once the spot is clear) or hides it.
void CHologram::UseToggle( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !m_iState )
	{
		SetThink( &CHologram::SpawnThink );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		pev->solid = SOLID_NOT;
		pev->takedamage = DAMAGE_NO;
		pev->effects = EF_NODRAW;
		m_iState = 0;
		EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "ambience/holodisplay.wav", 0.3, ATTN_NORM, 0, PITCH_NORM );
	}

	UTIL_SetOrigin( pev, pev->origin );
}

void CHologram::PlayIdle( void )
{
	if ( m_iCreature == HOLOGRAM_BEAK )
	{
		switch ( RANDOM_LONG( 1, 6 ) )
		{
		case 1:	SetSequenceByName( "idlelookout" ); break;
		case 2:	SetSequenceByName( "walk" ); break;
		case 3:	SetSequenceByName( "run" ); break;
		case 4:	SetSequenceByName( "leap" ); break;
		case 5:	SetSequenceByName( "bite" ); break;
		case 6:	SetSequenceByName( "roarangry" ); break;
		}
	}
	else if ( m_iCreature == HOLOGRAM_TUBE )
	{
		switch ( RANDOM_LONG( 1, 4 ) )
		{
		case 1:	SetSequenceByName( "walk" ); break;
		case 2:	SetSequenceByName( "idle" ); break;
		case 3:	SetSequenceByName( "mourn" ); break;
		case 4:	SetSequenceByName( "shoot" ); break;
		}
	}
	else
	{
		switch ( RANDOM_LONG( 1, 6 ) )
		{
		case 1:	SetSequenceByName( "idlenormal" ); break;
		case 2:	SetSequenceByName( "idlechitter" ); break;
		case 3:	SetSequenceByName( "bitechunk" ); break;
		case 4:	SetSequenceByName( "chewchunk" ); break;
		case 5:	SetSequenceByName( "roarangry" ); break;
		case 6:	SetSequenceByName( "run" ); break;
		}
	}

	ResetSequenceInfo( );
	if ( m_flFrameRate == 0.0 )
	{
		pev->sequence = 0;
		ResetSequenceInfo( );
	}
	pev->frame = 0;
}

void CHologram::PlayFlinch( void )
{
	if ( m_iCreature == HOLOGRAM_BEAK )
	{
		switch ( RANDOM_LONG( 0, 1 ) )
		{
		case 0:	SetSequenceByName( "bigflinch" ); break;
		case 1:	SetSequenceByName( "smallflinch" ); break;
		}
	}
	else if ( m_iCreature == HOLOGRAM_TUBE )
		SetSequenceByName( "flinch" );
	else
	{
		if ( RANDOM_LONG( 0, 1 ) )
			SetSequenceByName( "smallflinch" );
		else
			SetSequenceByName( "bigflinch" );
	}

	ResetSequenceInfo( );
	if ( m_flFrameRate == 0.0 )
	{
		pev->sequence = 0;
		ResetSequenceInfo( );
	}
	pev->frame = 0;
}

void CHologram::PlayDeath( void )
{
	if ( m_iCreature == HOLOGRAM_BEAK )
	{
		if ( RANDOM_LONG( 0, 1 ) )
			SetSequenceByName( "diesimple" );
		else
			SetSequenceByName( "dietwitch" );
	}
	else if ( m_iCreature == HOLOGRAM_TUBE )
	{
		if ( RANDOM_LONG( 0, 1 ) )
			SetSequenceByName( "diesimple" );
		else
			SetSequenceByName( "dieback" );
	}
	else
	{
		if ( RANDOM_LONG( 0, 1 ) )
			SetSequenceByName( "diesimple" );
		else
			SetSequenceByName( "diefight" );
	}

	ResetSequenceInfo( );
	if ( m_flFrameRate == 0.0 )
	{
		pev->sequence = 0;
		ResetSequenceInfo( );
	}
	pev->frame = 0;
	STOP_SOUND( ENT( pev ), CHAN_STATIC, "ambience/holodisplay.wav" );
}

//=========================================================
// hologram_damage - a hologram that wants one kind of
// damage: the right kind fires its target, any other kind
// fires targetfail
//=========================================================
class CHologramDamage : public CHologram
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	CBaseEntity *m_pTargetWin;
	int m_iDamage;		// the kind of damage wanted, see TakeDamage
	float m_flNextUseTarget;
};
LINK_ENTITY_TO_CLASS( hologram_damage, CHologramDamage );

TYPEDESCRIPTION CHologramDamage::m_SaveData[] =
{
	DEFINE_FIELD( CHologramDamage, m_iDamage, FIELD_INTEGER ),
	DEFINE_FIELD( CHologramDamage, m_pTargetWin, FIELD_CLASSPTR ),
	DEFINE_FIELD( CHologramDamage, m_flNextUseTarget, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CHologramDamage, CHologram );

void CHologramDamage::Spawn( void )
{
	m_flNextUseTarget = 0;
	CHologram::Spawn( );
	m_bFailOnDeath = FALSE;
	pev->classname = MAKE_STRING( "hologram_damage" );
}

void CHologramDamage::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "targetfail" ) )
	{
		m_iFailName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "damagetype" ) )
	{
		m_iDamage = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "creaturetype" ) )
	{
		m_iCreature = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		pkvd->fHandled = FALSE;	// sic: forgets the first two keys were handled
}

int CHologramDamage::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	pev->health -= flDamage;
	if ( pev->health <= 0 )
	{
		// it never dies of the damage, it fades out
		pev->health = 1;
		m_flDieTime = gpGlobals->time;
		m_flNextFlinchAnimation = gpGlobals->time + 1.0;
	}

	if ( m_iDamage == 3 )
		m_iDamage = 4;	// sic: kind 3 is never dealt, it means kind 4

	if ( !m_pTargetWin )
	{
		edict_t *pentWin = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) );
		if ( !FNullEnt( pentWin ) )
			m_pTargetWin = CBaseEntity::Instance( pentWin );
	}

	if ( m_iFailName )
	{
		edict_t *pentFail = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( m_iFailName ) );
		if ( !FNullEnt( pentFail ) )
			m_pTargetFail = CBaseEntity::Instance( pentFail );
	}

	if ( m_flNextFlinchAnimation < gpGlobals->time )
	{
		m_flNextFlinchAnimation = gpGlobals->time + 0.25;
		PlayFlinch( );
		m_flNextAnimationSwitch = gpGlobals->time;
	}

	// what hit it
	int iDamage = 4;
	if ( pevInflictor )
	{
		if ( FClassnameIs( pevInflictor, "gauss_charged" ) )
			iDamage = 2;
		else if ( FClassnameIs( pevInflictor, "gauss_bolt" ) )
			iDamage = 1;
		else if ( bitsDamageType & DMG_ENERGYBEAM )
			iDamage = 0;
		else if ( bitsDamageType & DMG_ACID )
			iDamage = 5;
		else if ( bitsDamageType & DMG_BURN )
			iDamage = 6;
		else if ( bitsDamageType & DMG_BLAST )
			iDamage = 7;
		else if ( bitsDamageType & DMG_RADIATION )
			iDamage = 8;
	}

	if ( m_flNextUseTarget < gpGlobals->time )
	{
		if ( m_iDamage == iDamage )
		{
			if ( m_pTargetWin )
				SUB_UseTargets( this, USE_ON, 0 );
		}
		else if ( m_pTargetFail )
			m_pTargetFail->Use( this, this, USE_ON, 0 );

		m_flNextUseTarget = gpGlobals->time + 3.0;
	}

	return 1;
}
