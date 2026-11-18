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
// monster_hatchetfish: the mayan hatchet fish. Half-Life's
// leech with Gunman's model and sounds: it swims about in
// its water, bites whatever it chases and flops when it
// leaves the water.
//=========================================================

#include	"float.h"
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"

// Animation events
#define HATCHETFISH_AE_ATTACK		1
#define HATCHETFISH_AE_FLOP			2


// Movement constants

#define		HATCHETFISH_ACCELERATE		10
#define		HATCHETFISH_CHECK_DIST		45
#define		HATCHETFISH_SWIM_SPEED		75
#define		HATCHETFISH_SWIM_ACCEL		80
#define		HATCHETFISH_SWIM_DECEL		10
#define		HATCHETFISH_TURN_RATE		90
#define		HATCHETFISH_SIZEX			10
#define		HATCHETFISH_FRAMETIME		0.1


class CHatchetFish : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );

	void EXPORT SwimThink( void );
	void EXPORT DeadThink( void );
	void Touch( CBaseEntity *pOther )
	{
		if ( pOther->IsPlayer() )
		{
			// If the client is pushing me, give me some base velocity
			if ( gpGlobals->trace_ent && gpGlobals->trace_ent == edict() )
			{
				pev->basevelocity = pOther->pev->velocity * 2;
				pev->flags |= FL_BASEVELOCITY;
			}
		}
	}

	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin + Vector(-8,-8,0);
		pev->absmax = pev->origin + Vector(8,8,2);
	}

	void AlertSound( void );
	void UpdateMotion( void );
	float ObstacleDistance( CBaseEntity *pTarget );
	void MakeVectors( void );
	void RecalculateWaterlevel( void );
	void SwitchLeechState( void );
	
	// Base entity functions
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int	BloodColor( void ) { return DONT_BLEED; }
	void Killed( entvars_t *pevAttacker, int iGib );
	void Activate( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	int	Classify( void ) { return CLASS_ALIEN_PREY; }
	int IRelationship( CBaseEntity *pTarget );
	int VisionFlags( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

private:
	// UNDONE: Remove unused boid vars, do group behavior
	float	m_flTurning;// is this boid turning?
	BOOL	m_fPathBlocked;// TRUE if there is an obstacle ahead
	float	m_flAccelerate;
	float	m_obstacle;
	float	m_top;
	float	m_bottom;
	float	m_height;
	float	m_waterTime;
	float	m_sideTime;		// Timer to randomly check clearance on sides
	float	m_zTime;
	float	m_stateTime;
};



LINK_ENTITY_TO_CLASS( monster_hatchetfish, CHatchetFish );

TYPEDESCRIPTION	CHatchetFish::m_SaveData[] = 
{
	DEFINE_FIELD( CHatchetFish, m_flTurning, FIELD_FLOAT ),
	DEFINE_FIELD( CHatchetFish, m_fPathBlocked, FIELD_BOOLEAN ),
	DEFINE_FIELD( CHatchetFish, m_flAccelerate, FIELD_FLOAT ),
	DEFINE_FIELD( CHatchetFish, m_obstacle, FIELD_FLOAT ),
	DEFINE_FIELD( CHatchetFish, m_top, FIELD_FLOAT ),
	DEFINE_FIELD( CHatchetFish, m_bottom, FIELD_FLOAT ),
	DEFINE_FIELD( CHatchetFish, m_height, FIELD_FLOAT ),
	DEFINE_FIELD( CHatchetFish, m_waterTime, FIELD_TIME ),
	DEFINE_FIELD( CHatchetFish, m_sideTime, FIELD_TIME ),
	DEFINE_FIELD( CHatchetFish, m_zTime, FIELD_TIME ),
	DEFINE_FIELD( CHatchetFish, m_stateTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CHatchetFish, CBaseMonster );


void CHatchetFish::Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), "models/hatchet.mdl");
	
	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_FLY;
	SetBits(pev->flags, FL_SWIM);
	pev->health			= gSkillData.hatchetfishHealth;
	// Don't push the minz down too much or the water check will fail because this entity is really point-sized
	UTIL_SetSize( pev, Vector(-1,-1,0), Vector(1,1,2));

	m_flFieldOfView		= -0.5;	// 180 degree FOV
	m_flDistLook		= 750;
	MonsterInit();
	SetThink( &CHatchetFish::SwimThink );
	SetUse( NULL );
	SetTouch( NULL );
	pev->view_ofs = g_vecZero;

	m_flTurning = 0;
	m_fPathBlocked = FALSE;
	SetActivity( ACT_SWIM );
	SetState( MONSTERSTATE_IDLE );
	m_stateTime = gpGlobals->time + RANDOM_FLOAT( 1, 5 );
}


