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
// tuberocket: the little critter the tubes, the tube queen's
// mortars and the sitting tube mortars fire. It flies like a
// hornet: it homes in on its enemy while its turning fuel
// lasts, speeds up toward 1200 units a second and splats on
// what it hits.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "effects.h"

int iTubeRocketTrail;

class CTubeRocket : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	void UpdateTarget( void );

	void EXPORT StartTrack( void );
	void EXPORT TrackTarget( void );
	void EXPORT TrackTouch( CBaseEntity *pOther );
	void EXPORT DieTouch( CBaseEntity *pOther );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	float	m_fStopAttack;
	float	m_fFlySpeed;
	float	m_fFlySpeedAdd;	// how fast it speeds up to 1200
	float	m_TurnFuel;		// each turn burns some; it flies straight when it is out
};

LINK_ENTITY_TO_CLASS( tuberocket, CTubeRocket );

TYPEDESCRIPTION	CTubeRocket::m_SaveData[] =
{
	DEFINE_FIELD( CTubeRocket, m_fStopAttack, FIELD_TIME ),
	DEFINE_FIELD( CTubeRocket, m_fFlySpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CTubeRocket, m_fFlySpeedAdd, FIELD_FLOAT ),
	DEFINE_FIELD( CTubeRocket, m_TurnFuel, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CTubeRocket, CBaseMonster );

int CTubeRocket::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_NEVERGIB );
}

void CTubeRocket::Spawn( void )
{
	Precache( );

	pev->movetype	= MOVETYPE_FLY;
	pev->solid		= SOLID_BBOX;
	pev->flags		|= FL_MONSTER;
	pev->health		= 1;
	pev->takedamage	= DAMAGE_NO;
	pev->dmg		= RANDOM_LONG( 0, 3 ) + 8;

	m_fStopAttack	= gpGlobals->time + 5.0;
	m_flFieldOfView	= -1;
	m_fFlySpeed		= 500;
	m_fFlySpeedAdd	= RANDOM_FLOAT( 0.01, 0.1 );

	switch ( g_iSkillLevel )
	{
	case SKILL_MEDIUM:
		m_TurnFuel = RANDOM_FLOAT( 0.7, 1.4 );
		break;
	case SKILL_HARD:
		m_TurnFuel = RANDOM_FLOAT( 1.4, 2.0 );
		break;
	default:
		m_TurnFuel = RANDOM_FLOAT( 0.1, 1.2 );
		break;
	}

	m_TurnFuel = m_TurnFuel * m_TurnFuel * m_TurnFuel + 0.2;

	SET_MODEL( ENT(pev), "models/tubeprojectile.mdl" );
	UTIL_SetSize( pev, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ) );

	// trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex( ) );	// entity
		WRITE_SHORT( iTubeRocketTrail );	// model
		WRITE_BYTE( 6 );	// life
		WRITE_BYTE( 3 );	// width
		WRITE_BYTE( 32 );	// r, g, b
		WRITE_BYTE( 128 );	// r, g, b
		WRITE_BYTE( 16 );	// r, g, b
		WRITE_BYTE( 128 );	// brightness
	MESSAGE_END( );

	SetTouch( &CTubeRocket::TrackTouch );
	SetThink( &CTubeRocket::StartTrack );

	pev->nextthink = gpGlobals->time + 0.1;
	ResetSequenceInfo( );
}

void CTubeRocket::Precache( void )
{
	PRECACHE_MODEL( "models/tubeprojectile.mdl" );
	PRECACHE_MODEL( "sprites/gsplat.spr" );
	iTubeRocketTrail = PRECACHE_MODEL( "sprites/laserbeam.spr" );

	PRECACHE_SOUND( "tube/tubecritter_buzz1.wav" );
	PRECACHE_SOUND( "tube/tubecritter_buzz2.wav" );
	PRECACHE_SOUND( "tube/tubecritter_buzz3.wav" );

	PRECACHE_SOUND( "tube/tubecritter_hit1.wav" );
	PRECACHE_SOUND( "tube/tubecritter_hit2.wav" );
	PRECACHE_SOUND( "tube/tubecritter_hit3.wav" );
}

int CTubeRocket::Classify( void )
{
	return CLASS_ALIEN_BIOWEAPON;
}

//=========================================================
// StartTrack - forgets an enemy behind it, then tracks
//=========================================================
void CTubeRocket::StartTrack( void )
{
	if ( m_hEnemy )
	{
		Vector vecDirToEnemy = ( m_hEnemy->Center( ) - pev->origin ).Normalize( );
		Vector vecFlightDir = pev->velocity.Normalize( );

		if ( DotProduct( vecFlightDir, vecDirToEnemy ) < 0 )
			m_hEnemy = NULL;
	}

	SetThink( &CTubeRocket::TrackTarget );
	TrackTarget( );
}

//=========================================================
// UpdateTarget - finds an enemy if it has none and aims a
// little above it; without one in sight it flies on
//=========================================================
void CTubeRocket::UpdateTarget( void )
{
	if ( m_hEnemy == NULL )
	{
		Look( 1024 );
		m_hEnemy = BestVisibleEnemy( );
	}

	if ( m_hEnemy != NULL && FVisible( m_hEnemy ) )
	{
		Vector vecTarget = m_hEnemy->BodyTarget( pev->origin );
		int iHeight;

		switch ( g_iSkillLevel )
		{
		case SKILL_MEDIUM:
			iHeight = 6;
			break;
		case SKILL_HARD:
			iHeight = 0;
			break;
		default:
			iHeight = 12;
			break;
		}

		// sic: both ends are the body target, so the offset is
		// straight up
		m_vecEnemyLKP = vecTarget + ( vecTarget - m_hEnemy->BodyTarget( pev->origin ) ).Normalize( ) * iHeight;
	}
	else
	{
		m_vecEnemyLKP = m_vecEnemyLKP + pev->velocity * m_fFlySpeed * 0.1;
	}
}

