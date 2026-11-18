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
// decore_explodable: a model that blows up when it is used,
// with sphere explosions, fireballs, metal debris and
// burning gibs. gib_dropship is the big gib it throws,
// which explodes again when it lands.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "sphereexplosion.h"

//=========================================================
// gib_dropship - a gib with two flames on it that blows up
// at the first touch
//=========================================================
class CDropshipGib : public CGib
{
public:
	void Spawn( const char *szGibModel );
	void Precache( void );
	void EXPORT GibExplodeTouch( CBaseEntity *pOther );

	CSprite *m_pFlame1;
	CSprite *m_pFlame2;
	int m_iPlateGibs;
	int m_iFireball;
};
LINK_ENTITY_TO_CLASS( gib_dropship, CDropshipGib );

void CDropshipGib::Spawn( const char *szGibModel )
{
	CGib::Spawn( szGibModel );
	Precache( );

	m_pFlame1 = CSprite::SpriteCreate( "sprites/flame.spr", pev->origin, TRUE );
	m_pFlame1->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
	m_pFlame1->pev->spawnflags |= SF_SPRITE_TEMPORARY;
	m_pFlame1->SetAttachment( edict(), 1 );
	m_pFlame1->SetScale( 2.0 );
	m_pFlame1->pev->framerate = 15;
	m_pFlame1->pev->frame = 0;
	m_pFlame1->TurnOn( );
	m_pFlame1->pev->spawnflags |= SF_SPRITE_TEMPORARY;

	m_pFlame2 = CSprite::SpriteCreate( "sprites/flame.spr", pev->origin, TRUE );
	m_pFlame2->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
	m_pFlame2->pev->spawnflags |= SF_SPRITE_TEMPORARY;
	m_pFlame2->SetAttachment( edict(), 2 );
	m_pFlame2->SetScale( 2.0 );
	m_pFlame2->pev->framerate = 15;
	m_pFlame2->pev->frame = 0;
	m_pFlame2->TurnOn( );
	m_pFlame2->pev->spawnflags |= SF_SPRITE_TEMPORARY;

	SetTouch( &CDropshipGib::GibExplodeTouch );
}

void CDropshipGib::GibExplodeTouch( CBaseEntity *pOther )
{
	Vector vecSpot = pev->origin;
	vecSpot.z += 170;

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( RandomExplosionSprite() );
		WRITE_BYTE( 40 ); // scale * 10
		WRITE_BYTE( 15 ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );
		WRITE_SHORT( m_iFireball );
		WRITE_BYTE( 80 ); // scale * 10
		WRITE_BYTE( 15 ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	Vector vecCenter = pev->origin + ( pev->mins + pev->maxs ) * 0.5;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecCenter );
		WRITE_BYTE( TE_BREAKMODEL );

		// position
		WRITE_COORD( vecCenter.x );
		WRITE_COORD( vecCenter.y );
		WRITE_COORD( vecCenter.z + 64 );

		// size
		WRITE_COORD( 400 );
		WRITE_COORD( 400 );
		WRITE_COORD( 128 );

		// velocity
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_COORD( 50 );

		// randomization
		WRITE_BYTE( 30 );

		// Model
		WRITE_SHORT( m_iPlateGibs );	//model id#

		// # of shards
		WRITE_BYTE( 6 );

		// duration
		WRITE_BYTE( 100 );// 10.0 seconds

		// flags
		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

	if ( m_pFlame1 )
	{
		UTIL_Remove( m_pFlame1 );
		m_pFlame1 = NULL;
	}
	if ( m_pFlame2 )
	{
		UTIL_Remove( m_pFlame2 );
		m_pFlame2 = NULL;
	}

	UTIL_Remove( this );
}

void CDropshipGib::Precache( void )
{
	PRECACHE_MODEL( "sprites/flame.spr" );
	m_iPlateGibs = PRECACHE_MODEL( "models/plategibs.mdl" );
	m_iFireball = PRECACHE_MODEL( "sprites/fexplo.spr" );
}


//=========================================================
// decore_explodable
//=========================================================
class CDecoreExplode : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	int  Classify( void );
	int  ISoundMask( void );

	void EXPORT ActionAnimUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT ExplodeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	CBaseMonster *ThrowGib( const char *szGibModel, float flVelScale, int iBody, int iType );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t m_iszModel;
	string_t m_iszGibModel;
	string_t m_iszIdleAnim;
	int m_iFireTrail;		// sprites/firebeam.spr
	int m_iPlateGibs;		// models/plategibs.mdl
	int m_iWhiteTrail;		// sprites/white2.spr
	string_t m_iszActionAnim;	// played at the first use; the second blows it up
};
LINK_ENTITY_TO_CLASS( decore_explodable, CDecoreExplode );