int CHatchetFish::VisionFlags( void )
{
	return 7;
}


void CHatchetFish::AlertSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_STATIC, "hatchet/hatchet_sight1.wav", 1.0, 0.3, 0, PITCH_NORM );
		// sic: no break, the first sight sound is followed by the second
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_STATIC, "hatchet/hatchet_sight2.wav", 1.0, 0.3, 0, PITCH_NORM );
		break;
	}
}


void CHatchetFish::Activate( void )
{
	RecalculateWaterlevel();
}



void CHatchetFish::RecalculateWaterlevel( void )
{
	// Calculate boundaries
	Vector vecTest = pev->origin - Vector(0,0,400);

	TraceResult tr;

	UTIL_TraceLine(pev->origin, vecTest, missile, edict(), &tr);
	if ( tr.flFraction != 1.0 )
		m_bottom = tr.vecEndPos.z + 1;
	else
		m_bottom = vecTest.z;

	m_top = UTIL_WaterLevel( pev->origin, pev->origin.z, pev->origin.z + 400 ) - 1;

	// Chop off 20% of the outside range
	float newBottom = m_bottom * 0.8 + m_top * 0.2;
	m_top = m_bottom * 0.2 + m_top * 0.8;
	m_bottom = newBottom;
	m_height = RANDOM_FLOAT( m_bottom, m_top );
	m_waterTime = gpGlobals->time + RANDOM_FLOAT( 5, 7 );
}


void CHatchetFish::SwitchLeechState( void )
{
	m_stateTime = gpGlobals->time + RANDOM_FLOAT( 3, 6 );
	if ( m_MonsterState == MONSTERSTATE_COMBAT )
	{
		m_hEnemy = NULL;
		SetState( MONSTERSTATE_IDLE );
		// We may be up against the player, so redo the side checks
		m_sideTime = 0;
	}
	else
	{
		Look( m_flDistLook );
		CBaseEntity *pEnemy = BestVisibleEnemy();
		if ( pEnemy && pEnemy->pev->waterlevel != 0 )
		{
			m_hEnemy = pEnemy;
			SetState( MONSTERSTATE_COMBAT );
			m_stateTime = gpGlobals->time + RANDOM_FLOAT( 18, 25 );
		}
	}
}


int CHatchetFish::IRelationship( CBaseEntity *pTarget )
{
	if ( pTarget->IsPlayer() )
		return R_DL;
	return CBaseMonster::IRelationship( pTarget );
}


void CHatchetFish::Precache( void )
{
	PRECACHE_MODEL("models/hatchet.mdl");

	PRECACHE_SOUND("hatchet/hatchet_bite1.wav");
	PRECACHE_SOUND("hatchet/hatchet_bite2.wav");
	PRECACHE_SOUND("hatchet/hatchet_splash1.wav");
	PRECACHE_SOUND("hatchet/hatchet_splash2.wav");
	PRECACHE_SOUND("hatchet/hatchet_sight1.wav");
	PRECACHE_SOUND("hatchet/hatchet_sight2.wav");
}


int CHatchetFish::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	pev->velocity = g_vecZero;

	// Nudge the fish away from the damage
	if ( pevInflictor )
	{
		pev->velocity = (pev->origin - pevInflictor->origin).Normalize() * 25;
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}


