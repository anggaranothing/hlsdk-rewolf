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
// decorations: props that sit in the level, animate, gib
// and react to the player; the embryos in their tubes, the
// lab clutter, the asteroids and space debris, and the
// training bot
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "effects.h"
#include "soundent.h"
#include "decals.h"
#include "weapons.h"
#include "customentity.h"
#include "sphereexplosion.h"

//=========================================================
// CDecore - the base of the decorations: a monster that
// stands in place, animates and notices players touching it
//=========================================================
class CDecore : public CBaseMonster
{
public:
	void KeyValue( KeyValueData *pkvd );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	// a player touched the decoration (slot 139)
	virtual int PlayerTouch( CBaseEntity *pOther );

	void DecoreInit( void );
	void EXPORT DecoreTouch( CBaseEntity *pOther );
	void EXPORT Animate( void );

	int m_iAnimate;		// +0x270
	int m_maxFrame;		// +0x274
	int m_iUnused[3];	// +0x278
	int m_fAnimEnd;		// +0x284: CBigFootCrush waits for it, nothing sets it
	int m_iUnused2;		// +0x288
};

void CDecore::DecoreInit( void )
{
	pev->takedamage = DAMAGE_YES;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ) );
	SetTouch( &CDecore::DecoreTouch );
	pev->nextthink = gpGlobals->time + 0.1;
	SetThink( &CDecore::Animate );
	pev->health = 20;
	ResetSequenceInfo( );
}

void CDecore::DecoreTouch( CBaseEntity *pOther )
{
	if ( pOther && ( pOther->pev->flags & FL_CLIENT ) )
		PlayerTouch( pOther );
}

void CDecore::Animate( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CDecore::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "frame" ) )
	{
		pev->frame = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		pev->frame = 0;	// sic: every other key is dropped
}

//=========================================================
// decore_baboon - a dying baboon twitching on the ground
//=========================================================
class CDecoreBaboon : public CDecore
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	void GibMonster( void );
	int PlayerTouch( CBaseEntity *pOther );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
};
LINK_ENTITY_TO_CLASS( decore_baboon, CDecoreBaboon );

TYPEDESCRIPTION	CDecoreBaboon::m_SaveData[] =
{
	DEFINE_FIELD( CDecoreBaboon, m_maxFrame, FIELD_INTEGER ),
	DEFINE_FIELD( CDecoreBaboon, m_iAnimate, FIELD_INTEGER ),
};

void CDecoreBaboon::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/baboon.mdl" );
	DecoreInit( );
	pev->health = 30;
	SetSequenceByName( "deadtwitch" );
	ResetSequenceInfo( );
	m_bloodColor = BLOOD_COLOR_RED;
}

void CDecoreBaboon::GibMonster( void )
{
	SpawnGib( "models/hgibs.mdl", 1, 10, 0 );
	SpawnGib( "models/hgibs.mdl", 1, 10, 0 );
	SpawnGib( "models/hgibs.mdl", 1, 10, 0 );
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "ambience/monkydie.wav", 1, 0.8, 0, PITCH_NORM );
}

int CDecoreBaboon::Classify( void )
{
	return CLASS_NONE;
}

void CDecoreBaboon::Precache( void )
{
	PRECACHE_MODEL( "models/baboon.mdl" );
	PRECACHE_MODEL( "models/hgibs.mdl" );
	PRECACHE_SOUND( "ambience/monkydie.wav" );
}

int CDecoreBaboon::PlayerTouch( CBaseEntity *pOther )
{
	return 0;
}

IMPLEMENT_SAVERESTORE( CDecoreBaboon, CDecore );

//=========================================================
// decore_pipes - pipes that start on a random frame
//=========================================================
class CDecorePipes : public CDecore
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	int PlayerTouch( CBaseEntity *pOther );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
};
LINK_ENTITY_TO_CLASS( decore_pipes, CDecorePipes );

TYPEDESCRIPTION	CDecorePipes::m_SaveData[] =
{
	DEFINE_FIELD( CDecorePipes, m_maxFrame, FIELD_INTEGER ),
	DEFINE_FIELD( CDecorePipes, m_iAnimate, FIELD_INTEGER ),
};

void CDecorePipes::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/pipes.mdl" );
	pev->takedamage = DAMAGE_NO;
	m_iAnimate = 1;
	m_maxFrame = 255;
	ResetSequenceInfo( );
	pev->frame = rand( ) % m_maxFrame;
	DecoreInit( );	// sic: makes the pipes take damage after all
}

int CDecorePipes::Classify( void )
{
	return CLASS_NONE;
}

void CDecorePipes::Precache( void )
{
	PRECACHE_MODEL( "models/pipes.mdl" );
}

int CDecorePipes::PlayerTouch( CBaseEntity *pOther )
{
	return 0;
}

IMPLEMENT_SAVERESTORE( CDecorePipes, CDecore );

//=========================================================
// decore_mushroom - a mushroom that puffs spores when a
// player steps on it and bursts when it's hurt
//=========================================================
int g_sModelIndexGibMushroom;	// sprites/gibmushroom.spr

class CDecoreMushroom : public CDecore
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	int PlayerTouch( CBaseEntity *pOther );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
};
LINK_ENTITY_TO_CLASS( decore_mushroom, CDecoreMushroom );

void CDecoreMushroom::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/mushroom.mdl" );
	DecoreInit( );
	m_bloodColor = DONT_BLEED;
}

int CDecoreMushroom::Classify( void )
{
	return CLASS_NONE;
}

void CDecoreMushroom::Precache( void )
{
	PRECACHE_MODEL( "models/mushroom.mdl" );
	PRECACHE_SOUND( "mushroom/mushroom_hit2.wav" );
	PRECACHE_SOUND( "mushroom/mushroom_hit3.wav" );
	PRECACHE_SOUND( "mushroom/mushroom_pop1.wav" );
	g_sModelIndexGibMushroom = PRECACHE_MODEL( "sprites/gibmushroom.spr" );
}

int CDecoreMushroom::PlayerTouch( CBaseEntity *pOther )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE_SPRAY );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_COORD( 1 );
		WRITE_SHORT( g_sModelIndexGibMushroom );
		WRITE_BYTE( 16 );	// count
		WRITE_BYTE( 40 );	// speed
		WRITE_BYTE( 100 );	// noise
	MESSAGE_END( );

	if ( RANDOM_LONG( 0, 1 ) )
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "mushroom/mushroom_hit2.wav", 1, 0.8, 0, PITCH_NORM );
	else
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "mushroom/mushroom_hit3.wav", 1, 0.8, 0, PITCH_NORM );

	SetTouch( NULL );
	return TRUE;
}

int CDecoreMushroom::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE_SPRAY );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_COORD( 1 );
		WRITE_SHORT( g_sModelIndexGibMushroom );
		WRITE_BYTE( 16 );	// count
		WRITE_BYTE( 40 );	// speed
		WRITE_BYTE( 100 );	// noise
	MESSAGE_END( );

	EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "mushroom/mushroom_pop1.wav", 1, 0.8, 0, PITCH_NORM );

	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
	return 0;
}

//=========================================================
// decore_mushroom2 - the other mushroom
//=========================================================
class CDecoreMushroom2 : public CDecore
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	int PlayerTouch( CBaseEntity *pOther );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
};
LINK_ENTITY_TO_CLASS( decore_mushroom2, CDecoreMushroom2 );

