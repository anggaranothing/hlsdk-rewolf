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
// the DML's rocket: the hornet's homing, the RPG rocket's
// laser guidance, a spiralling pair of brothers, and the
// grenade's timer, cluster and tripmine payloads
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "effects.h"
#include "gamerules.h"
#include "soundent.h"
#include "decals.h"
#include "dmlrocket.h"

int iDmlRocketPuff;

//=========================================================
// RocketDecal - a crater on metal, a scorch mark elsewhere
//=========================================================
void RocketDecal( TraceResult *pTrace, Vector vecSrc, Vector vecEnd )
{
	char chTextureType;
	char szbuffer[64];
	const char *pTextureName;
	float rgfl1[3];
	float rgfl2[3];

	if ( !pTrace )
		return;

	CBaseEntity *pEntity = CBaseEntity::Instance(pTrace->pHit);

	if (pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE)
		;
	else
	{
		// hit world

		// find texture under strike, get material type

		// copy trace vector into array for trace_texture

		vecSrc.CopyToArray(rgfl1);
		vecEnd.CopyToArray(rgfl2);

		// get texture from entity or world (world is ent(0))
		if (pEntity)
			pTextureName = TRACE_TEXTURE( ENT(pEntity->pev), rgfl1, rgfl2 );
		else
			pTextureName = TRACE_TEXTURE( ENT(0), rgfl1, rgfl2 );

		if ( pTextureName )
		{
			// strip leading '-0' or '+0~' or '{' or '!'
			if (*pTextureName == '-' || *pTextureName == '+')
				pTextureName += 2;

			if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
				pTextureName++;
			// '}}'
			strcpy(szbuffer, pTextureName);
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;

			// get texture type
			chTextureType = TEXTURETYPE_Find(szbuffer);

			if ( chTextureType == CHAR_TEX_METAL )
			{
				UTIL_DecalTrace( pTrace, DECAL_CRATER3 );
				return;
			}
		}
	}

	if ( RANDOM_LONG( 0, 1 ) )
		UTIL_DecalTrace( pTrace, DECAL_SCORCH2 );
	else
		UTIL_DecalTrace( pTrace, DECAL_SCORCH1 );
}


LINK_ENTITY_TO_CLASS( dmlRocket, CDmlRocket );

TYPEDESCRIPTION	CDmlRocket::m_SaveData[] =
{
	DEFINE_FIELD( CDmlRocket, m_flStopAttack, FIELD_TIME ),
	DEFINE_FIELD( CDmlRocket, m_flFlySpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CDmlRocket, m_flNextBounceSound, FIELD_TIME ),
	DEFINE_FIELD( CDmlRocket, m_iHornetType, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlRocket, m_iFlightpath, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlRocket, m_iPayload, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlRocket, m_iDetonate, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlRocket, m_flStopHoming, FIELD_TIME ),
	DEFINE_FIELD( CDmlRocket, m_iIsBrother, FIELD_INTEGER ),
	DEFINE_FIELD( CDmlRocket, m_iSpiralScalar_In, FIELD_FLOAT ),
	DEFINE_FIELD( CDmlRocket, m_iSpiralScalar_Up, FIELD_FLOAT ),
	DEFINE_FIELD( CDmlRocket, m_flIgniteTime, FIELD_TIME ),
	DEFINE_FIELD( CDmlRocket, m_BrotherRocket, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDmlRocket, m_pSprite, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDmlRocket, m_flPowerUp, FIELD_TIME ),
	DEFINE_FIELD( CDmlRocket, m_vecDir, FIELD_VECTOR ),
	DEFINE_FIELD( CDmlRocket, m_vecEnd, FIELD_VECTOR ),
	DEFINE_FIELD( CDmlRocket, m_flBeamLength, FIELD_FLOAT ),
	DEFINE_FIELD( CDmlRocket, m_hOwner, FIELD_EHANDLE ),
	DEFINE_FIELD( CDmlRocket, m_pBeam, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDmlRocket, m_posOwner, FIELD_VECTOR ),
	DEFINE_FIELD( CDmlRocket, m_angleOwner, FIELD_VECTOR ),
	DEFINE_FIELD( CDmlRocket, m_bSpawnedTrip, FIELD_BOOLEAN ),
	DEFINE_FIELD( CDmlRocket, m_BrotherRocket, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDmlRocket, m_pRealOwner, FIELD_EDICT ),
};

IMPLEMENT_SAVERESTORE( CDmlRocket, CBaseMonster );