TYPEDESCRIPTION	CDecoreExplode::m_SaveData[] =
{
	DEFINE_FIELD( CDecoreExplode, m_iszModel, FIELD_STRING ),
	DEFINE_FIELD( CDecoreExplode, m_iszGibModel, FIELD_STRING ),
	DEFINE_FIELD( CDecoreExplode, m_iszIdleAnim, FIELD_STRING ),
	DEFINE_FIELD( CDecoreExplode, m_iszActionAnim, FIELD_STRING ),
};
IMPLEMENT_SAVERESTORE( CDecoreExplode, CBaseEntity );	// sic: skips CBaseMonster's fields


void CDecoreExplode::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT(pev), STRING(m_iszModel) );
	UTIL_SetSize( pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_NONE;
	m_bloodColor = DONT_BLEED;
	pev->health = 8;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_FLY;
	UTIL_SetOrigin( pev, pev->origin );

	if ( m_iszIdleAnim )
	{
		SetSequenceByName( (char *)STRING( m_iszIdleAnim ) );
		ResetSequenceInfo( );
	}

	SetUse( &CDecoreExplode::ExplodeUse );
	if ( m_iszActionAnim && strcmp( "none", STRING( m_iszActionAnim ) ) )
		SetUse( &CDecoreExplode::ActionAnimUse );
}

void CDecoreExplode::Precache( void )
{
	if ( m_iszModel )
		PRECACHE_MODEL( (char *)STRING( m_iszModel ) );
	if ( m_iszGibModel )
		PRECACHE_MODEL( (char *)STRING( m_iszGibModel ) );

	m_iFireTrail = PRECACHE_MODEL( "sprites/firebeam.spr" );
	m_iWhiteTrail = PRECACHE_MODEL( "sprites/white2.spr" );
	m_iPlateGibs = PRECACHE_MODEL( "models/plategibs.mdl" );
	PRECACHE_MODEL( "models/dropshipgib.mdl" );
	UTIL_PrecacheOther( "sphere_explosion" );
	UTIL_PrecacheOther( "gib_dropship" );
}

void CDecoreExplode::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "decoremodel" ) )
	{
		m_iszModel = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "decoregib" ) )
	{
		m_iszGibModel = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "decoreidle" ) )
	{
		m_iszIdleAnim = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "decoreaction" ) )
	{
		m_iszActionAnim = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		pkvd->fHandled = FALSE;	// sic: unhandles the three keys above, and never passes keys on to CBaseMonster
}

int CDecoreExplode::Classify( void )
{
	return CLASS_NONE;
}

int CDecoreExplode::ISoundMask( void )
{
	return NULL;
}

void CDecoreExplode::ActionAnimUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_iszActionAnim && strcmp( "none", STRING( m_iszActionAnim ) ) )
	{
		SetSequenceByName( (char *)STRING( m_iszActionAnim ) );
		ResetSequenceInfo( );
	}

	SetUse( &CDecoreExplode::ExplodeUse );
}