void CDecoreMushroom2::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/mushroom2.mdl" );
	DecoreInit( );
	m_bloodColor = DONT_BLEED;
}

int CDecoreMushroom2::Classify( void )
{
	return CLASS_NONE;
}

void CDecoreMushroom2::Precache( void )
{
	PRECACHE_MODEL( "models/mushroom2.mdl" );
	PRECACHE_SOUND( "mushroom/mushroom_hit1.wav" );
	PRECACHE_SOUND( "mushroom/mushroom_hit2.wav" );
	PRECACHE_SOUND( "mushroom/mushroom_pop2.wav" );
	g_sModelIndexGibMushroom = PRECACHE_MODEL( "sprites/gibmushroom.spr" );
}

int CDecoreMushroom2::PlayerTouch( CBaseEntity *pOther )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE_SPRAY );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_COORD( 1 );
		WRITE_SHORT( g_sModelIndexGibMushroom );
		WRITE_BYTE( 16 );	// count
		WRITE_BYTE( 40 );	// speed
		WRITE_BYTE( 100 );	// noise
	MESSAGE_END( );

	// sic: both picks play hit1; hit2 is precached and never played
	if ( RANDOM_LONG( 0, 1 ) )
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "mushroom/mushroom_hit1.wav", 1, 0.8, 0, PITCH_NORM );
	else
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "mushroom/mushroom_hit1.wav", 1, 0.8, 0, PITCH_NORM );

	SetTouch( NULL );
	return TRUE;
}

int CDecoreMushroom2::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE_SPRAY );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_COORD( 1 );
		WRITE_SHORT( g_sModelIndexGibMushroom );
		WRITE_BYTE( 16 );	// count
		WRITE_BYTE( 40 );	// speed
		WRITE_BYTE( 100 );	// noise
	MESSAGE_END( );

	EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "mushroom/mushroom_pop2.wav", 1, 0.8, 0, PITCH_NORM );

	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
	return 0;
}

//=========================================================
// decore_cactus - pricks players who walk into it, at most
// once a second, and breaks into pieces
//=========================================================
class CDecoreCactus : public CDecore
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	void GibMonster( void );
	int PlayerTouch( CBaseEntity *pOther );

	float m_flNextHurt;
};
LINK_ENTITY_TO_CLASS( decore_cactus, CDecoreCactus );

void CDecoreCactus::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/cactus.mdl" );
	DecoreInit( );
	pev->max_health = pev->health;
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 64 ) );
	m_flNextHurt = gpGlobals->time;
	m_bloodColor = BLOOD_COLOR_YELLOW;

	if ( DROP_TO_FLOOR( ENT( pev ) ) == 0 )
	{
		ALERT( at_error, "Item %s fell out of level at %f,%f,%f", STRING( pev->classname ), pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
	}
}

int CDecoreCactus::Classify( void )
{
	return CLASS_NONE;
}

void CDecoreCactus::Precache( void )
{
	PRECACHE_MODEL( "models/cactus.mdl" );
	PRECACHE_MODEL( "models/cactusgib.mdl" );
}

int CDecoreCactus::PlayerTouch( CBaseEntity *pOther )
{
	if ( m_flNextHurt < gpGlobals->time )
	{
		m_flNextHurt = gpGlobals->time + 1;
		pOther->TakeDamage( pev, pev, 1, DMG_SLASH );
	}
	return TRUE;
}

void CDecoreCactus::GibMonster( void )
{
	for ( int i = 1; i < 16; i += 2 )
	{
		CBaseMonster *pGib = SpawnGib( "models/cactusgib.mdl", RANDOM_FLOAT( 0.5, 1.5 ), i % 3, 0 );
		if ( pGib )
			UTIL_SetOrigin( pGib->pev, pev->origin + Vector( 0, 0, i ) );
	}
}

//=========================================================
// decore_prickle - a small prickly plant
//=========================================================
class CDecorePrickle : public CDecore
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );

	float m_flNextHurt;	// set, never read
};
LINK_ENTITY_TO_CLASS( decore_prickle, CDecorePrickle );

void CDecorePrickle::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/prickle.mdl" );
	DecoreInit( );
	UTIL_SetSize( pev, Vector( -8, -8, 0 ), Vector( 8, 8, 8 ) );
	m_flNextHurt = gpGlobals->time;
	pev->skin = RANDOM_LONG( 0, 1 );
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->takedamage = DAMAGE_YES;

	if ( DROP_TO_FLOOR( ENT( pev ) ) == 0 )
	{
		ALERT( at_error, "Item %s fell out of level at %f,%f,%f", STRING( pev->classname ), pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
	}
}

int CDecorePrickle::Classify( void )
{
	return CLASS_NONE;
}

void CDecorePrickle::Precache( void )
{
	PRECACHE_MODEL( "models/prickle.mdl" );
}

//=========================================================
// decore_gutspile, decore_bodygib, decore_hatgib - human
// remains that bleed when shot and, once, smell for the
// monsters ten seconds after they spawn
//=========================================================
class CDecoreHumanGibsParts : public CDecore
{
public:
	CDecoreHumanGibsParts( ) { }

	void Spawn( void );
	int Classify( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	// the model of this kind of remains (slot 140)
	virtual void SetGibModel( void ) = 0;

private:
	void EXPORT GutSmell( void );
};

void CDecoreHumanGibsParts::GutSmell( void )
{
	CSoundEnt::InsertSound( bits_SOUND_MEAT, pev->origin, 400, 10 );
}

class CDecoreGutspile : public CDecoreHumanGibsParts
{
public:
	void Precache( void );
	void SetGibModel( void );
};
LINK_ENTITY_TO_CLASS( decore_gutspile, CDecoreGutspile );

void CDecoreHumanGibsParts::Spawn( void )
{
	Precache( );
	SetGibModel( );
	DecoreInit( );
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 40 ) );
	pev->solid = SOLID_NOT;
	m_bloodColor = BLOOD_COLOR_RED;
	m_iAnimate = 0;
	m_maxFrame = 0;
	pev->health = 400;

	if ( DROP_TO_FLOOR( ENT( pev ) ) == 0 )
	{
		ALERT( at_error, "Item %s fell out of level at %f,%f,%f", STRING( pev->classname ), pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
		return;
	}

	SetThink( &CDecoreHumanGibsParts::GutSmell );
	pev->nextthink = gpGlobals->time + 10;
}

int CDecoreHumanGibsParts::Classify( void )
{
	return CLASS_NONE;
}

int CDecoreHumanGibsParts::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( RANDOM_FLOAT( 0, 1 ) < 0.5 )
		UTIL_BloodDrips( pev->origin, UTIL_RandomBloodVector( ), BLOOD_COLOR_RED, 10 );

	CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
	return 1;
}

void CDecoreGutspile::SetGibModel( void )
{
	SET_MODEL( ENT( pev ), "models/gutspile.mdl" );
}

void CDecoreGutspile::Precache( void )
{
	PRECACHE_MODEL( "models/gutspile.mdl" );
}

class CDecoreBodygib : public CDecoreHumanGibsParts
{
public:
	void Precache( void );
	void SetGibModel( void );
};
LINK_ENTITY_TO_CLASS( decore_bodygib, CDecoreBodygib );

void CDecoreBodygib::SetGibModel( void )
{
	SET_MODEL( ENT( pev ), "models/bodygib.mdl" );
}