//=========================================================
// don't let rockets gib, ever.
//=========================================================
void CDmlRocket :: Spawn( void )
{
	Precache();

	pev->movetype	= MOVETYPE_FLY;
	pev->solid		= SOLID_BBOX;
	pev->takedamage = DAMAGE_YES;
	pev->spawnflags	= 0;
	pev->health		= 1;// weak!
	pev->gravity	= 0.5;
	pev->dmg		= gSkillData.plrDmgDmlRocket;

	m_flFlySpeed = 900;

	SET_MODEL(ENT( pev ), "models/dmlrocket.mdl");
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	if ( g_pGameRules->IsMultiplayer() )
	{
		// rockets don't live as long in multiplayer
		m_flStopAttack = gpGlobals->time + 3.5;
	}
	else
	{
		m_flStopAttack	= gpGlobals->time + 5.0;
	}

	m_flFieldOfView = 0.9; // +- 25 degrees

	// the trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 20 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 100 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iWhiteTrail );	// model
		WRITE_BYTE( 9 ); // life
		WRITE_BYTE( 1 );  // width
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 150 );	// brightness
	MESSAGE_END();

	SetTouch( &CDmlRocket::RocketTouch );

	m_flHomingFuel = 0;

	switch ( m_iFlightpath )
	{
	case 1:
		SetThink( &CDmlRocket::FollowThink );
		break;
	case 2:
		{
			SetThink( &CDmlRocket::TrackTarget );
			float flFuel = RANDOM_FLOAT( 0, 0.5 ) + 0.8;
			m_flHomingFuel = flFuel * flFuel * flFuel + 1;
		}
		break;
	default:
		SetThink( &CDmlRocket::RocketThink );
		break;
	}

	pev->nextthink = gpGlobals->time + 0.1;

	CreateFlame( );

	m_flStopHoming = gpGlobals->time + 3.0;
	m_flNextBounceSound = 0;
	m_vecDir = gpGlobals->v_forward;
}


void CDmlRocket :: Precache()
{
	PRECACHE_MODEL("models/dmlrocket.mdl");
	PRECACHE_MODEL("sprites/flame.spr");
	iDmlRocketPuff = PRECACHE_MODEL( "sprites/muz1.spr" );
	m_iTrail = PRECACHE_MODEL("sprites/firebeam.spr");
	m_sExplosion = PRECACHE_MODEL("sprites/part1.spr");
	m_sSmoke = PRECACHE_MODEL("sprites/part2.spr");
	PRECACHE_MODEL("sprites/dustyred.spr");
	m_iWhiteTrail = PRECACHE_MODEL("sprites/white2.spr");

	UTIL_PrecacheOther( "dml_cluster" );

	PRECACHE_SOUND("weapons/rocket1.wav");
	PRECACHE_SOUND("weapons/kabam1.wav");
	PRECACHE_SOUND("weapons/kabam2.wav");
	PRECACHE_SOUND("weapons/kabam3.wav");
	PRECACHE_SOUND("weapons/kaboom1.wav");
	PRECACHE_SOUND("weapons/kaboom2.wav");
	PRECACHE_SOUND("weapons/kaboom3.wav");
	PRECACHE_SOUND("weapons/dml_fragment.wav");
	PRECACHE_SOUND("weapons/grenade_hit1.wav");
	PRECACHE_SOUND("weapons/grenade_hit2.wav");
	PRECACHE_SOUND("weapons/grenade_hit3.wav");

	m_usRocketSmoke = PRECACHE_EVENT( 1, "events/RocketSmoke.sc" );
}


//=========================================================
// CreateSpiralRocket - one of the spiral's two brothers:
// placed before it spawns
//=========================================================
CDmlRocket *CDmlRocket::CreateSpiralRocket( const Vector &vecOrigin, const Vector &vecAngles, edict_t *pentOwner, int iFlightpath, int iPayload, int iDetonate )
{
	CDmlRocket *pRocket = GetClassPtr( (CDmlRocket *)NULL );

	pRocket->pev->classname = MAKE_STRING("dmlRocket");
	pRocket->pev->owner = pentOwner;
	pRocket->pev->origin = vecOrigin;
	pRocket->pev->angles = vecAngles;
	pRocket->m_iFlightpath = iFlightpath;
	pRocket->m_iPayload = iPayload;
	pRocket->m_iDetonate = iDetonate;
	pRocket->Spawn();

	return pRocket;
}

//=========================================================
// CreateDmlRocket - placed after it spawns
//=========================================================
CDmlRocket *CDmlRocket::CreateDmlRocket( const Vector &vecOrigin, const Vector &vecAngles, edict_t *pentOwner, int iFlightpath, int iPayload, int iDetonate )
{
	CDmlRocket *pRocket = GetClassPtr( (CDmlRocket *)NULL );

	pRocket->pev->classname = MAKE_STRING("dmlRocket");
	pRocket->m_iFlightpath = iFlightpath;
	pRocket->m_iPayload = iPayload;
	pRocket->m_iDetonate = iDetonate;
	pRocket->Spawn();
	pRocket->pev->owner = pentOwner;
	pRocket->pev->origin = vecOrigin;
	pRocket->pev->angles = vecAngles;

	return pRocket;
}

