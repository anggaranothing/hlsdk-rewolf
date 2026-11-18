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
// the mayan fliers, made from the SDK's flocking flyers
// (aflock.cpp): monster_dragonfly, a lone flyer that
// hovers and turns away from what it meets, and
// decore_butterflyflock, which spawns a flock of
// monster_butterfly around itself
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

#define		AFLOCK_MAX_RECRUIT_RADIUS	1024
#define		AFLOCK_FLY_SPEED			75
#define		AFLOCK_TURN_RATE			120
#define		AFLOCK_ACCELERATE			2
#define		AFLOCK_CHECK_DIST			192
#define		AFLOCK_TOO_CLOSE			40
#define		AFLOCK_TOO_FAR				256

//=========================================================
// CDragonfly - flies straight ahead until something is in
// the way, then stops and turns until the way is clear
//=========================================================
class CDragonfly : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	void BoidAdvanceFrame( void );
	BOOL FPathBlocked( void );
	void EXPORT FallHack( void );
	void EXPORT IdleThink( void );
	void EXPORT Start( void );
	void EXPORT FlockLeaderThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL	m_fTurning;// is this boid turning?
	BOOL	m_fCourseAdjust;// not used
	Vector	m_vecAdjustedVelocity;// not used
	float	m_flLastBlockedTime;
	float	m_flAlertTime;// not used
};

LINK_ENTITY_TO_CLASS( monster_dragonfly, CDragonfly );