void CDecoreBodygib::Precache( void )
{
	PRECACHE_MODEL( "models/bodygib.mdl" );
}

class CDecoreHatgib : public CDecoreHumanGibsParts
{
public:
	void Precache( void );
	void SetGibModel( void );
};
LINK_ENTITY_TO_CLASS( decore_hatgib, CDecoreHatgib );

void CDecoreHatgib::SetGibModel( void )
{
	SET_MODEL( ENT( pev ), "models/hatgib.mdl" );
}

void CDecoreHatgib::Precache( void )
{
	PRECACHE_MODEL( "models/hatgib.mdl" );
}

//=========================================================
// decore_icebeak - the beak frozen in a block of ice
//=========================================================
class CDecoreIcebeak : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
};
LINK_ENTITY_TO_CLASS( decore_icebeak, CDecoreIcebeak );

void CDecoreIcebeak::Precache( void )
{
	PRECACHE_MODEL( "models/icebeak.mdl" );
}

void CDecoreIcebeak::Spawn( void )
{
	pev->model = MAKE_STRING( "models/icebeak.mdl" );
	pev->classname = MAKE_STRING( "decore_icebeak" );
	SET_MODEL( ENT( pev ), "models/icebeak.mdl" );
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	pev->effects = 0;
	pev->takedamage = DAMAGE_NO;
}

//=========================================================
// decore_ice - a block of ice, with a beak inside when
// "beakinside" is set; it shatters when used
//=========================================================
class CIce : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	int Classify( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int m_iBeakInside;
	CBaseEntity *pBeak;

private:
	void EXPORT SpawnIceBeak( void )
	{
		if ( m_iBeakInside == 1 )
		{
			pev->renderamt = 180;
			pBeak = GetClassPtr( (CDecoreIcebeak *)NULL );
			pBeak->Spawn( );
			pBeak->pev->origin = pev->origin;
			pBeak->pev->angles = pev->angles;
		}
	}
	void EXPORT Melt( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		if ( m_iBeakInside && pBeak )
			UTIL_Remove( pBeak );

		SpawnGib( "models/ceilinggibs.mdl", 1, 0, 0 );
		SpawnGib( "models/ceilinggibs.mdl", 1, 0, 0 );
		SpawnGib( "models/ceilinggibs.mdl", 1, 0, 0 );
		SpawnGib( "models/ceilinggibs.mdl", 1, 0, 0 );
		UTIL_Remove( this );
	}
};

TYPEDESCRIPTION	CIce::m_SaveData[] =
{
	DEFINE_FIELD( CIce, m_iBeakInside, FIELD_INTEGER ),
	DEFINE_FIELD( CIce, pBeak, FIELD_CLASSPTR ),
};

LINK_ENTITY_TO_CLASS( decore_ice, CIce );

void CIce::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "beakinside" ) )
	{
		m_iBeakInside = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}

void CIce::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/ice.mdl" );
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 40 ) );
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->takedamage = DAMAGE_NO;
	pBeak = NULL;
	pev->renderamt = 200;
	pev->rendermode = kRenderTransTexture;

	if ( DROP_TO_FLOOR( ENT( pev ) ) == 0 )
	{
		ALERT( at_error, "Item %s fell out of level at %f,%f,%f", STRING( pev->classname ), pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
		return;
	}

	SetTouch( NULL );
	SetUse( &CIce::Melt );
	SetThink( &CIce::SpawnIceBeak );
	pev->nextthink = gpGlobals->time + 0.1;
}

int CIce::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	return 0;
}

int CIce::Classify( void )
{
	return CLASS_NONE;
}

void CIce::Precache( void )
{
	PRECACHE_MODEL( "models/ceilinggibs.mdl" );
	PRECACHE_MODEL( "models/ice.mdl" );
	if ( m_iBeakInside )
		UTIL_PrecacheOther( "decore_icebeak" );
}

IMPLEMENT_SAVERESTORE( CIce, CBaseMonster );

//=========================================================
// decore_swampplants - swamp plants
//=========================================================
class CDecoreSwampPlants : public CDecore
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	int PlayerTouch( CBaseEntity *pOther );
};
LINK_ENTITY_TO_CLASS( decore_swampplants, CDecoreSwampPlants );

void CDecoreSwampPlants::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/swampstuff.mdl" );
	DecoreInit( );
	pev->solid = SOLID_NOT;
}

int CDecoreSwampPlants::Classify( void )
{
	return CLASS_NONE;
}

void CDecoreSwampPlants::Precache( void )
{
	PRECACHE_MODEL( "models/swampstuff.mdl" );
}

int CDecoreSwampPlants::PlayerTouch( CBaseEntity *pOther )
{
	return 0;
}

//=========================================================
// decore_goldskull - the golden monkey skull; it opens
// when used
//=========================================================
class CDecoreMonkeySkull : public CDecore
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	int PlayerTouch( CBaseEntity *pOther );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

private:
	void EXPORT MonkeyUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		SetSequenceByName( "opening" );
		SetUse( NULL );
	}
};

TYPEDESCRIPTION	CDecoreMonkeySkull::m_SaveData[] =
{
	DEFINE_FIELD( CDecoreMonkeySkull, m_maxFrame, FIELD_INTEGER ),
	DEFINE_FIELD( CDecoreMonkeySkull, m_iAnimate, FIELD_INTEGER ),
};

LINK_ENTITY_TO_CLASS( decore_goldskull, CDecoreMonkeySkull );

void CDecoreMonkeySkull::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/goldskull.mdl" );
	DecoreInit( );
	pev->solid = SOLID_NOT;
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	SetUse( &CDecoreMonkeySkull::MonkeyUse );
	SetThink( NULL );
	pev->takedamage = DAMAGE_NO;
}

int CDecoreMonkeySkull::Classify( void )
{
	return CLASS_NONE;
}

void CDecoreMonkeySkull::Precache( void )
{
	PRECACHE_MODEL( "models/goldskull.mdl" );
}

int CDecoreMonkeySkull::PlayerTouch( CBaseEntity *pOther )
{
	return 0;
}

IMPLEMENT_SAVERESTORE( CDecoreMonkeySkull, CDecore );

//=========================================================
// decore_foot - the renesaur's foot, hidden until used;
// then it stamps down
//=========================================================
class CBigFootCrush : public CDecore
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	int PlayerTouch( CBaseEntity *pOther );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

private:
	void EXPORT BigFootUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		SetSequenceByName( "footcrush" );
		SetUse( NULL );
		pev->effects &= ~EF_NODRAW;
		SetThink( &CBigFootCrush::CheckAnimEnd );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	void EXPORT CheckAnimEnd( void );
};

TYPEDESCRIPTION	CBigFootCrush::m_SaveData[] =
{
	DEFINE_FIELD( CBigFootCrush, m_maxFrame, FIELD_INTEGER ),
	DEFINE_FIELD( CBigFootCrush, m_iAnimate, FIELD_INTEGER ),
};

void CBigFootCrush::CheckAnimEnd( void )
{
	// sic: nothing sets m_fAnimEnd, so the foot stays
	if ( m_fAnimEnd )
		SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}

LINK_ENTITY_TO_CLASS( decore_foot, CBigFootCrush );