//=========================================================
// rockets will never get mad at each other, no matter who the owner is.
//=========================================================
int CDmlRocket::IRelationship ( CBaseEntity *pTarget )
{
	if ( pTarget->pev->modelindex == pev->modelindex )
	{
		return R_NO;
	}

	return CBaseMonster :: IRelationship( pTarget );
}

//=========================================================
// TrackTarget - the homing flightpath
//=========================================================
void CDmlRocket :: TrackTarget ( void )
{
	Vector	vecFlightDir;
	Vector	vecDirToEnemy;
	float	flDelta = 0;	// sic: never read, see vecVelocity

	if ( m_iDetonate == 2 )
	{
		// proximity: go off by anything worth it
		Look( 125 );
		CBaseEntity *pEnemy = BestVisibleEnemy( );
		CBaseEntity *pOwner = CBaseEntity::Instance( pev->owner );
		CBaseMonster *pMonster = NULL;

		if ( pOwner )
			pMonster = pOwner->MyMonsterPointer( );

		if ( pEnemy )
		{
			if ( !pMonster || ( ( pEnemy->pev->takedamage != DAMAGE_NO || pEnemy->IsAlive() ) && ( g_pGameRules->IsMultiplayer() || pMonster->IRelationship( pEnemy ) > R_NO ) ) )
			{
				Touch( NULL );
				pev->nextthink = gpGlobals->time;
				return;
			}
		}
	}

	if ( m_iDetonate == 3 && pev->dmgtime < gpGlobals->time )
	{
		Touch( NULL );
		return;
	}

	if ( g_pGameRules->IsMultiplayer() && m_flHomingFuel <= 0 )
	{
		// out of fuel: fly straight on
		SetThink( &CDmlRocket::RocketThink );
		pev->velocity = pev->velocity.Normalize() * 900;
		return;
	}

	if ( m_hEnemy == NULL || m_flStopHoming < gpGlobals->time )
	{
		SetThink( &CDmlRocket::RocketThink );
		return;
	}

	if ( m_hEnemy != NULL && FVisible( m_hEnemy ))
	{
		m_vecEnemyLKP = m_hEnemy->BodyTarget( pev->origin );
	}
	else
	{
		m_vecEnemyLKP = m_vecEnemyLKP + pev->velocity * m_flFlySpeed * 0.1;
	}

	vecDirToEnemy = ( m_vecEnemyLKP - pev->origin ).Normalize();

	if (pev->velocity.Length() < 0.1)
		vecFlightDir = vecDirToEnemy;
	else
		vecFlightDir = pev->velocity.Normalize();

	// measure how far the turn is, the wider the turn, the slow we'll go this time.
	flDelta = vecFlightDir.x * vecDirToEnemy.x + vecFlightDir.y * vecDirToEnemy.y + vecFlightDir.z * vecDirToEnemy.z;

	if ( flDelta <= 0 )
	{// no flying backwards, but we don't want to invert this, cause we'd go fast when we have to turn REAL far.
		flDelta = 0.25;
	}

	pev->velocity = ( vecFlightDir + vecDirToEnemy).Normalize();

	if ( pev->owner && (pev->owner->v.flags & FL_MONSTER) )
	{
		// random pattern only applies to rockets fired by monsters, not players.

		pev->velocity.x += RANDOM_FLOAT ( -0.10, 0.10 );// scramble the flight dir a bit.
		pev->velocity.y += RANDOM_FLOAT ( -0.10, 0.10 );
		pev->velocity.z += RANDOM_FLOAT ( -0.10, 0.10 );
	}

	m_flHomingFuel += flDelta - 1;

	pev->velocity = pev->velocity * ( m_flFlySpeed * flDelta );// scale the dir by the ( speed * width of turn )
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1, 0.3 );

	pev->angles = UTIL_VecToAngles (pev->velocity);
	Vector vecVelocity( pev->velocity );	// sic: never used; with flDelta's dead
						// initialisation it gives retail's inlining

	if ( pev->velocity.Length() > 2000 )
	{
		pev->velocity = pev->velocity.Normalize() * 1900;
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// Tracking Rocket hit something
//=========================================================
void CDmlRocket :: TrackTouch ( CBaseEntity *pOther )
{
	if ( UTIL_PointContents( pev->origin ) == CONTENTS_SKY )
	{
		pev->nextthink = gpGlobals->time;
		SetThink( &CBaseEntity::SUB_Remove );
		return;
	}

	if ( pOther->edict() != pev->owner && pOther->pev->modelindex != pev->modelindex )
	{
		RocketTouch( pOther );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

//=========================================================
// ShootClusterGrenades - the cluster payload
//=========================================================
void CDmlRocket::ShootClusterGrenades( CBaseEntity *pOther )
{
	TraceResult tr;

	if ( m_pSprite )
	{
		UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}

	if ( m_iDetonate == 3 && pev->dmgtime >= gpGlobals->time )
	{
		// timed: tumble until it's time
		SetTouch( &CDmlRocket::BounceSoundTouch );
		SetThink( &CDmlRocket::TumbleThink );
		pev->nextthink = gpGlobals->time + 0.1;
		pev->movetype = MOVETYPE_BOUNCE;
		pev->friction = 0.8;
		pev->gravity = 1.0;
		return;
	}

	if ( m_iDetonate == 3 )
	{
		pev->angles.x = RANDOM_FLOAT( -90, -70 );
		pev->angles.y = RANDOM_FLOAT( -90, 90 );
		pev->angles.z = 0;
	}

	UTIL_MakeVectors( pev->angles );
	m_vecDir = gpGlobals->v_forward;
	m_vecEnd = pev->origin + m_vecDir * 2048;

	UTIL_TraceLine( pev->origin, pev->origin + m_vecDir * 128, dont_ignore_monsters, pev->owner, &tr );

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
		if ( pHit && !( pHit->pev->flags & FL_CONVEYOR ) )
		{
			Vector angles( UTIL_VecToAngles( tr.vecPlaneNormal ) );
			UTIL_MakeVectors( angles );
			m_vecDir = gpGlobals->v_forward;
		}
	}

	for ( int i = 0; i < 4; i++ )
	{
		CClusterGrenade *pCluster = CClusterGrenade::CreateCluster( );
		pCluster->pev->owner = pev->owner;
		pCluster->pev->velocity = m_vecDir * RANDOM_LONG( 50, 250 ) + gpGlobals->v_right * RANDOM_LONG( -200, 200 );
		pCluster->pev->origin = pev->origin + m_vecDir * RANDOM_LONG( 5, 20 ) + gpGlobals->v_right * RANDOM_LONG( -15, 15 );
	}

	EMIT_SOUND( ENT(pev), CHAN_ITEM, "weapons/dml_fragment.wav", 1, ATTN_NORM );
	SetTouch( NULL );
	SetThink( NULL );
	UTIL_Remove( this );
}

void CDmlRocket::BounceSoundTouch( CBaseEntity *pOther )
{
	if ( !pOther )
		return;

	if ( !FClassnameIs( ENT( pOther->pev ), "worldspawn" ) )
		return;

	if ( m_flNextBounceSound < gpGlobals->time )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/grenade_hit1.wav", 1, ATTN_NORM);	break;
		case 1:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/grenade_hit2.wav", 1, ATTN_NORM);	break;
		case 2:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/grenade_hit3.wav", 1, ATTN_NORM);	break;
		}

		m_flNextBounceSound = gpGlobals->time + 0.5;
	}
}