void CHatchetFish::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case HATCHETFISH_AE_ATTACK:
		CBaseEntity *pEnemy;

		pEnemy = m_hEnemy;
		if ( pEnemy != NULL )
		{
			Vector dir, face;

			UTIL_MakeVectorsPrivate( pev->angles, face, NULL, NULL );
			face.z = 0;
			dir = (pEnemy->pev->origin - pev->origin);
			dir.z = 0;
			dir = dir.Normalize();
			face = face.Normalize();

			
			if ( DotProduct(dir, face) > 0.9 )		// Only take damage if the fish is facing the prey
				pEnemy->TakeDamage( pev, pev, gSkillData.hatchetfishDmg, DMG_SLASH );
		}
		m_stateTime -= 2;
		break;
	
	case HATCHETFISH_AE_FLOP:
		// Play flop sound
		break;
	
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}


void CHatchetFish::MakeVectors( void )
{
	Vector tmp = pev->angles;
	tmp.x = -tmp.x;
	UTIL_MakeVectors ( tmp );
}


//
// ObstacleDistance - returns normalized distance to obstacle
//
float CHatchetFish::ObstacleDistance( CBaseEntity *pTarget )
{
	TraceResult		tr;
	Vector			vecTest;

	// use VELOCITY, not angles, not all boids point the direction they are flying
	//Vector vecDir = UTIL_VecToAngles( pev->velocity );
	MakeVectors();

	// check for obstacle ahead
	vecTest = pev->origin + gpGlobals->v_forward * HATCHETFISH_CHECK_DIST;
	UTIL_TraceLine(pev->origin, vecTest, missile, edict(), &tr);

	if ( tr.fStartSolid )
	{
		pev->speed = -HATCHETFISH_SWIM_SPEED * 0.5;
	}

	if ( tr.flFraction != 1.0 )
	{
		if ( (pTarget == NULL || tr.pHit != pTarget->edict()) )
		{
			return tr.flFraction;
		}
		else
		{
			if ( fabs(m_height - pev->origin.z) > 10 )
				return tr.flFraction;
		}
	}

	if ( m_sideTime < gpGlobals->time )
	{
		// extra wide checks
		vecTest = pev->origin + gpGlobals->v_right * HATCHETFISH_SIZEX * 2 + gpGlobals->v_forward * HATCHETFISH_CHECK_DIST;
		UTIL_TraceLine(pev->origin, vecTest, missile, edict(), &tr);
		if (tr.flFraction != 1.0)
			return tr.flFraction;

		vecTest = pev->origin - gpGlobals->v_right * HATCHETFISH_SIZEX * 2 + gpGlobals->v_forward * HATCHETFISH_CHECK_DIST;
		UTIL_TraceLine(pev->origin, vecTest, missile, edict(), &tr);
		if (tr.flFraction != 1.0)
			return tr.flFraction;

		// Didn't hit either side, so stop testing for another 0.5 - 1 seconds
		m_sideTime = gpGlobals->time + RANDOM_FLOAT(0.5,1);
	}
	return 1.0;
}


void CHatchetFish::DeadThink( void )
{
	if ( m_fSequenceFinished )
	{
		if ( m_Activity == ACT_DIEFORWARD )
		{
			SetThink( NULL );
			StopAnimation();
			return;
		}
		else if ( pev->flags & FL_ONGROUND )
		{
			pev->solid = SOLID_NOT;
			SetActivity(ACT_DIEFORWARD);
		}
	}
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	// Apply damage velocity, but keep out of the walls
	if ( pev->velocity.x != 0 || pev->velocity.y != 0 )
	{
		TraceResult tr;

		// Look 0.5 seconds ahead
		UTIL_TraceLine(pev->origin, pev->origin + pev->velocity * 0.5, missile, edict(), &tr);
		if (tr.flFraction != 1.0)
		{
			pev->velocity.x = 0;
			pev->velocity.y = 0;
		}
	}
}