void CBigFootCrush::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/renesaurfoot.mdl" );
	UTIL_SetSize( pev, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ) );
	UTIL_SetOrigin( pev, pev->origin );
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	SetUse( &CBigFootCrush::BigFootUse );
	SetThink( NULL );
	SetTouch( NULL );
	pev->effects = EF_NODRAW;
	pev->takedamage = DAMAGE_NO;
}

int CBigFootCrush::Classify( void )
{
	return CLASS_NONE;
}

void CBigFootCrush::Precache( void )
{
	PRECACHE_MODEL( "models/renesaurfoot.mdl" );
}

int CBigFootCrush::PlayerTouch( CBaseEntity *pOther )
{
	return 0;
}

IMPLEMENT_SAVERESTORE( CBigFootCrush, CDecore );

//=========================================================
// xenome embryo: a small creature that idles, chitters now
// and then, and bursts when used or hurt
//=========================================================
class CXenomeEmbryo : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void GibMonster( void );

private:
	void EXPORT XenomeEmbryoGibUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		ALERT( at_console, "Using Embryo\n" );
		Killed( pev, GIB_ALWAYS );
	}
	void EXPORT IdleSoundThink( void )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "xenome/xmbryo_idle1.wav", 1, ATTN_NORM );
			break;
		case 1:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "xenome/xmbryo_idle2.wav", 1, ATTN_NORM );
			break;
		case 2:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "xenome/xmbryo_idle3.wav", 1, ATTN_NORM );
			break;
		}

		pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 1, 25 );
	}
};

LINK_ENTITY_TO_CLASS( monster_xenome_embryo, CXenomeEmbryo );

void CXenomeEmbryo::Spawn( void )
{
	Precache( );

	pev->health = 20;
	pev->takedamage = DAMAGE_YES;
	m_bloodColor = 100;

	SET_MODEL( ENT(pev), "models/xmbryo.mdl" );
	UTIL_SetSize( pev, Vector( -6, -6, -6 ), Vector( 6, 6, 12 ) );

	pev->solid = SOLID_SLIDEBOX;
	m_flFieldOfView = -0.7;
	m_MonsterState = MONSTERSTATE_IDLE;
	pev->spawnflags |= SF_MONSTER_FALL_TO_GROUND;

	SetUse( &CXenomeEmbryo::XenomeEmbryoGibUse );
	SetThink( &CXenomeEmbryo::IdleSoundThink );
	pev->nextthink = gpGlobals->time + RANDOM_LONG( 0, 6 );
}

int CXenomeEmbryo::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, DMG_ALWAYSGIB );
}

void CXenomeEmbryo::GibMonster( void )
{
	SpawnGib( "models/xenomegibs.mdl", 0.3, 0, 0 );
	SpawnGib( "models/xenomegibs.mdl", 0.3, 1, 0 );
	SpawnGib( "models/xenomegibs.mdl", 0.3, 2, 0 );
	CBaseMonster::GibMonster( );
}

int CXenomeEmbryo::Classify( void )
{
	return CLASS_ALIEN_CRITTER;
}

void CXenomeEmbryo::Precache( void )
{
	PRECACHE_MODEL( "models/xmbryo.mdl" );
	PRECACHE_MODEL( "models/xenomegibs.mdl" );
	PRECACHE_SOUND( "xenome/xmbryo_idle1.wav" );
	PRECACHE_SOUND( "xenome/xmbryo_idle2.wav" );
	PRECACHE_SOUND( "xenome/xmbryo_idle3.wav" );
}

//=========================================================
// sitting tube mortar: a tube that sits on the floor and,
// touched or hurt, bursts into a spray of tube rockets
//=========================================================
class CSittingTubeMortar : public CDecore
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	int PlayerTouch( CBaseEntity *pOther );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int m_iModelIndexTubeSuicide;
};
LINK_ENTITY_TO_CLASS( decore_sittingtubemortar, CSittingTubeMortar );

void CSittingTubeMortar::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT(pev), "models/tubemortar.mdl" );
	DecoreInit( );
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 48 ) );
	SetSequenceByName( "standing" );
	ResetSequenceInfo( );

	if ( DROP_TO_FLOOR( ENT(pev) ) == 0 )
	{
		ALERT( at_error, "Item %s fell out of level at %f,%f,%f", STRING( pev->classname ), pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
	}
}

int CSittingTubeMortar::Classify( void )
{
	return CLASS_NONE;
}

void CSittingTubeMortar::Precache( void )
{
	m_iModelIndexTubeSuicide = PRECACHE_MODEL( "sprites/tubeguts.spr" );
	PRECACHE_MODEL( "models/tubemortar.mdl" );
	PRECACHE_SOUND( "tubequeen/mortarexplode1.wav" );
	PRECACHE_SOUND( "tubequeen/mortarexplode2.wav" );
	PRECACHE_SOUND( "tubequeen/mortarexplode3.wav" );
	PRECACHE_SOUND( "tube/tube_explodingdeath.wav" );
	UTIL_PrecacheOther( "tuberocket" );
}

int CSittingTubeMortar::PlayerTouch( CBaseEntity *pOther )
{
	Vector vecDir, vecSrc, vecAngles;

	EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "tube/tube_explodingdeath.wav", 1, ATTN_NORM, 0, PITCH_NORM );

	int iRockets = RANDOM_LONG( 6, 12 );
	for ( int i = 0; i < iRockets; i++ )
	{
		vecDir = m_vecEnemyLKP - pev->origin;
		vecDir = vecDir.Normalize( );
		vecAngles = UTIL_VecToAngles( vecDir );	// sic: never used
		vecSrc = pev->origin + vecDir * 25;
		vecSrc.z += 16;
		vecDir.z = vecDir.z * -1;

		CBaseEntity *pRocket = CBaseEntity::Create( "tuberocket", vecSrc, UTIL_VecToAngles( vecDir ), edict( ) );
		UTIL_MakeVectors( pRocket->pev->angles );
		pRocket->pev->velocity = gpGlobals->v_forward * 300;

		CBaseMonster *pMonster = pRocket->MyMonsterPointer( );
		if ( pMonster )
			pMonster->m_hEnemy = m_hEnemy;

		// sic: the rest goes through pMonster without testing it
		vecDir.x = RANDOM_FLOAT( -0.5, 0.5 );
		vecDir.y = RANDOM_FLOAT( -0.5, 0.5 );
		vecDir.z = RANDOM_FLOAT( 0.1, 0.5 );
		vecDir = vecDir.Normalize( );
		pMonster->pev->angles = UTIL_VecToAngles( vecDir );
		UTIL_MakeAimVectors( pMonster->pev->angles );
		pMonster->pev->velocity = gpGlobals->v_forward * 300;
		pMonster->pev->origin.x += RANDOM_FLOAT( -20, 20 );
		pMonster->pev->origin.y += RANDOM_FLOAT( -20, 20 );
		pMonster->pev->origin.z += RANDOM_FLOAT( 32, 64 );
		pMonster->pev->takedamage = DAMAGE_NO;
		pMonster->pev->movetype = MOVETYPE_TOSS;
		pMonster->pev->nextthink = gpGlobals->time + 0.3;

		if ( m_MonsterState == MONSTERSTATE_COMBAT )
		{
			pMonster->m_vecEnemyLKP = m_vecEnemyLKP;
			pMonster->m_hEnemy = m_hEnemy;
		}

		pMonster->m_flNextAttack = gpGlobals->time + RANDOM_FLOAT( 0.3, 0.6 );
	}

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( m_iModelIndexTubeSuicide );
		WRITE_BYTE( 10 );	// scale * 10
		WRITE_BYTE( 15 );	// framerate
		WRITE_BYTE( TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND );
	MESSAGE_END( );

	UTIL_Remove( this );
	return 1;
}