void CDecoreExplode::ExplodeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecSpot = pev->origin;
	vecSpot.z += 30;

	CSphereExplosion *pSphere = CSphereExplosion::CreateSphereExplosion( vecSpot, this, 450, 8, 255, 5, 1, TRUE, FALSE, 2.5, 1.0, 0.1 );
	CSphereExplosion *pSphere2 = CSphereExplosion::CreateSphereExplosion( vecSpot, this, 120, 2, 220, 5, 1, FALSE, FALSE, 0, 0, 0 );

	if ( pSphere )
		pSphere->UseToggle( this, this, USE_ON, 0 );
	if ( pSphere2 )
		pSphere2->UseToggle( this, this, USE_ON, 0 );

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( RandomExplosionSprite() );
		WRITE_BYTE( 40 ); // scale * 10
		WRITE_BYTE( 15 ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );
		WRITE_SHORT( RandomExplosionSprite() );
		WRITE_BYTE( 60 ); // scale * 10
		WRITE_BYTE( 10 ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	Vector vecCenter = pev->origin + ( pev->mins + pev->maxs ) * 0.5;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecCenter );
		WRITE_BYTE( TE_BREAKMODEL );

		// position
		WRITE_COORD( vecCenter.x );
		WRITE_COORD( vecCenter.y );
		WRITE_COORD( vecCenter.z + 64 );

		// size
		WRITE_COORD( 400 );
		WRITE_COORD( 400 );
		WRITE_COORD( 128 );

		// velocity
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_COORD( 200 );

		// randomization
		WRITE_BYTE( 30 );

		// Model
		WRITE_SHORT( m_iPlateGibs );	//model id#

		// # of shards
		WRITE_BYTE( 18 );

		// duration
		WRITE_BYTE( 200 );// 20.0 seconds

		// flags
		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

	// sic: without a gib model it explodes again at every use and is never removed
	if ( m_iszGibModel )
	{
		// sic: nothing calls UTIL_MakeVectors, so v_right and v_up are whatever was last made
		CBaseMonster *pGib = ThrowGib( STRING( m_iszGibModel ), 4, 1, 1 );
		if ( pGib )
			UTIL_SetOrigin( pGib->pev, pev->origin + gpGlobals->v_right * 20 + gpGlobals->v_up * 50 );

		pGib = ThrowGib( STRING( m_iszGibModel ), 4, 2, 1 );
		if ( pGib )
			UTIL_SetOrigin( pGib->pev, pev->origin + gpGlobals->v_right * 20 );

		pGib = ThrowGib( "models/dropshipgib.mdl", 6, 0, 2 );
		if ( pGib )
		{
			UTIL_SetOrigin( pGib->pev, pev->origin + Vector( 0, 0, 50 ) );

			// fire and smoke trails from both its attachments
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_BEAMFOLLOW );
				WRITE_SHORT( pGib->entindex() | 0x1000 );	// entity, attachment 1
				WRITE_SHORT( m_iFireTrail );	// model
				WRITE_BYTE( 30 ); // life
				WRITE_BYTE( 25 );  // width
				WRITE_BYTE( 224 );   // r
				WRITE_BYTE( 224 );   // g
				WRITE_BYTE( 100 );   // b
				WRITE_BYTE( 255 );	// brightness
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_BEAMFOLLOW );
				WRITE_SHORT( pGib->entindex() | 0x1000 );	// entity, attachment 1
				WRITE_SHORT( m_iWhiteTrail );	// model
				WRITE_BYTE( 9 ); // life
				WRITE_BYTE( 3 );  // width
				WRITE_BYTE( 255 );   // r
				WRITE_BYTE( 255 );   // g
				WRITE_BYTE( 255 );   // b
				WRITE_BYTE( 150 );	// brightness
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_BEAMFOLLOW );
				WRITE_SHORT( pGib->entindex() | 0x2000 );	// entity, attachment 2
				WRITE_SHORT( m_iFireTrail );	// model
				WRITE_BYTE( 20 ); // life
				WRITE_BYTE( 25 );  // width
				WRITE_BYTE( 224 );   // r
				WRITE_BYTE( 224 );   // g
				WRITE_BYTE( 100 );   // b
				WRITE_BYTE( 255 );	// brightness
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_BEAMFOLLOW );
				WRITE_SHORT( pGib->entindex() | 0x2000 );	// entity, attachment 2
				WRITE_SHORT( m_iWhiteTrail );	// model
				WRITE_BYTE( 9 ); // life
				WRITE_BYTE( 3 );  // width
				WRITE_BYTE( 255 );   // r
				WRITE_BYTE( 255 );   // g
				WRITE_BYTE( 255 );   // b
				WRITE_BYTE( 150 );	// brightness
			MESSAGE_END();
		}

		UTIL_Remove( this );
	}
}

//=========================================================
// ThrowGib - CBaseMonster::SpawnGib for the explosion:
// iType 1 throws a smoking CGib, 2 a CDropshipGib, which
// goes up more slowly and never at the player
//=========================================================
CBaseMonster *CDecoreExplode::ThrowGib( const char *szGibModel, float flVelScale, int iBody, int iType )
{
	CGib *pGib;

	if ( iType == 1 )
		pGib = GetClassPtr( (CGib *)NULL );
	else
		pGib = GetClassPtr( (CDropshipGib *)NULL );

	pGib->Spawn( szGibModel );
	pGib->pev->body = iBody;
	pGib->pev->skin = pev->skin;

	if ( pev && pGib )
	{
		Vector vecOrigin = pev->origin + pev->view_ofs;
		vecOrigin = vecOrigin + Vector( RANDOM_FLOAT( -4, 4 ), RANDOM_FLOAT( -4, 4 ), RANDOM_FLOAT( -4, 4 ) );
		UTIL_SetOrigin( pGib->pev, vecOrigin );

		edict_t *pentPlayer = FIND_CLIENT_IN_PVS( pGib->edict() );

		if ( iType == 2 )
		{
			pGib->pev->velocity = Vector( RANDOM_FLOAT( -50, 50 ), RANDOM_FLOAT( -50, 50 ), RANDOM_FLOAT( 200, 300 ) );
		}
		else if ( UTIL_PercentChance( 5 ) && pentPlayer )
		{
			// 5% chance the gib will be thrown at player's face.
			entvars_t *pevPlayer = VARS( pentPlayer );

			pGib->pev->velocity = ( ( pevPlayer->origin + pevPlayer->view_ofs ) - pGib->pev->origin ).Normalize() * 300;
			pGib->pev->velocity.z += 100;
		}
		else
		{
			pGib->pev->velocity = Vector( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 ) );
		}

		pGib->pev->avelocity.x = RANDOM_FLOAT( 100, 200 );
		pGib->pev->avelocity.y = RANDOM_FLOAT( 100, 300 );
		pGib->pev->gravity = 1.0;
		pGib->m_bloodColor = DONT_BLEED;

		pGib->pev->velocity = pGib->pev->velocity * flVelScale;

		pGib->LimitVelocity();

		if ( iType == 1 )
			pGib->StartSmoke( iType );
	}

	return pGib->MyMonsterPointer();
}