TYPEDESCRIPTION	CDragonfly::m_SaveData[] =
{
	DEFINE_FIELD( CDragonfly, m_fTurning, FIELD_BOOLEAN ),
	DEFINE_FIELD( CDragonfly, m_fCourseAdjust, FIELD_BOOLEAN ),
	DEFINE_FIELD( CDragonfly, m_vecAdjustedVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CDragonfly, m_flLastBlockedTime, FIELD_TIME ),
	DEFINE_FIELD( CDragonfly, m_flAlertTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CDragonfly, CBaseMonster );

void CDragonfly::Spawn( void )
{
	Precache();

	pev->deadflag	= DEAD_NO;
	pev->solid		= SOLID_NOT;
	pev->movetype	= MOVETYPE_FLY;
	pev->takedamage	= DAMAGE_NO;
	pev->health		= 1;
	pev->classname	= MAKE_STRING( "monster_dragonfly" );
	pev->skin		= RANDOM_LONG( 0, 2 );
	pev->spawnflags	|= SF_MONSTER_HITMONSTERCLIP;

	m_flFieldOfView	= 0.2;

	SET_MODEL( ENT(pev), "models/dragonfly.mdl" );
	UTIL_SetSize( pev, Vector( -5, -5, 0 ), Vector( 5, 5, 2 ) );

	pev->frame = 0;
	pev->nextthink = gpGlobals->time + 0.1;
	SetThink( &CDragonfly::IdleThink );
}

void CDragonfly::Precache( void )
{
	PRECACHE_MODEL( "models/dragonfly.mdl" );
}

void CDragonfly::Killed( entvars_t *pevAttacker, int iGib )
{
	pev->deadflag = DEAD_DEAD;

	pev->framerate = 0;
	pev->effects = EF_NOINTERP;

	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	pev->movetype = MOVETYPE_TOSS;

	SetThink( &CDragonfly::FallHack );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CDragonfly::FallHack( void )
{
	if ( pev->flags & FL_ONGROUND )
	{
		if ( !FClassnameIs ( pev->groundentity, "worldspawn" ) )
		{
			pev->flags &= ~FL_ONGROUND;
			pev->nextthink = gpGlobals->time + 0.1;
		}
		else
		{
			pev->velocity = g_vecZero;
			SetThink( NULL );
		}
	}
}

//=========================================================
// BoidAdvanceFrame - the wings beat at one speed; only the
// lean follows the change of speed
//=========================================================
void CDragonfly::BoidAdvanceFrame( void )
{
	float flapspeed = ( pev->speed - pev->armorvalue ) / AFLOCK_ACCELERATE;
	pev->armorvalue = pev->armorvalue * .8 + pev->speed * .2;

	if ( flapspeed < 0 ) flapspeed = -flapspeed;
	if ( flapspeed < 0.25 ) flapspeed = 0.25;
	if ( flapspeed > 1.9 ) flapspeed = 1.9;

	pev->framerate = 1.5;

	// lean
	pev->avelocity.x = - ( pev->angles.x + flapspeed * 5 );

	StudioFrameAdvance( 0.1 );
}

void CDragonfly::IdleThink( void )
{
	pev->nextthink = gpGlobals->time + 0.2;

	// see if there's a client in the same pvs as the monster
	if ( !FNullEnt( FIND_CLIENT_IN_PVS( edict() ) ) )
	{
		SetThink( &CDragonfly::Start );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

//=========================================================
// Start - player enters the pvs, so get things going.
//=========================================================
void CDragonfly::Start( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	SetThink( &CDragonfly::FlockLeaderThink );

	SetActivity( ACT_FLY );
	ResetSequenceInfo();
	BoidAdvanceFrame();

	pev->speed = 200;
}

//=========================================================
// FPathBlocked - returns TRUE if there is an obstacle ahead.
// Leaves the normal of the last surface traced against in
// m_vecLastPosition, which the turn runs along; one time in
// twenty a clear path counts as blocked, straight ahead.
//=========================================================
BOOL CDragonfly::FPathBlocked( void )
{
	TraceResult		tr;
	BOOL			fBlocked;

	UTIL_MakeVectors( pev->angles );

	fBlocked = FALSE;// assume the way ahead is clear

	// check for obstacle ahead
	UTIL_TraceLine( pev->origin, pev->origin + gpGlobals->v_forward * 125, ignore_monsters, ENT(pev), &tr );
	if ( tr.flFraction != 1.0 )
	{
		m_flLastBlockedTime = gpGlobals->time;
		fBlocked = TRUE;
	}
	else
	{
		// extra wide checks
		UTIL_TraceLine( pev->origin + gpGlobals->v_right * 12, pev->origin + gpGlobals->v_right * 12 + gpGlobals->v_forward * 125, ignore_monsters, ENT(pev), &tr );
		if ( tr.flFraction != 1.0 )
		{
			m_flLastBlockedTime = gpGlobals->time;
			fBlocked = TRUE;
		}

		UTIL_TraceLine( pev->origin - gpGlobals->v_right * 12, pev->origin - gpGlobals->v_right * 12 + gpGlobals->v_forward * 125, ignore_monsters, ENT(pev), &tr );
		if ( tr.flFraction != 1.0 )
		{
			m_flLastBlockedTime = gpGlobals->time;
			fBlocked = TRUE;
		}
	}

	m_vecLastPosition = tr.vecPlaneNormal;

	if ( !fBlocked && UTIL_PercentChance( 5 ) )
	{
		m_vecLastPosition = gpGlobals->v_forward;
		return TRUE;
	}

	return fBlocked;
}

//=========================================================
// FlockLeaderThink - every tenth
//=========================================================
void CDragonfly::FlockLeaderThink( void )
{
	TraceResult		tr;
	Vector			vecDist;// used for general measurements
	Vector			vecDir;// used for general measurements
	int				cProcessed = 0;// keep track of how many other boids we've processed
	float			flLeftSide;
	float			flRightSide;
	BOOL			fBlocked;

	pev->nextthink = gpGlobals->time + 0.1;

	UTIL_MakeVectors( pev->angles );
	BoidAdvanceFrame();

	fBlocked = FALSE;
	if ( FPathBlocked() )
		fBlocked = TRUE;

	// still turning away from what was in the way?
	if ( m_fTurning && DotProduct( gpGlobals->v_forward, m_vecLastPosition ) < 0 )
		fBlocked = TRUE;

	// is the way ahead clear?
	if ( !fBlocked )
	{
		// if the boid is turning, stop the trend.
		if ( m_fTurning )
		{
			m_fTurning = FALSE;
			pev->avelocity.y = 0;
		}

		if ( pev->speed <= 200 )
			pev->speed += 5;

		// sic: flattens the engine's own forward vector
		gpGlobals->v_forward.z = 0;
		pev->velocity = gpGlobals->v_forward * pev->speed;
		return;
	}

	// IF we get this far in the function, the leader's path is blocked!
	if ( !m_fTurning )// something in the way and boid is not already turning to avoid
	{
		// measure clearance on left and right to pick the best dir to turn
		UTIL_TraceLine( pev->origin, pev->origin + gpGlobals->v_right * 125, ignore_monsters, ENT(pev), &tr );
		vecDist = ( tr.vecEndPos - pev->origin );
		flRightSide = vecDist.Length();

		UTIL_TraceLine( pev->origin, pev->origin - gpGlobals->v_right * 125, ignore_monsters, ENT(pev), &tr );
		vecDist = ( tr.vecEndPos - pev->origin );
		flLeftSide = vecDist.Length();

		// turn right if more clearance on right side
		if ( flRightSide > flLeftSide )
		{
			pev->avelocity.y = -AFLOCK_TURN_RATE;
			m_fTurning = TRUE;
		}
		else
		{
			pev->avelocity.y = AFLOCK_TURN_RATE;
			m_fTurning = TRUE;
		}
	}

	// check and make sure we aren't about to plow into the ground, don't let it happen
	UTIL_TraceLine( pev->origin, pev->origin - gpGlobals->v_up * 16, ignore_monsters, ENT(pev), &tr );
	if ( tr.flFraction != 1.0 && pev->velocity.z < 0 )
		pev->velocity.z = 0;

	// maybe it did, though.
	if ( FBitSet( pev->flags, FL_ONGROUND ) )
	{
		UTIL_SetOrigin( pev, pev->origin + Vector( 0, 0, 1 ) );
		pev->velocity.z = 0;
	}

	// hover while turning
	vecDir = pev->velocity;	// sic: unused
	pev->velocity = g_vecZero;
	pev->speed = 0;
}

//=========================================================
// CButterfly - a flocking flyer
//=========================================================
class CButterfly : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SpawnCommonCode( void );
	void EXPORT IdleThink( void );
	void BoidAdvanceFrame( void );
	void EXPORT FormFlock( void );
	void EXPORT Start( void );
	void EXPORT FlockLeaderThink( void );
	void EXPORT FlockFollowerThink( void );
	void EXPORT FallHack( void );
	void SpreadFlock( void );
	void SpreadFlock2( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	BOOL FPathBlocked( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int IsLeader( void ) { return m_pSquadLeader == this; }
	int	InSquad( void ) { return m_pSquadLeader != NULL; }
	int	SquadCount( void );
	void SquadRemove( CButterfly *pRemove );
	void SquadUnlink( void );
	void SquadAdd( CButterfly *pAdd );
	void SquadDisband( void );

	CButterfly *m_pSquadLeader;
	CButterfly *m_pSquadNext;
	BOOL	m_fTurning;// is this boid turning?
	BOOL	m_fCourseAdjust;// followers set this flag TRUE to override flocking while they avoid something
	Vector	m_vecReferencePoint;// last place we saw leader
	Vector	m_vecAdjustedVelocity;// adjusted velocity (used when fCourseAdjust is TRUE)
	float	m_flGoalSpeed;
	float	m_flLastBlockedTime;
	float	m_flFakeBlockedTime;
	float	m_flAlertTime;
	float	m_flFlockNextSoundTime;
	float	m_fTempLeader;// a stalled follower leads until this time
};

//=========================================================
// CButterflyFlock - spawns the flock and goes away
//=========================================================
class CButterflyFlock : public CBaseMonster
{
public:
	CButterflyFlock( void );
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void SpawnFlock( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int		m_cFlockSize;
	float	m_flFlockRadius;
};

LINK_ENTITY_TO_CLASS( decore_butterflyflock, CButterflyFlock );

TYPEDESCRIPTION	CButterflyFlock::m_SaveData[] =
{
	DEFINE_FIELD( CButterflyFlock, m_cFlockSize, FIELD_INTEGER ),
	DEFINE_FIELD( CButterflyFlock, m_flFlockRadius, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CButterflyFlock, CBaseMonster );

void CButterflyFlock::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "iFlockSize"))
	{
		m_cFlockSize = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "flFlockRadius"))
	{
		m_flFlockRadius = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

CButterflyFlock::CButterflyFlock( void )
{
	m_cFlockSize = 15;
	m_flFlockRadius = 100;
}

void CButterflyFlock::Spawn( void )
{
	Precache( );
	SpawnFlock();

	REMOVE_ENTITY(ENT(pev));		// dump the spawn ent
}

void CButterflyFlock::Precache( void )
{
	PRECACHE_MODEL("models/butterfly.mdl");
}

void CButterflyFlock::SpawnFlock( void )
{
	float R = m_flFlockRadius;
	int iCount;
	Vector vecSpot;
	CButterfly *pBoid, *pLeader;

	pLeader = pBoid = NULL;

	for ( iCount = 0 ; iCount < m_cFlockSize ; iCount++ )
	{
		pBoid = GetClassPtr( (CButterfly *)NULL );

		if ( !pLeader )
		{
			// make this guy the leader.
			pLeader = pBoid;

			pLeader->m_pSquadLeader = pLeader;
			pLeader->m_pSquadNext = NULL;
		}

		vecSpot.x = RANDOM_FLOAT( -R, R );
		vecSpot.y = RANDOM_FLOAT( -R, R );
		vecSpot.z = RANDOM_FLOAT( 0, 16 );
		vecSpot = pev->origin + vecSpot;

		UTIL_SetOrigin(pBoid->pev, vecSpot);
		pBoid->pev->movetype = MOVETYPE_FLY;
		pBoid->SpawnCommonCode();
		pBoid->pev->flags &= ~FL_ONGROUND;
		pBoid->pev->velocity = g_vecZero;
		pBoid->pev->angles	 = pev->angles;

		pBoid->pev->frame = 0;
		pBoid->pev->nextthink = gpGlobals->time + 0.2;
		pBoid->SetThink( &CButterfly::IdleThink );

		if ( pBoid != pLeader )
		{
			pLeader->SquadAdd( pBoid );
		}
	}
}

LINK_ENTITY_TO_CLASS( monster_butterfly, CButterfly );

TYPEDESCRIPTION	CButterfly::m_SaveData[] =
{
	DEFINE_FIELD( CButterfly, m_pSquadLeader, FIELD_CLASSPTR ),
	DEFINE_FIELD( CButterfly, m_pSquadNext, FIELD_CLASSPTR ),
	DEFINE_FIELD( CButterfly, m_fTurning, FIELD_BOOLEAN ),
	DEFINE_FIELD( CButterfly, m_fCourseAdjust, FIELD_BOOLEAN ),
	DEFINE_FIELD( CButterfly, m_vecReferencePoint, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CButterfly, m_vecAdjustedVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CButterfly, m_flGoalSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CButterfly, m_flLastBlockedTime, FIELD_TIME ),
	DEFINE_FIELD( CButterfly, m_flFakeBlockedTime, FIELD_TIME ),
	DEFINE_FIELD( CButterfly, m_flAlertTime, FIELD_TIME ),
//	DEFINE_FIELD( CButterfly, m_flFlockNextSoundTime, FIELD_TIME ),	// don't need to save
	DEFINE_FIELD( CButterfly, m_fTempLeader, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CButterfly, CBaseMonster );

void CButterfly::Spawn( void )
{
	Precache( );
	SpawnCommonCode();

	pev->frame = 0;
	pev->nextthink = gpGlobals->time + 0.1;
	m_fTempLeader = gpGlobals->time;
	SetThink( &CButterfly::IdleThink );
}

void CButterfly::Precache( void )
{
	PRECACHE_MODEL("models/butterfly.mdl");
}

void CButterfly::Killed( entvars_t *pevAttacker, int iGib )
{
	CButterfly *pSquad;

	pSquad = (CButterfly *)m_pSquadLeader;

	while ( pSquad )
	{
		pSquad->m_flAlertTime = gpGlobals->time + 15;
		pSquad = (CButterfly *)pSquad->m_pSquadNext;
	}

	if ( m_pSquadLeader )
	{
		m_pSquadLeader->SquadRemove( this );
	}

	pev->deadflag = DEAD_DEAD;

	pev->framerate = 0;
	pev->effects = EF_NOINTERP;

	UTIL_SetSize( pev, Vector(0,0,0), Vector(0,0,0) );
	pev->movetype = MOVETYPE_TOSS;

	SetThink ( &CButterfly::FallHack );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CButterfly::FallHack( void )
{
	if ( pev->flags & FL_ONGROUND )
	{
		if ( !FClassnameIs ( pev->groundentity, "worldspawn" ) )
		{
			pev->flags &= ~FL_ONGROUND;
			pev->nextthink = gpGlobals->time + 0.1;
		}
		else
		{
			pev->velocity = g_vecZero;
			SetThink( NULL );
		}
	}
}

void CButterfly::SpawnCommonCode( void )
{
	pev->deadflag	= DEAD_NO;
	pev->solid		= SOLID_TRIGGER;
	pev->movetype	= MOVETYPE_FLY;
	pev->takedamage	= DAMAGE_NO;
	pev->health		= 1;
	pev->classname	= MAKE_STRING("monster_butterfly");
	pev->skin		= RANDOM_LONG( 0, 9 );
	pev->spawnflags	|= SF_MONSTER_HITMONSTERCLIP;

	m_flFieldOfView	= 0.2;

	SET_MODEL(ENT(pev), "models/butterfly.mdl");
	UTIL_SetSize(pev, Vector(-5,-5,-5), Vector(5,5,5));
	UTIL_SetOrigin(pev, pev->origin);
}

void CButterfly::BoidAdvanceFrame( void )
{
	float flapspeed = (pev->speed - pev->armorvalue) / AFLOCK_ACCELERATE;
	pev->armorvalue = pev->armorvalue * .8 + pev->speed * .2;

	if (flapspeed < 0) flapspeed = -flapspeed;
	if (flapspeed < 0.25) flapspeed = 0.25;
	if (flapspeed > 1.9) flapspeed = 1.9;

	pev->framerate = flapspeed;

	// lean
	pev->avelocity.x = - (pev->angles.x + flapspeed * 5);

	// bank
	pev->avelocity.z = - (pev->angles.z + pev->avelocity.y);

	// pev->framerate		= flapspeed;
	StudioFrameAdvance( 0.1 );
}

void CButterfly::IdleThink( void )
{
	pev->nextthink = gpGlobals->time + 0.2;

	// see if there's a client in the same pvs as the monster
	if ( !FNullEnt( FIND_CLIENT_IN_PVS( edict() ) ) )
	{
		SetThink( &CButterfly::Start );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

//=========================================================
// Start - player enters the pvs, so get things going.
//=========================================================
void CButterfly::Start( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( IsLeader() )
	{
		SetThink( &CButterfly::FlockLeaderThink );
	}
	else
	{
		SetThink( &CButterfly::FlockFollowerThink );
	}

	SetActivity ( ACT_FLY );
	ResetSequenceInfo( );
	BoidAdvanceFrame( );

	pev->speed = AFLOCK_FLY_SPEED;// no delay!
}

//=========================================================
// Leader boid calls this to form a flock from surrounding boids
//=========================================================
void CButterfly::FormFlock( void )
{
	if ( !InSquad() )
	{
		// I am my own leader
		m_pSquadLeader = this;
		m_pSquadNext = NULL;
		int squadCount = 1;

		CBaseEntity *pEntity = NULL;

		while ((pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, AFLOCK_MAX_RECRUIT_RADIUS )) != NULL)
		{
			CBaseMonster *pRecruit = pEntity->MyMonsterPointer( );

			if ( pRecruit && pRecruit != this && pRecruit->IsAlive() && !pRecruit->m_pCine )
			{
				// Can we recruit this guy?
				if ( FClassnameIs ( pRecruit->pev, "monster_butterfly" ) )
				{
					squadCount++;
					SquadAdd( (CButterfly *)pRecruit );
				}
			}
		}
	}

	SetThink( &CButterfly::IdleThink );// now that flock is formed, go to idle and wait for a player to come along.
	pev->nextthink = gpGlobals->time;
}

//=========================================================
// Searches for boids that are too close and pushes them away
//=========================================================
void CButterfly::SpreadFlock( void )
{
	Vector		vecDir;
	float		flSpeed;// holds vector magnitude while we fiddle with the direction

	CButterfly *pList = m_pSquadLeader;
	while ( pList )
	{
		if ( pList != this && ( pev->origin - pList->pev->origin ).Length() <= AFLOCK_TOO_CLOSE )
		{
			// push the other away
			vecDir = ( pList->pev->origin - pev->origin );
			vecDir = vecDir.Normalize();

			// store the magnitude of the other boid's velocity, and normalize it so we
			// can average in a course that points away from the leader.
			flSpeed = pList->pev->velocity.Length();
			pList->pev->velocity = pList->pev->velocity.Normalize();
			pList->pev->velocity = ( pList->pev->velocity + vecDir ) * 0.5;
			pList->pev->velocity = pList->pev->velocity * flSpeed;
		}

		pList = pList->m_pSquadNext;
	}
}

//=========================================================
// Alters the caller's course if he's too close to others
//
// This function should **ONLY** be called when Caller's velocity is normalized!!
//=========================================================
void CButterfly::SpreadFlock2( void )
{
	Vector		vecDir;

	CButterfly *pList = m_pSquadLeader;
	while ( pList )
	{
		if ( pList != this && ( pev->origin - pList->pev->origin ).Length() <= AFLOCK_TOO_CLOSE )
		{
			vecDir = ( pev->origin - pList->pev->origin );
			vecDir = vecDir.Normalize();

			pev->velocity = (pev->velocity + vecDir);
		}

		pList = pList->m_pSquadNext;
	}
}

//=========================================================
// FBoidPathBlocked - returns TRUE if there is an obstacle ahead
//=========================================================
BOOL CButterfly::FPathBlocked( void )
{
	TraceResult		tr;
	Vector			vecDist;// used for general measurements
	Vector			vecDir;// used for general measurements
	BOOL			fBlocked;

	if ( m_flFakeBlockedTime > gpGlobals->time )
	{
		m_flLastBlockedTime = gpGlobals->time;
		return TRUE;
	}

	// use VELOCITY, not angles, not all boids point the direction they are flying
	//vecDir = UTIL_VecToAngles( pevBoid->velocity );
	UTIL_MakeVectors ( pev->angles );

	fBlocked = FALSE;// assume the way ahead is clear

	// check for obstacle ahead
	UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_forward * AFLOCK_CHECK_DIST, ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction != 1.0)
	{
		m_flLastBlockedTime = gpGlobals->time;
		fBlocked = TRUE;
	}

	// extra wide checks
	UTIL_TraceLine(pev->origin + gpGlobals->v_right * 12, pev->origin + gpGlobals->v_right * 12 + gpGlobals->v_forward * AFLOCK_CHECK_DIST, ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction != 1.0)
	{
		m_flLastBlockedTime = gpGlobals->time;
		fBlocked = TRUE;
	}

	UTIL_TraceLine(pev->origin - gpGlobals->v_right * 12, pev->origin - gpGlobals->v_right * 12 + gpGlobals->v_forward * AFLOCK_CHECK_DIST, ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction != 1.0)
	{
		m_flLastBlockedTime = gpGlobals->time;
		fBlocked = TRUE;
	}

	if ( !fBlocked && gpGlobals->time - m_flLastBlockedTime > 6 )
	{
		// not blocked, and it's been a few seconds since we've actually been blocked.
		m_flFakeBlockedTime = gpGlobals->time + RANDOM_LONG(1, 3);
	}

	return	fBlocked;
}

//=========================================================
// Leader boids use this think every tenth
//=========================================================
void CButterfly::FlockLeaderThink( void )
{
	TraceResult		tr;
	Vector			vecDist;// used for general measurements
	Vector			vecDir;// used for general measurements
	int				cProcessed = 0;// keep track of how many other boids we've processed
	float			flLeftSide;
	float			flRightSide;

	// a follower leading for a while goes back to following
	if ( !IsLeader() && m_fTempLeader < gpGlobals->time )
	{
		SetThink( &CButterfly::FlockFollowerThink );
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;

	UTIL_MakeVectors ( pev->angles );

	// is the way ahead clear?
	if ( !FPathBlocked () )
	{
		// if the boid is turning, stop the trend.
		if ( m_fTurning )
		{
			m_fTurning = FALSE;
			pev->avelocity.y = 0;
		}

		if (pev->speed <= AFLOCK_FLY_SPEED )
			pev->speed+= 5;

		pev->velocity = gpGlobals->v_forward * pev->speed;

		BoidAdvanceFrame( );

		return;
	}

	// IF we get this far in the function, the leader's path is blocked!
	if ( !m_fTurning)// something in the way and boid is not already turning to avoid
	{
		// measure clearance on left and right to pick the best dir to turn
		UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_right * AFLOCK_CHECK_DIST, ignore_monsters, ENT(pev), &tr);
		vecDist = (tr.vecEndPos - pev->origin);
		flRightSide = vecDist.Length();

		UTIL_TraceLine(pev->origin, pev->origin - gpGlobals->v_right * AFLOCK_CHECK_DIST, ignore_monsters, ENT(pev), &tr);
		vecDist = (tr.vecEndPos - pev->origin);
		flLeftSide = vecDist.Length();

		// turn right if more clearance on right side
		if ( flRightSide > flLeftSide )
		{
			pev->avelocity.y = -AFLOCK_TURN_RATE;
			m_fTurning = TRUE;
		}
		// default to left turn :)
		else if ( flLeftSide > flRightSide )
		{
			pev->avelocity.y = AFLOCK_TURN_RATE;
			m_fTurning = TRUE;
		}
		else
		{
			// equidistant. Pick randomly between left and right.
			m_fTurning = TRUE;

			if ( RANDOM_LONG( 0, 1 ) == 0 )
			{
				pev->avelocity.y = AFLOCK_TURN_RATE;
			}
			else
			{
				pev->avelocity.y = -AFLOCK_TURN_RATE;
			}
		}
	}

	if ( IsLeader() )
		SpreadFlock( );

	pev->velocity = gpGlobals->v_forward * pev->speed;

	// check and make sure we aren't about to plow into the ground, don't let it happen
	UTIL_TraceLine(pev->origin, pev->origin - gpGlobals->v_up * 16, ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction != 1.0 && pev->velocity.z < 0 )
		pev->velocity.z = 0;

	// maybe it did, though.
	if ( FBitSet (pev->flags, FL_ONGROUND) )
	{
		UTIL_SetOrigin (pev, pev->origin + Vector ( 0 , 0 , 1 ) );
		pev->velocity.z = 0;
	}

	if ( m_flFlockNextSoundTime < gpGlobals->time )
	{
		// butterflies make no sound
		m_flFlockNextSoundTime = gpGlobals->time + RANDOM_FLOAT( 1, 3 );
	}

	BoidAdvanceFrame( );

	return;
}

//=========================================================
// follower boids execute this code when flocking
//=========================================================
void CButterfly::FlockFollowerThink( void )
{
	TraceResult		tr;
	Vector			vecDist;
	Vector			vecDir;
	Vector			vecDirToLeader;
	float			flDistToLeader;

	pev->nextthink = gpGlobals->time + 0.1;

	// stalled: lead for a few seconds
	if ( pev->velocity.Length() < 5 )
	{
		SetThink( &CButterfly::FlockLeaderThink );
		pev->nextthink = gpGlobals->time + 0.1;
		m_fTempLeader = gpGlobals->time + RANDOM_LONG( 5, 24 );
		return;
	}

	if ( IsLeader() || !InSquad() )
	{
		// the leader has been killed and this flyer suddenly finds himself the leader.
		SetThink ( &CButterfly::FlockLeaderThink );
		return;
	}

	vecDirToLeader = ( m_pSquadLeader->pev->origin - pev->origin );
	flDistToLeader = vecDirToLeader.Length();

	// head between our course and the leader
	pev->angles = UTIL_VecToAngles( pev->velocity.Normalize() + vecDirToLeader.Normalize() );

	//
	// We can see the leader, so try to catch up to it
	//
	if ( FInViewCone ( m_pSquadLeader ) )
	{
		// if we're too far away, speed up
		if ( flDistToLeader > AFLOCK_TOO_FAR )
		{
			m_flGoalSpeed = m_pSquadLeader->pev->velocity.Length() * 1.5;
		}

		// if we're too close, slow down
		else if ( flDistToLeader < AFLOCK_TOO_CLOSE )
		{
			m_flGoalSpeed = m_pSquadLeader->pev->velocity.Length() * 0.5;
		}
	}
	else
	{
		// wait up! the leader isn't out in front, so we slow down to let him pass
		m_flGoalSpeed = m_pSquadLeader->pev->velocity.Length() * 0.5;
	}

	SpreadFlock2();

	pev->speed = pev->velocity.Length();
	pev->velocity = pev->velocity.Normalize();

	// if we are too far from leader, average a vector towards it into our current velocity
	if ( flDistToLeader > AFLOCK_TOO_FAR )
	{
		vecDirToLeader = vecDirToLeader.Normalize();
		pev->velocity = (pev->velocity + vecDirToLeader) * 0.5;
	}

	// clamp speeds and handle acceleration
	if ( m_flGoalSpeed > AFLOCK_FLY_SPEED * 2 )
	{
		m_flGoalSpeed  = AFLOCK_FLY_SPEED * 2;
	}

	if ( pev->speed < m_flGoalSpeed )
	{
		pev->speed += AFLOCK_ACCELERATE;
	}
	else if ( pev->speed > m_flGoalSpeed )
	{
		pev->speed -= AFLOCK_ACCELERATE;
	}

	pev->velocity = pev->velocity * pev->speed;

	BoidAdvanceFrame( );
}

//=========================================================
//
// SquadUnlink(), Unlink the squad pointers.
//
//=========================================================
void CButterfly::SquadUnlink( void )
{
	m_pSquadLeader = NULL;
	m_pSquadNext	= NULL;
}

//=========================================================
//
// SquadAdd(), add pAdd to my squad
//
//=========================================================
void CButterfly::SquadAdd( CButterfly *pAdd )
{
	ASSERT( pAdd!=NULL );
	ASSERT( !pAdd->InSquad() );
	ASSERT( this->IsLeader() );

	pAdd->m_pSquadNext = m_pSquadNext;
	m_pSquadNext = pAdd;
	pAdd->m_pSquadLeader = this;
}

//=========================================================
//
// SquadRemove(), remove pRemove from my squad.
// If I am pRemove, promote m_pSquadNext to leader
//
//=========================================================
void CButterfly::SquadRemove( CButterfly *pRemove )
{
	ASSERT( pRemove!=NULL );
	ASSERT( this->IsLeader() );
	ASSERT( pRemove->m_pSquadLeader == this );

	if ( SquadCount() > 2 )
	{
		// Removing the leader, promote m_pSquadNext to leader
		if ( pRemove == this )
		{
			CButterfly *pLeader = m_pSquadNext;

			// copy the enemy LKP to the new leader
			pLeader->m_vecEnemyLKP = m_vecEnemyLKP;

			if ( pLeader )
			{
				CButterfly *pList = pLeader;

				while ( pList )
				{
					pList->m_pSquadLeader = pLeader;
					pList = pList->m_pSquadNext;
				}

			}
			SquadUnlink();
		}
		else	// removing a node
		{
			CButterfly *pList = this;

			// Find the node before pRemove
			while ( pList->m_pSquadNext != pRemove )
			{
				// assert to test valid list construction
				ASSERT( pList->m_pSquadNext != NULL );
				pList = pList->m_pSquadNext;
			}
			// List validity
			ASSERT( pList->m_pSquadNext == pRemove );

			// Relink without pRemove
			pList->m_pSquadNext = pRemove->m_pSquadNext;

			// Unlink pRemove
			pRemove->SquadUnlink();
		}
	}
	else
		SquadDisband();
}

//=========================================================
//
// SquadCount(), return the number of members of this squad
// callable from leaders & followers
//
//=========================================================
int CButterfly::SquadCount( void )
{
	CButterfly *pList = m_pSquadLeader;
	int squadCount = 0;
	while ( pList )
	{
		squadCount++;
		pList = pList->m_pSquadNext;
	}

	return squadCount;
}

//=========================================================
//
// SquadDisband(), Unlink all squad members
//
//=========================================================
void CButterfly::SquadDisband( void )
{
	CButterfly *pList = m_pSquadLeader;
	CButterfly *pNext;

	while ( pList )
	{
		pNext = pList->m_pSquadNext;
		pList->SquadUnlink();
		pList = pNext;
	}
}