int CSittingTubeMortar::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	PlayerTouch( NULL );
	return 0;
}

TYPEDESCRIPTION	CSittingTubeMortar::m_SaveData[] =
{
	DEFINE_FIELD( CSittingTubeMortar, m_iModelIndexTubeSuicide, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CSittingTubeMortar, CDecore );

//=========================================================
// the sack around a tube embryo
//=========================================================
class CDecoreSack : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	Schedule_t *GetSchedule( void );
};
LINK_ENTITY_TO_CLASS( decore_sack, CDecoreSack );

void CDecoreSack::Precache( void )
{
	PRECACHE_MODEL( "models/sack.mdl" );
}

int CDecoreSack::Classify( void )
{
	return CLASS_ALIEN_CRITTER;
}

void CDecoreSack::Spawn( void )
{
	pev->model = MAKE_STRING( "models/sack.mdl" );
	pev->classname = MAKE_STRING( "decore_sack" );
	SET_MODEL( ENT(pev), "models/sack.mdl" );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	pev->effects = 0;
	pev->takedamage = DAMAGE_NO;
	pev->spawnflags |= SF_MONSTER_FALL_TO_GROUND;

	MonsterInit( );
}

Schedule_t *CDecoreSack::GetSchedule( void )
{
	return GetScheduleOfType( SCHED_LOCKED_IDLE_STAND );
}

//=========================================================
// tube embryo: an embryo floating in its sack, which
// twitches when shocked
//=========================================================
class CTubeEmbryo : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	Schedule_t *GetSchedule( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CBaseMonster *pSack;

private:
	void EXPORT SpawnSack( void )
	{
		pSack = GetClassPtr( (CDecoreSack *)NULL );
		pSack->Spawn( );
		pSack->pev->origin = pev->origin;
		pSack->pev->angles = pev->angles;
		pSack->pev->renderamt = 180;
		pSack->pev->rendermode = kRenderTransTexture;

		UTIL_SetSize( pev, Vector( -12, -12, -16 ), Vector( 12, 12, 24 ) );
		MonsterInit( );
		SetUse( &CTubeEmbryo::ShockAnim );
	}
	void EXPORT ShockAnim( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		SetSequenceByName( "shock" );
		if ( pSack )
			pSack->SetSequenceByName( "shock" );
		else
			ALERT( at_console, "no sack for shocking!\n" );
	}
};

LINK_ENTITY_TO_CLASS( monster_tube_embryo, CTubeEmbryo );

void CTubeEmbryo::Spawn( void )
{
	Precache( );

	pev->health = 30;
	m_bloodColor = 50;

	SET_MODEL( ENT(pev), "models/tubryo.mdl" );
	UTIL_SetSize( pev, Vector( -6, -6, -6 ), Vector( 6, 6, 12 ) );

	pSack = NULL;
	pev->solid = SOLID_SLIDEBOX;
	m_flFieldOfView = -0.7;
	m_MonsterState = MONSTERSTATE_IDLE;
	pev->nextthink = gpGlobals->time + 0.1;
	pev->spawnflags |= SF_MONSTER_FALL_TO_GROUND;

	SetThink( &CTubeEmbryo::SpawnSack );
}

void CTubeEmbryo::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( pSack )
		UTIL_Remove( pSack );

	CBaseEntity::Killed( pevAttacker, iGib );
}

Schedule_t *CTubeEmbryo::GetSchedule( void )
{
	return GetScheduleOfType( SCHED_LOCKED_IDLE_STAND );
}

int CTubeEmbryo::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, DMG_ALWAYSGIB );
}

int CTubeEmbryo::Classify( void )
{
	return CLASS_ALIEN_CRITTER;
}

void CTubeEmbryo::Precache( void )
{
	PRECACHE_MODEL( "models/tubryo.mdl" );
	UTIL_PrecacheOther( "decore_sack" );
}

TYPEDESCRIPTION	CTubeEmbryo::m_SaveData[] =
{
	DEFINE_FIELD( CTubeEmbryo, pSack, FIELD_CLASSPTR ),
};

IMPLEMENT_SAVERESTORE( CTubeEmbryo, CBaseMonster );

// CDecore's own defaults sit here in retail, between the tube embryo and
// the lab stuff.
int CDecore::PlayerTouch( CBaseEntity *pOther )
{
	return 0;
}

int CDecore::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// lab stuff: clutter on the lab tables that breaks into
// pieces at the first hit
//=========================================================
class CDecoreLabstuff : public CDecore
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	int ObjectCaps( void );
	int PlayerTouch( CBaseEntity *pOther );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	int m_iGibModel;
};
LINK_ENTITY_TO_CLASS( decore_labstuff, CDecoreLabstuff );

void CDecoreLabstuff::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT(pev), "models/labstuff.mdl" );
	DecoreInit( );
}

int CDecoreLabstuff::Classify( void )
{
	return CLASS_NONE;
}

int CDecoreLabstuff::ObjectCaps( void )
{
	return 0;
}

void CDecoreLabstuff::Precache( void )
{
	PRECACHE_MODEL( "models/labstuff.mdl" );
	m_iGibModel = PRECACHE_MODEL( "models/decoregibs.mdl" );
}

int CDecoreLabstuff::PlayerTouch( CBaseEntity *pOther )
{
	return 0;
}

int CDecoreLabstuff::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BREAKMODEL );

		// position
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );

		// size
		WRITE_COORD( 10 );
		WRITE_COORD( 10 );
		WRITE_COORD( 10 );

		// velocity
		WRITE_COORD( 10 );
		WRITE_COORD( 10 );
		WRITE_COORD( 32 );

		// randomization
		WRITE_BYTE( 5 );

		// model id#
		WRITE_SHORT( m_iGibModel );

		// # of shards
		WRITE_BYTE( 5 );

		// duration
		WRITE_BYTE( 25 );

		// flags
		WRITE_BYTE( BREAK_GLASS );
	MESSAGE_END( );

	UTIL_Remove( this );
	return 0;
}

//=========================================================
// asteroid: a rock that spins in place in the sky; the
// bigger it is, the faster it may spin
//=========================================================
class CAsteroid : public CBaseMonster
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	BOOL IsAlive( void ) { return FALSE; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int m_iBody;
};
LINK_ENTITY_TO_CLASS( decore_asteroid, CAsteroid );