void CDmlRocket::RocketTouch( CBaseEntity *pOther )
{
	if ( m_pSprite )
	{
		UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}

	if ( UTIL_PointContents( pev->origin ) == CONTENTS_SKY )
	{
		pev->nextthink = gpGlobals->time;
		SetThink( &CBaseEntity::SUB_Remove );
		return;
	}

	if ( m_iDetonate == 3 && pev->dmgtime >= gpGlobals->time )
	{
		// timed: bounce until it's time
		SetTouch( &CDmlRocket::BounceSoundTouch );
		SetThink( &CDmlRocket::TumbleThink );
		pev->nextthink = gpGlobals->time + 0.1;
		pev->movetype = MOVETYPE_BOUNCE;

		float flSpeed = pev->velocity.Length();
		pev->velocity = pev->velocity.Normalize() * flSpeed / 3.8;
		pev->gravity = 1.1;
		pev->avelocity.x = RANDOM_FLOAT( -700, 700 );
		pev->avelocity.y = RANDOM_FLOAT( -700, 700 );
		pev->avelocity.z = RANDOM_FLOAT( -700, 700 );
		return;
	}

	if ( m_iDetonate == 4 && pOther )
	{
		// tripmine: stick to the world
		if ( FClassnameIs( ENT( pOther->pev ), "worldspawn" ) )
		{
			SetTouch( NULL );

			if ( m_bSpawnedTrip )
				return;

			m_bSpawnedTrip = TRUE;
			BecomeTripmine( );
			return;
		}
	}

	TraceResult tr;
	Vector		vecSpot;// trace starts here!

	vecSpot = pev->origin - pev->velocity.Normalize() * 32;
	UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, ENT(pev), &tr );

	if ( m_iPayload != 2 )
		RocketDecal( &tr, vecSpot, vecSpot + pev->velocity.Normalize() * 64 );

	SetTouch( NULL );
	SetThink( &CDmlRocket::RemoveThink );
	pev->nextthink = gpGlobals->time + 5.0;
	pev->effects |= EF_NODRAW;

	ExplodeThink( );
}