//=========================================================
// TrackTarget - the rocket is flying, tracking its target
//=========================================================
void CTubeRocket::TrackTarget( void )
{
	StudioFrameAdvance( );

	pev->movetype = MOVETYPE_FLY;

	if ( pev->velocity.Length( ) < 20 || gpGlobals->time > m_fStopAttack ||
		pev->origin.x < -4096 || pev->origin.x > 4096 ||
		pev->origin.y < -4096 || pev->origin.y > 4096 ||
		pev->origin.z < -4096 || pev->origin.z > 4096 )
	{
		UTIL_Remove( this );
		return;
	}

	switch ( g_iSkillLevel )
	{
	case SKILL_MEDIUM:
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1, 0.13 );
		break;
	case SKILL_HARD:
		pev->nextthink = gpGlobals->time + 0.1;
		break;
	default:
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1, 0.18 );
		break;
	}

	m_fFlySpeed += ( 1200 - m_fFlySpeed ) * m_fFlySpeedAdd;
	pev->angles = UTIL_VecToAngles( pev->velocity );

	UpdateTarget( );

	if ( m_TurnFuel > 0 && m_hEnemy != NULL )
	{
		Vector	vecDirToEnemy;
		Vector	vecFlightDir;
		float	flDelta;

		vecDirToEnemy = ( m_vecEnemyLKP - pev->origin ).Normalize( );

		if ( pev->velocity.Length( ) < 0.1 )
			vecFlightDir = vecDirToEnemy;
		else
			vecFlightDir = pev->velocity.Normalize( );

		// measure how far the turn is, the wider the turn, the slow we'll go this time.
		flDelta = vecFlightDir.x * vecDirToEnemy.x + vecFlightDir.y * vecDirToEnemy.y + vecFlightDir.z * vecDirToEnemy.z;

		if ( flDelta < 0.5 )
		{
			// hafta turn wide again. play sound
			switch ( RANDOM_LONG( 0, 2 ) )
			{
			case 0:
				EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "tube/tubecritter_buzz1.wav", 0.5, 0.8, 0, 100 );
				break;
			case 1:
				EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "tube/tubecritter_buzz2.wav", 0.5, 0.8, 0, 100 );
				break;
			case 2:
				EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "tube/tubecritter_buzz3.wav", 0.5, 0.8, 0, 100 );
				break;
			}
		}

		if ( flDelta <= 0 )
			flDelta = 0.25;

		m_TurnFuel += flDelta - 1;

		flDelta *= 1.1;
		pev->velocity = vecFlightDir + vecDirToEnemy * flDelta;
		Vector vecVelocity = pev->velocity;	// sic: never used
		pev->velocity = pev->velocity.Normalize( );
		pev->velocity = pev->velocity * ( ( flDelta / 2 ) * m_fFlySpeed );
	}

	float flSpeed = pev->velocity.Length( );

	if ( flSpeed >= 1600 )
	{
		pev->velocity = pev->velocity.Normalize( );
		pev->velocity = pev->velocity * 1600;
	}
	else if ( flSpeed < 300 )
	{
		pev->velocity = pev->velocity.Normalize( );
		pev->velocity = pev->velocity * 300;
	}
}
//=========================================================
// TrackTouch - bites what it hits, unless it is another
// bioweapon
//=========================================================
void CTubeRocket::TrackTouch( CBaseEntity *pOther )
{
	if ( pOther->Classify( ) != Classify( ) )
	{
		if ( IRelationship( pOther ) > R_NO && pOther->pev->takedamage != DAMAGE_NO )
			pOther->TakeDamage( pev, pev, pev->dmg, DMG_SLASH );

		DieTouch( pOther );
	}
}

//=========================================================
// DieTouch - splats
//=========================================================
void CTubeRocket::DieTouch( CBaseEntity *pOther )
{
	pev->modelindex = 0;
	pev->solid = SOLID_NOT;

	CSprite *pSprite = CSprite::SpriteCreate( "sprites/gsplat.spr", pev->origin, TRUE );
	pSprite->AnimateAndDie( 10 );
	pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
	pSprite->pev->spawnflags |= SF_SPRITE_TEMPORARY;
	pSprite->pev->frame = 0;
	pSprite->pev->framerate = 10;
	UTIL_SetOrigin( pSprite->pev, pev->origin );

	if ( pOther && pOther->IsPlayer( ) )
	{
		EMIT_SOUND_DYN( pOther->edict( ), CHAN_VOICE, "tube/tubecritter_hit3.wav", 1, 0.8, 0, 130 );
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "common/null.wav", 0, 0.8, 0, 100 );
	}
	else
	{
		switch ( RANDOM_LONG( 0, 1 ) )
		{
		case 0:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "tube/tubecritter_hit1.wav", 0.7, 0.8, 0, 100 );
			break;
		case 1:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "tube/tubecritter_hit2.wav", 0.7, 0.8, 0, 100 );
			break;
		}
	}

	pev->health = 0;
	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.2;
}