TYPEDESCRIPTION	CAsteroid::m_SaveData[] =
{
	DEFINE_FIELD( CAsteroid, m_iBody, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CAsteroid, CBaseMonster );

void CAsteroid::KeyValue( KeyValueData *pkvd )
{
	m_iBody = 1;	// sic: every key resets the size the "asteroidsize" key set

	if ( FStrEq( pkvd->szKeyName, "asteroidsize" ) )
	{
		m_iBody = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		pkvd->fHandled = FALSE;
}

void CAsteroid::Spawn( void )
{
	pev->classname = MAKE_STRING( "decore_asteroid" );
	PRECACHE_MODEL( "models/rockspin.mdl" );
	SET_MODEL( ENT(pev), "models/rockspin.mdl" );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_NO;
	pev->effects = 0;
	pev->health = 60;
	pev->body = m_iBody;
	pev->velocity = g_vecZero;

	float flSpin = ( m_iBody + 1 ) * 10;
	pev->avelocity.x = RANDOM_FLOAT( -flSpin, flSpin );
	pev->avelocity.y = RANDOM_FLOAT( -flSpin, flSpin );
	pev->avelocity.z = RANDOM_FLOAT( -flSpin, flSpin );

	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	pev->nextthink = gpGlobals->time + 0.1;
	SetTouch( NULL );
}

//=========================================================
// space debris: hidden until used, then flies at its target
// tumbling, and goes away when it stops or its life runs out
//=========================================================
class CSpaceDebris : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	BOOL IsAlive( void ) { return FALSE; }

	void EXPORT UseToggle( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT DebrisRemoveThink( void );
	void EXPORT DebrisTouch( CBaseEntity *pOther );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	float m_flSpeed;
	float m_flAngleSpeed;
	float m_flLife;
	string_t m_iszModel;
	Vector m_vecDir;		// sic: the target's position, not a direction
	Vector m_vecLastOrigin;
};
LINK_ENTITY_TO_CLASS( decore_spacedebris, CSpaceDebris );

TYPEDESCRIPTION	CSpaceDebris::m_SaveData[] =
{
	DEFINE_FIELD( CSpaceDebris, m_flSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CSpaceDebris, m_flAngleSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CSpaceDebris, m_vecDir, FIELD_VECTOR ),
	DEFINE_FIELD( CSpaceDebris, m_flLife, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CSpaceDebris, CBaseMonster );

void CSpaceDebris::Spawn( void )
{
	pev->classname = MAKE_STRING( "decore_spacedebris" );
	Precache( );
	SET_MODEL( ENT(pev), STRING( m_iszModel ) );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	pev->effects = EF_NODRAW;
	pev->health = 60;
	pev->gravity = 0.001;
	pev->velocity = g_vecZero;

	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	SetThink( NULL );
	SetTouch( NULL );
	SetUse( &CSpaceDebris::UseToggle );
}

void CSpaceDebris::Precache( void )
{
	if ( m_iszModel )
		PRECACHE_MODEL( (char *)STRING( m_iszModel ) );
}

void CSpaceDebris::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "modelname" ) )
	{
		m_iszModel = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "forwardspeed" ) )
	{
		m_flSpeed = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "anglespeed" ) )
	{
		m_flAngleSpeed = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "debrislife" ) )
	{
		m_flLife = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		pkvd->fHandled = FALSE;	// sic: drops the keys above as unhandled
}

void CSpaceDebris::UseToggle( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	edict_t *pentTarget = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) );
	if ( !FNullEnt( pentTarget ) )
	{
		CBaseEntity *pTarget = CBaseEntity::Instance( pentTarget );
		if ( pTarget )
			m_vecDir = pTarget->pev->origin;
	}

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->effects = 0;
	pev->gravity = 0.001;
	Vector vecToTarget = m_vecDir - pev->origin;
	pev->velocity = vecToTarget.Normalize( ) * m_flSpeed;
	pev->avelocity.x = RANDOM_FLOAT( -m_flAngleSpeed, m_flAngleSpeed );
	pev->avelocity.y = RANDOM_FLOAT( -m_flAngleSpeed, m_flAngleSpeed );
	pev->avelocity.z = RANDOM_FLOAT( -m_flAngleSpeed, m_flAngleSpeed );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	SetThink( &CSpaceDebris::DebrisRemoveThink );
	pev->nextthink = gpGlobals->time + 0.2;
	SetTouch( &CSpaceDebris::DebrisTouch );

	UTIL_SetOrigin( pev, pev->origin );
	m_vecLastOrigin = pev->origin;
}

void CSpaceDebris::DebrisRemoveThink( void )
{
	pev->solid = SOLID_NOT;
	m_flLife -= 0.1;

	// stuck: it hasn't moved since the last think
	if ( m_vecLastOrigin == pev->origin )
	{
		UTIL_Remove( this );
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	m_vecLastOrigin = pev->origin;

	if ( m_flLife <= 0 )
		UTIL_Remove( this );
}

void CSpaceDebris::DebrisTouch( CBaseEntity *pOther )
{
	// pieces of debris pass through each other
	if ( FStrEq( STRING( pev->classname ), STRING( pOther->pev->classname ) ) )
		pev->solid = SOLID_NOT;
}

//=========================================================
// the training bot: a battery-powered drone that floats along its
// path_corners with three beams and a glow between its arms, slows
// down after a while, and falls smoking or blows up when killed
//=========================================================

#define SF_TRAININGBOT_NORING	0x0400	// no ring event while it floats

class CDummyBot : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int BloodColor( void ) { return 156; }	// the robots' oil
	int Classify( void ) { return CLASS_ROBOT; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );

	void EXPORT FloatThink( void );
	void EXPORT BotSparkThink( void );
	void EXPORT SmokeFallThink( void );
	void EXPORT BotExplodeTouch( CBaseEntity *pOther );

	void BotExplode( void );
	void CreateBeams( void );
	void UpdateBeams( void );
	void ClearBeams( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CBaseEntity *m_pGoalEnt;	// the path_corner it floats to
	Vector m_vGoalPos;
	CBeam *AttackBeam1;
	CBeam *AttackBeam2;
	CBeam *AttackBeam3;
	CSprite *m_pSprite;			// the glow between the arms
	int m_iExplosion;
	float m_fSpeed;
	float m_flSlowDown;			// when it halves its speed
	float m_flNextRingTime;
	unsigned short m_usBeamRing;
	int m_iSmoke;
	int m_iBeam;
};
LINK_ENTITY_TO_CLASS( monster_trainingbot, CDummyBot );

TYPEDESCRIPTION	CDummyBot::m_SaveData[] =
{
	DEFINE_FIELD( CDummyBot, m_pGoalEnt, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDummyBot, AttackBeam1, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDummyBot, AttackBeam2, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDummyBot, AttackBeam3, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDummyBot, m_pSprite, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDummyBot, m_vGoalPos, FIELD_VECTOR ),
	DEFINE_FIELD( CDummyBot, m_iExplosion, FIELD_INTEGER ),
	DEFINE_FIELD( CDummyBot, m_fSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CDummyBot, m_flSlowDown, FIELD_TIME ),
	DEFINE_FIELD( CDummyBot, m_flNextRingTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CDummyBot, CBaseMonster );

void CDummyBot::Spawn( void )
{
	pev->classname = MAKE_STRING( "monster_trainingbot" );
	Precache( );

	SET_MODEL( ENT( pev ), "models/batterybot.mdl" );
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_AIM;
	pev->effects = 0;
	pev->health = 60;
	pev->velocity = g_vecZero;
	UTIL_SetSize( pev, Vector( -32, -32, -32 ), Vector( 32, 32, 32 ) );
	UTIL_SetOrigin( pev, pev->origin );
	SetTouch( NULL );

	if ( FStringNull( pev->target ) )
		pev->target = ALLOC_STRING( "bot1" );

	edict_t *pentTarget = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) );
	if ( !FNullEnt( pentTarget ) )
	{
		m_pGoalEnt = CBaseEntity::Instance( pentTarget );
		if ( m_pGoalEnt )
			m_vGoalPos = m_pGoalEnt->pev->origin;
	}

	m_fSpeed = RANDOM_FLOAT( 75, 175 ) * 2;
	m_flSlowDown = gpGlobals->time + 4.0;
	Vector vecDir = m_vGoalPos - pev->origin;
	pev->velocity = vecDir.Normalize( ) * m_fSpeed;

	CreateBeams( );
	MonsterInit( );

	SetThink( &CDummyBot::FloatThink );
	pev->nextthink = gpGlobals->time + 0.1;

	SetSequenceByName( "idle" );
	ResetSequenceInfo( );
	pev->frame = 0;
	pev->avelocity.y = RANDOM_FLOAT( -20, 20 );

	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "drone/drone_idle.wav", 0.65, 0.5 );
	m_flNextRingTime = 0;
}