//=========================================================
// RocketThink - straight on, or the spiral's two brothers
//=========================================================
void CDmlRocket::RocketThink( void )
{
	if ( m_pSprite && m_BrotherRocket )
	{
		UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}

	if ( m_hEnemy == NULL )
	{
		Look( 125 );
		m_hEnemy = BestVisibleEnemy( );
	}

	if ( m_hEnemy != NULL )
	{
		Touch( NULL );
		return;
	}

	if ( m_iDetonate == 3 && pev->dmgtime < gpGlobals->time )
	{
		Touch( NULL );
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;

	int iUp = m_iSpiralScalar_Up;

	if ( m_iIsBrother )
	{
		Vector vecForward, vecUp;

		if ( m_iIsBrother == 2 )
			iUp = -iUp;

		g_engfuncs.pfnAngleVectors( pev->angles, vecForward, NULL, vecUp );

		vecForward[0] *= 900;
		vecForward[1] *= 900;
		vecForward[2] *= -900;
		vecUp[0] *= iUp;
		vecUp[1] *= iUp;
		vecUp[2] *= -iUp;
		pev->velocity = vecForward + vecUp;

		if ( m_iIsBrother == 1 )
		{
			SetThink( &CDmlRocket::LittleThink );
			return;
		}

		if ( m_iIsBrother == 2 )
			SetThink( &CDmlRocket::BigThink );
	}
}

//=========================================================
// FollowThink - the guided flightpath: head for a laser spot
//=========================================================
void CDmlRocket :: FollowThink( void  )
{
	CBaseEntity *pOther = NULL;
	Vector vecTarget;
	Vector vecDir;
	float flDist, flMax, flDot;
	TraceResult tr;

	UTIL_MakeAimVectors( pev->angles );

	vecTarget = gpGlobals->v_forward;
	flMax = 4096;

	// Examine all entities within a reasonable radius
	while ((pOther = UTIL_FindEntityByClassname( pOther, "laser_spot" )) != NULL)
	{
		UTIL_TraceLine ( pev->origin, pOther->pev->origin, dont_ignore_monsters, ENT(pev), &tr );
		// ALERT( at_console, "%f\n", tr.flFraction );
		if (tr.flFraction >= 0.90)
		{
			vecDir = pOther->pev->origin - pev->origin;
			flDist = vecDir.Length( );
			vecDir = vecDir.Normalize( );
			flDot = DotProduct( gpGlobals->v_forward, vecDir );
			if ((flDot > 0) && (flDist * (1 - flDot) < flMax))
			{
				flMax = flDist * (1 - flDot);
				vecTarget = vecDir;
			}
		}
	}

	pev->angles = UTIL_VecToAngles( vecTarget );

	// this acceleration and turning math is totally wrong, but it seems to respond well so don't change it.
	float flSpeed = pev->velocity.Length();
	if (gpGlobals->time - m_flIgniteTime < 1.0)
	{
		pev->velocity = pev->velocity * 0.2 + vecTarget * (flSpeed * 0.8 + 400);
		if (pev->waterlevel == 3)
		{
			// go slow underwater
			if (pev->velocity.Length() > 300)
			{
				pev->velocity = pev->velocity.Normalize() * 300;
			}
			UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 4 );
		}
		else
		{
			if (pev->velocity.Length() > 2000)
			{
				pev->velocity = pev->velocity.Normalize() * 2000;
			}
		}
	}
	else
	{
		if (pev->effects & EF_LIGHT)
		{
			pev->effects = 0;
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0, 0, SND_STOP, PITCH_NORM );
		}
		pev->velocity = pev->velocity * 0.3 + vecTarget * flSpeed * 0.798;
	}

	switch ( m_iDetonate )
	{
	case 2:
		pev->spawnflags = 0;
		Look( 125 );
		if ( BestVisibleEnemy( ) )
		{
			Touch( NULL );
			return;
		}
		break;
	case 3:
		if ( pev->dmgtime < gpGlobals->time )
		{
			Touch( NULL );
			return;
		}
		break;
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// BigThink - the big brother of the spiral pulls the little
// one, and itself, towards the other
//=========================================================
void CDmlRocket::BigThink( void )
{
	if ( m_iDetonate == 2 )
	{
		if ( m_hEnemy == NULL )
		{
			Look( 125 );
			m_hEnemy = BestVisibleEnemy( );
		}

		if ( m_hEnemy != NULL )
		{
			Touch( NULL );
			return;
		}
	}

	if ( m_iDetonate == 3 && pev->dmgtime < gpGlobals->time )
	{
		Touch( NULL );
		return;
	}

	if ( !m_BrotherRocket )
	{
		SetThink( &CDmlRocket::FollowThink );
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	Vector vecDir, vecVelocity;
	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, 50 ) ) != NULL )
	{
		if ( pEntity->pev->takedamage && pEntity->edict() != pev->owner && this != pEntity && ( pEntity->pev->velocity.y >= 1 || pev->velocity.y <= -1 ) && FClassnameIs( pev, STRING( pEntity->pev->classname ) ) )
		{
			vecVelocity = pev->velocity;
			vecDir = pEntity->pev->origin - pev->origin;
			vecDir.Normalize( );	// sic: the result is thrown away
			vecDir.x = vecDir.x * m_iSpiralScalar_In;
			vecDir[1] *= m_iSpiralScalar_In;
			vecDir[2] *= m_iSpiralScalar_In;
			pev->velocity = vecDir + vecVelocity;
		}
	}

	pEntity = NULL;
	if ( m_BrotherRocket )
	{
		CDmlRocket *pBrother = m_BrotherRocket;
		while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, pBrother->pev->origin, 50 ) ) != NULL )
		{
			if ( pEntity->pev->takedamage )
			{
				if ( !pBrother )
					return;

				if ( pEntity->edict() != pev->owner && pBrother != pEntity && ( pEntity->pev->velocity.y >= 1 || pev->velocity.y <= -1 ) && FClassnameIs( pev, STRING( pEntity->pev->classname ) ) )
				{
					vecVelocity = pBrother->pev->velocity;
					vecDir = pEntity->pev->origin - pBrother->pev->origin;
					vecDir.Normalize( );	// sic: the result is thrown away
					vecDir.x = vecDir.x * m_iSpiralScalar_In;
					vecDir[1] *= m_iSpiralScalar_In;
					vecDir[2] *= m_iSpiralScalar_In;
					pBrother->pev->velocity = vecDir + vecVelocity;
				}
			}
		}
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// LittleThink - the little brother, until the big one is gone
//=========================================================
void CDmlRocket::LittleThink( void )
{
	if ( !m_BrotherRocket )
	{
		m_iIsBrother = 2;
		SetThink( &CDmlRocket::FollowThink );
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

void CDmlRocket::ExplodeThink( void )
{
	int iScale = 20;

	if ( m_iFlightpath == 2 && g_pGameRules->IsMultiplayer() )
	{
		// homing rockets are weaker in multiplayer
		iScale = 11;
		pev->dmg *= 0.8;
	}

	if ( m_iPayload == 2 )
	{
		ShootClusterGrenades( NULL );
		return;
	}

	if ( m_pSprite )
	{
		UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}

	UTIL_MakeVectors( m_angleOwner );
	Vector vecSpot;
	vecSpot = pev->origin + gpGlobals->v_forward * -32;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );
		WRITE_SHORT( m_sExplosion );
		WRITE_BYTE( iScale ); // scale * 10
		WRITE_BYTE( 15  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END();

	CSprite *pSprite = CSprite::SpriteCreate( "sprites/part2.spr", pev->origin, TRUE );
	pSprite->SetScale( 4 );
	pSprite->AnimateAndDie( 10 );
	pSprite->SetTransparency( kRenderTransAlpha, 255, 255, 255, 255, kRenderFxNoDissipation );
	pSprite->pev->frame = 0;
	UTIL_SetOrigin( pSprite->pev, pev->origin );
	pSprite->pev->origin.z += 16;

	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	if ( m_iDetonate == 2 )
	{
		// proximity: a bigger, weaker blast
		::RadiusDamage( pev->origin, pev, pevOwner, pev->dmg * 1.5, pev->dmg * 3.0, CLASS_NONE, DMG_BLAST | DMG_ALWAYSGIB );
	}
	else
	{
		::RadiusDamage( pev->origin, pev, pevOwner, pev->dmg, pev->dmg * 2, CLASS_NONE, DMG_BLAST | DMG_ALWAYSGIB );
	}

	switch ( RANDOM_LONG( 0, 2 ) )
	{
		case 0:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/kabam1.wav", 1, ATTN_NORM);	break;
		case 1:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/kabam2.wav", 1, ATTN_NORM);	break;
		case 2:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/kabam3.wav", 1, ATTN_NORM);	break;
	}
}

//=========================================================
// Explode - CGrenade's, made over for the rocket. Nothing
// calls it.
//=========================================================
void CDmlRocket::Explode( TraceResult *pTrace, int bitsDamageType )
{
	pev->model = iStringNull;//invisible
	pev->takedamage = DAMAGE_NO;

	if ( m_pSprite )
	{
		UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}

	// Pull out of the wall a bit
	if ( pTrace->flFraction != 1.0 )
	{
		pev->origin = pTrace->vecEndPos + (pTrace->vecPlaneNormal * (pev->dmg - 24) * 0.6);
	}

	int iContents = UTIL_PointContents ( pev->origin );

	UTIL_MakeVectors( m_angleOwner );
	Vector vecSpot;
	vecSpot = pev->origin + gpGlobals->v_forward * -32;

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( vecSpot.x );	// Send to PAS because of the sound
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );
		if (iContents != CONTENTS_WATER)
		{
			WRITE_SHORT( m_sExplosion );
		}
		else
		{
			WRITE_SHORT( g_sModelIndexWExplosion );
		}
		WRITE_BYTE( (pev->dmg - 50) * .60  ); // scale * 10
		WRITE_BYTE( 15  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END();

	CSprite *pSprite = CSprite::SpriteCreate( "sprites/part2.spr", pev->origin, TRUE );
	pSprite->SetScale( 4 );
	pSprite->AnimateAndDie( 10 );
	pSprite->SetTransparency( kRenderTransAlpha, 255, 255, 255, 255, kRenderFxNoDissipation );
	pSprite->pev->frame = 0;
	UTIL_SetOrigin( pSprite->pev, pev->origin );
	pSprite->pev->origin.z += 16;

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	RadiusDamage ( pev, pevOwner, pev->dmg, CLASS_NONE, bitsDamageType );

	if ( RANDOM_FLOAT( 0 , 1 ) < 0.5 )
	{
		UTIL_DecalTrace( pTrace, DECAL_SCORCH1 );
	}
	else
	{
		UTIL_DecalTrace( pTrace, DECAL_SCORCH2 );
	}

	if ( m_iPayload == 1 )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/kabam1.wav", 1, ATTN_NORM);	break;
			case 1:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/kabam2.wav", 1, ATTN_NORM);	break;
			case 2:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/kabam3.wav", 1, ATTN_NORM);	break;
		}
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/kaboom1.wav", 0.25, ATTN_NORM);	break;
			case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/kaboom2.wav", 0.25, ATTN_NORM);	break;
			case 2:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/kaboom3.wav", 0.25, ATTN_NORM);	break;
		}
	}

	SetThink( &CDmlRocket::Smoke );
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 0.3;

	if (iContents != CONTENTS_WATER)
	{
		int sparkCount = RANDOM_LONG(0,3);
		for ( int i = 0; i < sparkCount; i++ )
			Create( "spark_shower", pev->origin, pev->v_angle, NULL );
	}
}