void CHatchetFish::UpdateMotion( void )
{
	float flapspeed = (pev->speed - m_flAccelerate) / HATCHETFISH_ACCELERATE;
	m_flAccelerate = m_flAccelerate * 0.8 + pev->speed * 0.2;

	if (flapspeed < 0) 
		flapspeed = -flapspeed;
	flapspeed += 1.0;
	if (flapspeed < 0.5) 
		flapspeed = 0.5;
	if (flapspeed > 1.9) 
		flapspeed = 1.9;

	pev->framerate = flapspeed;

	if ( !m_fPathBlocked )
		pev->avelocity.y = pev->ideal_yaw;
	else
		pev->avelocity.y = pev->ideal_yaw * m_obstacle;

	if ( pev->avelocity.y > 150 )
		m_IdealActivity = ACT_TURN_LEFT;
	else if ( pev->avelocity.y < -150 )
		m_IdealActivity = ACT_TURN_RIGHT;
	else
		m_IdealActivity = ACT_SWIM;

	// lean
	float targetPitch, delta;
	delta = m_height - pev->origin.z;

	if ( delta < -10 )
		targetPitch = -30;
	else if ( delta > 10 )
		targetPitch = 30;
	else
		targetPitch = 0;

	pev->angles.x = UTIL_Approach( targetPitch, pev->angles.x, 60 * HATCHETFISH_FRAMETIME );

	// bank
	pev->avelocity.z = - (pev->angles.z + (pev->avelocity.y * 0.25));

	if ( m_MonsterState == MONSTERSTATE_COMBAT && HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
		m_IdealActivity = ACT_MELEE_ATTACK1;

	// Out of water check
	if ( !pev->waterlevel )
	{
		pev->movetype = MOVETYPE_TOSS;
		m_IdealActivity = ACT_TWITCH;
		pev->velocity = g_vecZero;

		// Animation will intersect the floor if either of these is non-zero
		pev->angles.z = 0;
		pev->angles.x = 0;

		if ( pev->framerate < 1.0 )
			pev->framerate = 1.0;
	}
	else if ( pev->movetype == MOVETYPE_TOSS )
	{
		pev->movetype = MOVETYPE_FLY;
		pev->flags &= ~FL_ONGROUND;
		RecalculateWaterlevel();
		m_waterTime = gpGlobals->time + 2;	// Recalc again soon, water may be rising
	}

	if ( m_Activity != m_IdealActivity )
	{
		SetActivity ( m_IdealActivity );
	}
	float flInterval = StudioFrameAdvance();
	DispatchAnimEvents ( flInterval );
}


void CHatchetFish::SwimThink( void )
{
	TraceResult		tr;
	float			flLeftSide;
	float			flRightSide;
	float			targetSpeed;
	float			targetYaw = 0;
	CBaseEntity		*pTarget;

	if ( FNullEnt( FIND_CLIENT_IN_PVS( edict() ) ) )
	{
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(1,1.5);
		pev->velocity = g_vecZero;
		return;
	}
	else
		pev->nextthink = gpGlobals->time + 0.1;

	targetSpeed = HATCHETFISH_SWIM_SPEED;

	if ( m_waterTime < gpGlobals->time )
		RecalculateWaterlevel();

	if ( m_stateTime < gpGlobals->time )
		SwitchLeechState();

	ClearConditions( bits_COND_CAN_MELEE_ATTACK1 );
	switch( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		pTarget = m_hEnemy;
		if ( !pTarget )
			SwitchLeechState();
		else
		{
			// Chase the enemy's eyes
			m_height = pTarget->pev->origin.z + pTarget->pev->view_ofs.z - 5;
			// Clip to viable water area
			if ( m_height < m_bottom )
				m_height = m_bottom;
			else if ( m_height > m_top )
				m_height = m_top;
			Vector location = pTarget->pev->origin - pev->origin;
			location.z += (pTarget->pev->view_ofs.z);
			if ( location.Length() < 40 )
				SetConditions( bits_COND_CAN_MELEE_ATTACK1 );
			// Turn towards target ent
			targetYaw = UTIL_VecToYaw( location );

			targetYaw = UTIL_AngleDiff( targetYaw, UTIL_AngleMod( pev->angles.y ) );

			if ( targetYaw < (-HATCHETFISH_TURN_RATE*0.75) )
				targetYaw = (-HATCHETFISH_TURN_RATE*0.75);
			else if ( targetYaw > (HATCHETFISH_TURN_RATE*0.75) )
				targetYaw = (HATCHETFISH_TURN_RATE*0.75);
			else
				targetSpeed *= 2;
		}

		break;

	default:
		if ( m_zTime < gpGlobals->time )
		{
			float newHeight = RANDOM_FLOAT( m_bottom, m_top );
			m_height = 0.5 * m_height + 0.5 * newHeight;
			m_zTime = gpGlobals->time + RANDOM_FLOAT( 1, 4 );
		}
		if ( RANDOM_LONG( 0, 100 ) < 10 )
			targetYaw = RANDOM_LONG( -30, 30 );
		pTarget = NULL;
		// oldorigin test
		if ( (pev->origin - pev->oldorigin).Length() < 1 )
		{
			// If the fish didn't move, there must be something blocking it, so try to turn
			m_sideTime = 0;
		}

		break;
	}

	m_obstacle = ObstacleDistance( pTarget );
	pev->oldorigin = pev->origin;
	if ( m_obstacle < 0.1 )
		m_obstacle = 0.1;

	// is the way ahead clear?
	if ( m_obstacle == 1.0 )
	{
		// if the fish is turning, stop the trend.
		if ( m_flTurning != 0 )
		{
			m_flTurning = 0;
		}

		m_fPathBlocked = FALSE;
		pev->speed = UTIL_Approach( targetSpeed, pev->speed, HATCHETFISH_SWIM_ACCEL * HATCHETFISH_FRAMETIME );
		pev->velocity = gpGlobals->v_forward * pev->speed;

	}
	else
	{
		m_obstacle = 1.0 / m_obstacle;
		// IF we get this far in the function, the leader's path is blocked!
		m_fPathBlocked = TRUE;

		if ( m_flTurning == 0 )// something in the way and the fish is not already turning to avoid
		{
			Vector vecTest;
			// measure clearance on left and right to pick the best dir to turn
			vecTest = pev->origin + (gpGlobals->v_right * HATCHETFISH_SIZEX) + (gpGlobals->v_forward * HATCHETFISH_CHECK_DIST);
			UTIL_TraceLine(pev->origin, vecTest, missile, edict(), &tr);
			flRightSide = tr.flFraction;

			vecTest = pev->origin + (gpGlobals->v_right * -HATCHETFISH_SIZEX) + (gpGlobals->v_forward * HATCHETFISH_CHECK_DIST);
			UTIL_TraceLine(pev->origin, vecTest, missile, edict(), &tr);
			flLeftSide = tr.flFraction;

			// turn left, right or random depending on clearance ratio
			float delta = (flRightSide - flLeftSide);
			if ( delta > 0.1 || (delta > -0.1 && RANDOM_LONG(0,100)<50) )
				m_flTurning = -HATCHETFISH_TURN_RATE;
			else
				m_flTurning = HATCHETFISH_TURN_RATE;
		}
		pev->speed = UTIL_Approach( -(HATCHETFISH_SWIM_SPEED*0.5), pev->speed, HATCHETFISH_SWIM_DECEL * HATCHETFISH_FRAMETIME * m_obstacle );
		pev->velocity = gpGlobals->v_forward * pev->speed;
	}
	pev->ideal_yaw = m_flTurning + targetYaw;
	UpdateMotion();
}


void CHatchetFish::Killed(entvars_t *pevAttacker, int iGib)
{
	Vector			vecSplatDir;
	TraceResult		tr;

	// tell owner ( if any ) that we're dead.This is mostly for MonsterMaker functionality.
	CBaseEntity *pOwner = CBaseEntity::Instance(pev->owner);
	if (pOwner)
		pOwner->DeathNotice(pev);

	// When we hit the ground, play the "death_end" activity
	if ( pev->waterlevel )
	{
		pev->angles.z = 0;
		pev->angles.x = 0;
		pev->origin.z += 1;
		pev->avelocity = g_vecZero;
		if ( RANDOM_LONG( 0, 99 ) < 70 )
			pev->avelocity.y = RANDOM_LONG( -720, 720 );

		pev->gravity = 0.02;
		ClearBits(pev->flags, FL_ONGROUND);
		SetActivity( ACT_DIESIMPLE );
	}
	else
		SetActivity( ACT_DIEFORWARD );
	
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;
	pev->takedamage = DAMAGE_NO;
	SetThink( &CHatchetFish::DeadThink );
}