void CDummyBot::Precache( void )
{
	PRECACHE_MODEL( "models/batterybot.mdl" );
	m_iBeam = PRECACHE_MODEL( "sprites/xbeam1.spr" );
	PRECACHE_MODEL( "sprites/hugeball.spr" );
	m_iExplosion = PRECACHE_MODEL( "sprites/explosion4.spr" );
	PRECACHE_MODEL( "models/battgib.mdl" );

	PRECACHE_SOUND( "drone/drone_idle.wav" );
	PRECACHE_SOUND( "drone/drone_flinch1.wav" );
	PRECACHE_SOUND( "drone/drone_flinch2.wav" );

	UTIL_PrecacheOther( "sphere_explosion" );

	m_usBeamRing = PRECACHE_EVENT( 1, "events/batbotbeam.sc" );
	m_iSmoke = PRECACHE_MODEL( "sprites/dmlsmoke.spr" );
}

void CDummyBot::FloatThink( void )
{
	if ( m_flSlowDown && m_flSlowDown < gpGlobals->time )
	{
		m_flSlowDown = 0;
		m_fSpeed /= 2;
	}

	StudioFrameAdvance( );
	UpdateBeams( );

	if ( !m_pGoalEnt )
		m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );

	if ( m_pGoalEnt )
	{
		m_vGoalPos = m_pGoalEnt->pev->origin;
		UTIL_MakeAimVectors( m_pGoalEnt->pev->angles );
		Vector vecDir = m_vGoalPos - pev->origin;
		pev->velocity = vecDir.Normalize( ) * m_fSpeed;
	}

	float flDist = ( pev->origin - m_vGoalPos ).Length( );
	if ( m_pGoalEnt && flDist < 40 )
	{
		// on to the next corner
		m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( m_pGoalEnt->pev->target ) );
		if ( m_pGoalEnt )
		{
			m_vGoalPos = m_pGoalEnt->pev->origin;
			UTIL_MakeAimVectors( m_pGoalEnt->pev->angles );
			Vector vecDir = m_vGoalPos - pev->origin;
			pev->velocity = vecDir.Normalize( ) * m_fSpeed;
		}
	}

	if ( m_flNextRingTime < gpGlobals->time && !( pev->spawnflags & SF_TRAININGBOT_NORING ) )
	{
		PLAYBACK_EVENT_FULL( 0, ENT( pev ), m_usBeamRing, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );
		m_flNextRingTime = gpGlobals->time + 0.1;
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

int CDummyBot::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_BULLET );
}

void CDummyBot::Killed( entvars_t *pevAttacker, int iGib )
{
	// tell the training room
	pev->target = ALLOC_STRING( "bradybunch" );
	m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
	if ( m_pGoalEnt )
		m_pGoalEnt->Use( this, this, USE_ON, 0 );

	ClearBeams( );
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "drone/drone_idle.wav", 0, 0, SND_STOP, PITCH_NORM );

	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "drone/drone_flinch1.wav", 0.65, 0.5, 0, PITCH_NORM ); break;
	case 1:	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "drone/drone_flinch2.wav", 0.65, 0.5, 0, PITCH_NORM ); break;
	}

	if ( pev->health <= -30 && RANDOM_LONG( 0, 6 ) )
	{
		BotExplode( );
		CBaseMonster::Killed( pevAttacker, GIB_ALWAYS );
		return;
	}

	// fall down smoking, and blow up on landing
	pev->health = 1;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->avelocity.x = RANDOM_FLOAT( -150, 150 );
	pev->avelocity.y = RANDOM_FLOAT( -150, 150 );
	pev->avelocity.z = RANDOM_FLOAT( -150, 150 );
	SetThink( &CDummyBot::SmokeFallThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CDummyBot::BotSparkThink( void )
{
	SetTouch( &CDummyBot::BotExplodeTouch );

	if ( m_pSprite )
		m_pSprite->pev->scale += 0.1;

	// a spark to each corner around it
	UTIL_MakeVectors( pev->angles );

	Vector vecSpark = pev->origin + gpGlobals->v_forward * RANDOM_FLOAT( -30, 30 ) + gpGlobals->v_up * RANDOM_FLOAT( 12, 27 ) + gpGlobals->v_right * RANDOM_FLOAT( -27, -12 );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMPOINTS );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( vecSpark.x );
		WRITE_COORD( vecSpark.y );
		WRITE_COORD( vecSpark.z );
		WRITE_SHORT( m_iBeam );
		WRITE_BYTE( 0 );	// frame start
		WRITE_BYTE( 0 );	// framerate
		WRITE_BYTE( 1 );	// life
		WRITE_BYTE( 12 );	// width
		WRITE_BYTE( 50 );	// noise
		WRITE_BYTE( 80 );	// r
		WRITE_BYTE( 255 );	// g
		WRITE_BYTE( 120 );	// b
		WRITE_BYTE( 100 );	// brightness
		WRITE_BYTE( 5 );	// speed
	MESSAGE_END();

	vecSpark = pev->origin + gpGlobals->v_forward * RANDOM_FLOAT( -30, 30 ) + gpGlobals->v_up * RANDOM_FLOAT( 12, 27 ) + gpGlobals->v_right * RANDOM_FLOAT( 12, 27 );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMPOINTS );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( vecSpark.x );
		WRITE_COORD( vecSpark.y );
		WRITE_COORD( vecSpark.z );
		WRITE_SHORT( m_iBeam );
		WRITE_BYTE( 0 );	// frame start
		WRITE_BYTE( 0 );	// framerate
		WRITE_BYTE( 1 );	// life
		WRITE_BYTE( 12 );	// width
		WRITE_BYTE( 50 );	// noise
		WRITE_BYTE( 80 );	// r
		WRITE_BYTE( 255 );	// g
		WRITE_BYTE( 120 );	// b
		WRITE_BYTE( 100 );	// brightness
		WRITE_BYTE( 5 );	// speed
	MESSAGE_END();

	vecSpark = pev->origin + gpGlobals->v_forward * RANDOM_FLOAT( -30, 30 ) + gpGlobals->v_up * RANDOM_FLOAT( -27, -12 ) + gpGlobals->v_right * RANDOM_FLOAT( 12, 27 );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMPOINTS );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( vecSpark.x );
		WRITE_COORD( vecSpark.y );
		WRITE_COORD( vecSpark.z );
		WRITE_SHORT( m_iBeam );
		WRITE_BYTE( 0 );	// frame start
		WRITE_BYTE( 0 );	// framerate
		WRITE_BYTE( 1 );	// life
		WRITE_BYTE( 12 );	// width
		WRITE_BYTE( 50 );	// noise
		WRITE_BYTE( 80 );	// r
		WRITE_BYTE( 255 );	// g
		WRITE_BYTE( 120 );	// b
		WRITE_BYTE( 100 );	// brightness
		WRITE_BYTE( 5 );	// speed
	MESSAGE_END();

	vecSpark = pev->origin + gpGlobals->v_forward * RANDOM_FLOAT( -30, 30 ) + gpGlobals->v_up * RANDOM_FLOAT( -27, -12 ) + gpGlobals->v_right * RANDOM_FLOAT( -27, -12 );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMPOINTS );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( vecSpark.x );
		WRITE_COORD( vecSpark.y );
		WRITE_COORD( vecSpark.z );
		WRITE_SHORT( m_iBeam );
		WRITE_BYTE( 0 );	// frame start
		WRITE_BYTE( 0 );	// framerate
		WRITE_BYTE( 1 );	// life
		WRITE_BYTE( 12 );	// width
		WRITE_BYTE( 50 );	// noise
		WRITE_BYTE( 80 );	// r
		WRITE_BYTE( 255 );	// g
		WRITE_BYTE( 120 );	// b
		WRITE_BYTE( 100 );	// brightness
		WRITE_BYTE( 5 );	// speed
	MESSAGE_END();

	pev->nextthink = gpGlobals->time + 0.1;
}