void CDmlRocket::Smoke( void )
{
	if (UTIL_PointContents ( pev->origin ) == CONTENTS_WATER)
	{
		UTIL_Bubbles( pev->origin - Vector( 64, 64, 64 ), pev->origin + Vector( 64, 64, 64 ), 100 );
	}
	else
	{
		int iScale = (pev->dmg - 50) * 0.80;
		if ( iScale < 20 )
			iScale = 20;

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( iScale ); // scale * 10
			WRITE_BYTE( 12  ); // framerate
		MESSAGE_END();
	}
	UTIL_Remove( this );
}

void CDmlRocket::TumbleThink( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	if (pev->dmgtime - 1 < gpGlobals->time)
	{
		CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 400, 0.1 );
	}

	if (pev->dmgtime <= gpGlobals->time)
	{
		if ( m_iPayload == 2 )
		{
			SetThink( NULL );
			SetTouch( NULL );
			ShootClusterGrenades( NULL );
		}
		else
		{
			SetThink( NULL );
			SetTouch( NULL );
			RocketTouch( NULL );
		}
	}
	else if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// CreateFlame - the flame that follows the rocket
//=========================================================
void CDmlRocket::CreateFlame( void )
{
	m_pSprite = CSprite::SpriteCreate( "sprites/flame.spr", pev->origin, TRUE );
	m_pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
	m_pSprite->pev->spawnflags |= SF_SPRITE_TEMPORARY;
	m_pSprite->SetAttachment( edict(), 0 );
	m_pSprite->SetScale( 1 );
	m_pSprite->pev->framerate = 15;
	m_pSprite->pev->frame = 0;
	m_pSprite->TurnOn( );
}