void CDummyBot::SmokeFallThink( void )
{
	SetTouch( &CDummyBot::BotExplodeTouch );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE_SPRAY );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( 0 );	// direction
		WRITE_COORD( 0 );
		WRITE_COORD( 1 );
		WRITE_SHORT( m_iSmoke );
		WRITE_BYTE( 1 );	// count
		WRITE_BYTE( 100 );	// speed
		WRITE_BYTE( 20 );	// noise
	MESSAGE_END();

	pev->nextthink = gpGlobals->time + 0.1;
}

void CDummyBot::BotExplodeTouch( CBaseEntity *pOther )
{
	BotExplode( );

	CBaseEntity *pOwner = CBaseEntity::Instance( pev->owner );
	if ( pOwner )
		pOwner->DeathNotice( pev );

	UTIL_Remove( this );
}

void CDummyBot::BotExplode( void )
{
	Vector vecSrc = pev->origin;

	CSphereExplosion *pBig = CSphereExplosion::CreateSphereExplosion( vecSrc, this, 100, 2, 255, 7, 1, TRUE, FALSE, 0, 0, 0 );
	CSphereExplosion *pSmall = CSphereExplosion::CreateSphereExplosion( vecSrc, this, 25, 0.5, 220, 3, 1, FALSE, FALSE, 0, 0, 0 );
	if ( pBig )
		pBig->UseToggle( this, this, USE_ON, 0 );
	if ( pSmall )
		pSmall->UseToggle( this, this, USE_ON, 0 );

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( m_iExplosion );
		WRITE_BYTE( 12 );	// scale * 10
		WRITE_BYTE( 15 );	// framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	SpawnGib( "models/battgib.mdl", 1.0, 0, 0 );
	SpawnGib( "models/battgib.mdl", 1.0, 1, 0 );
	SpawnGib( "models/battgib.mdl", 1.0, 2, 0 );
	SpawnGib( "models/battgib.mdl", 1.0, 3, 0 );
}

void CDummyBot::CreateBeams( void )
{
	ClearBeams( );

	AttackBeam1 = CBeam::BeamCreate( "sprites/xbeam1.spr", 40 );
	AttackBeam2 = CBeam::BeamCreate( "sprites/xbeam1.spr", 40 );
	AttackBeam3 = CBeam::BeamCreate( "sprites/xbeam1.spr", 40 );

	AttackBeam1->EntsInit( entindex( ), entindex( ) );
	AttackBeam1->SetFlags( BEAM_FSINE );
	AttackBeam1->SetFlags( BEAM_FSHADEOUT );	// sic: replaces BEAM_FSINE
	AttackBeam1->SetScrollRate( 5 );
	AttackBeam1->SetNoise( 50 );
	AttackBeam1->SetBrightness( 255 );
	AttackBeam1->SetWidth( 25 );
	AttackBeam1->SetColor( 80, 120, 255 );
	AttackBeam1->SetEndAttachment( 1 );
	AttackBeam1->SetStartAttachment( 2 );
	AttackBeam1->pev->spawnflags |= SF_BEAM_TEMPORARY;

	AttackBeam2->EntsInit( entindex( ), entindex( ) );
	AttackBeam2->SetFlags( BEAM_FSINE );
	AttackBeam2->SetFlags( BEAM_FSHADEOUT );	// sic
	AttackBeam2->SetScrollRate( 5 );
	AttackBeam2->SetNoise( 50 );
	AttackBeam2->SetBrightness( 255 );
	AttackBeam2->SetWidth( 25 );
	AttackBeam2->SetColor( 80, 120, 255 );
	AttackBeam2->SetEndAttachment( 1 );
	AttackBeam2->SetStartAttachment( 3 );
	AttackBeam2->pev->spawnflags |= SF_BEAM_TEMPORARY;

	AttackBeam3->EntsInit( entindex( ), entindex( ) );
	AttackBeam3->SetFlags( BEAM_FSINE );
	AttackBeam3->SetFlags( BEAM_FSHADEOUT );	// sic
	AttackBeam3->SetScrollRate( 5 );
	AttackBeam3->SetNoise( 50 );
	AttackBeam3->SetBrightness( 255 );
	AttackBeam3->SetWidth( 25 );
	AttackBeam3->SetColor( 80, 120, 255 );
	AttackBeam3->SetEndAttachment( 1 );
	AttackBeam3->SetStartAttachment( 4 );
	AttackBeam3->pev->spawnflags |= SF_BEAM_TEMPORARY;

	m_pSprite = CSprite::SpriteCreate( "sprites/hugeball.spr", pev->origin, TRUE );
	m_pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
	m_pSprite->SetAttachment( edict( ), 1 );
	m_pSprite->SetScale( 0.2 );
	m_pSprite->pev->framerate = 10;
	m_pSprite->pev->frame = 0;
	m_pSprite->TurnOn( );
	UTIL_SetOrigin( m_pSprite->pev, m_pSprite->pev->origin );
	m_pSprite->pev->spawnflags |= SF_SPRITE_TEMPORARY;
}

void CDummyBot::UpdateBeams( void )
{
	if ( AttackBeam1 )
	{
		AttackBeam1->RelinkBeam( );
		if ( AttackBeam2 )
			AttackBeam2->RelinkBeam( );
		if ( AttackBeam3 )
			AttackBeam3->RelinkBeam( );
	}
	else
	{
		// the beams went away (a restore): make them again
		CreateBeams( );
	}
}

void CDummyBot::ClearBeams( void )
{
	if ( AttackBeam1 )
	{
		UTIL_Remove( AttackBeam1 );
		AttackBeam1 = NULL;
	}
	if ( AttackBeam2 )
	{
		UTIL_Remove( AttackBeam2 );
		AttackBeam2 = NULL;
	}
	if ( AttackBeam3 )
	{
		UTIL_Remove( AttackBeam3 );
		AttackBeam3 = NULL;
	}
	if ( m_pSprite )
	{
		UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}
}