void CDmlRocket::RemoveThink( void )
{
	UTIL_Remove( this );
}

//=========================================================
// BecomeTripmine - the tripmine payload sticks to the wall
// it hit and aims its beam away from it
//=========================================================
void CDmlRocket::BecomeTripmine( void )
{
	TraceResult tr, tr2;	// sic: tr2 is never used

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_NOT;
	pev->framerate = 0;
	pev->velocity.x = 0;
	pev->velocity.y = 0;
	pev->velocity.z = 0;

	UTIL_SetSize( pev, Vector( -8, -8, -8), Vector(8, 8, 8) );
	UTIL_SetOrigin( pev, pev->origin );

	// power up in a second
	m_flPowerUp = gpGlobals->time + 1.0;
	SetThink( &CDmlRocket::PowerupThink );
	pev->nextthink = gpGlobals->time + 0.2;

	pev->takedamage = DAMAGE_YES;
	pev->dmg = gSkillData.plrDmgDmlRocket;
	pev->health = 1; // don't let die normally

	UTIL_MakeVectors( pev->angles );
	m_vecDir = gpGlobals->v_forward;
	m_vecEnd = pev->origin + m_vecDir * 2048;
	m_pRealOwner = pev->owner;

	UTIL_TraceLine( pev->origin, pev->origin + m_vecDir * 128, dont_ignore_monsters, pev->owner, &tr );

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
		if ( pHit && !( pHit->pev->flags & FL_CONVEYOR ) )
		{
			Vector angles( UTIL_VecToAngles( tr.vecPlaneNormal ) );
			UTIL_MakeVectors( angles );
			m_vecDir = gpGlobals->v_forward;
			m_vecEnd = pev->origin + m_vecDir * 2048;
		}
	}

	pev->angles = UTIL_VecToAngles( pev->origin - m_vecEnd );
}

void CDmlRocket::PowerupThink( void  )
{
	pev->owner = NULL;

	if (gpGlobals->time > m_flPowerUp)
	{
		// make solid
		pev->solid = SOLID_BBOX;
		UTIL_SetOrigin( pev, pev->origin );

		MakeBeam( );
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

void CDmlRocket::KillBeam( void )
{
	if ( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}
}

void CDmlRocket::MakeBeam( void )
{
	TraceResult tr;

	UTIL_TraceLine( pev->origin, m_vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	m_flBeamLength = tr.flFraction;

	// set to follow laser spot
	SetThink( &CDmlRocket::BeamBreakThink );
	pev->nextthink = gpGlobals->time + 0.1;

	UTIL_MakeVectors( pev->angles );
	Vector vecTmpEnd = pev->origin + m_vecDir * 2048 * m_flBeamLength;

	m_pBeam = CBeam::BeamCreate( "sprites/dustyred.spr", 10 );
	m_pBeam->PointEntInit( vecTmpEnd, entindex() );
	m_pBeam->SetColor( 255, 0, 0 );
	m_pBeam->SetScrollRate( 150 );
	m_pBeam->SetBrightness( 255 );
	m_pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;
}

void CDmlRocket::BeamBreakThink( void  )
{
	TraceResult tr;

	// HACKHACK Set simple box using this really nice global!
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine( pev->origin, m_vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	// respawn detect.
	if ( !m_pBeam )
	{
		MakeBeam( );
		if ( tr.pHit )
			m_hOwner = CBaseEntity::Instance( tr.pHit );	// reset owner too
	}

	if (fabs( m_flBeamLength - tr.flFraction ) > 0.001)
	{
		pev->owner = m_pRealOwner;
		pev->health = 0;
		Killed( VARS( pev->owner ), GIB_NORMAL );
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

int CDmlRocket::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( m_iDetonate == 4 )
	{
		// a tripmine goes off when shot
		pev->owner = m_pRealOwner;
		pev->health = 0;
		Killed( VARS( pev->owner ), GIB_NORMAL );
		return 0;
	}

	return 0;
}

void CDmlRocket::Killed( entvars_t *pevAttacker, int iGib )
{
	pev->takedamage = DAMAGE_NO;

	if ( pevAttacker && ( pevAttacker->flags & FL_CLIENT ) )
	{
		// some client has destroyed this mine, he'll get credit for any kills
		pev->owner = ENT( pevAttacker );
	}

	SetThink( &CDmlRocket::DelayDeathThink );
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1, 0.3 );

	if ( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}

	EMIT_SOUND( ENT(pev), CHAN_BODY, "common/null.wav", 0.5, ATTN_NORM ); // shut off chargeup
}

void CDmlRocket::DelayDeathThink( void )
{
	pev->owner = m_pRealOwner;
	KillBeam();
	TraceResult tr;
	UTIL_TraceLine ( pev->origin + m_vecDir * 8, pev->origin - m_vecDir * 64,  dont_ignore_monsters, ENT(pev), & tr);

	SetTouch( NULL );
	SetThink( &CDmlRocket::RemoveThink );
	pev->nextthink = gpGlobals->time + 5.0;
	pev->effects |= EF_NODRAW;

	ExplodeThink( );
}
