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
// decorative birds: decore_eagle and decore_pteradon glide
// along a path of entities with the apache's flight model,
// flap now and then, and fall when they are shot
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

class CBaseBird : public CBaseMonster
{
public:
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void Spawn( void );
	int  Classify( void ) { return CLASS_NONE; }
	int  BloodColor( void ) { return DONT_BLEED; }
	void Killed( entvars_t *pevAttacker, int iGib );
	int  TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );

	virtual void CrySound( void ) = 0;
	virtual void FlapSound( void ) = 0;

private:
	void EXPORT HuntThink( void );
	void EXPORT FlyTouch( CBaseEntity *pOther );
	void EXPORT CrashTouch( CBaseEntity *pOther );
	void EXPORT DyingThink( void );
	void EXPORT StartupUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT NullThink( void );

	void Flight( void );

	float m_flForce;

	Vector m_posTarget;

	Vector m_vecDesired;
	Vector m_posDesired;

	Vector m_vecGoal;

	float m_flGoalSpeed;

	float m_flLastSeen;
	float m_flPrevSeen;

	float m_tNextFlap;
	float m_tNextCry;
};

TYPEDESCRIPTION	CBaseBird::m_SaveData[] =
{
	DEFINE_FIELD( CBaseBird, m_flForce, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseBird, m_posTarget, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseBird, m_vecDesired, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseBird, m_posDesired, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseBird, m_vecGoal, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseBird, m_flLastSeen, FIELD_TIME ),
	DEFINE_FIELD( CBaseBird, m_flPrevSeen, FIELD_TIME ),
	DEFINE_FIELD( CBaseBird, m_flGoalSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseBird, m_tNextFlap, FIELD_FLOAT ),
};
IMPLEMENT_SAVERESTORE( CBaseBird, CBaseMonster );


// the subclasses precache and set their model, then call this
void CBaseBird :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	UTIL_SetSize( pev, Vector( -32, -32, -16 ), Vector( 32, 32, 16 ) );
	UTIL_SetOrigin( pev, pev->origin );

	pev->flags |= FL_MONSTER;
	pev->takedamage		= DAMAGE_AIM;
	pev->health			= 10;

	m_flFieldOfView = -0.7;

	pev->sequence = 0;
	ResetSequenceInfo( );
	pev->frame = RANDOM_LONG(0, 0xFF);

	InitBoneControllers();

	SetThink( &CBaseBird::HuntThink );
	SetTouch( &CBaseBird::FlyTouch );
	pev->nextthink = gpGlobals->time + 1.0;

	m_tNextFlap = m_tNextCry = gpGlobals->time;
}


void CBaseBird::NullThink( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.5;
}


void CBaseBird::StartupUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( &CBaseBird::HuntThink );
	SetTouch( &CBaseBird::FlyTouch );
	pev->nextthink = gpGlobals->time + 0.1;
	SetUse( NULL );
}

void CBaseBird :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = 0.5;

	SetSequenceByName( "falling" );

	SetThink( &CBaseBird::DyingThink );
	SetTouch( &CBaseBird::CrashTouch );
	pev->nextthink = gpGlobals->time + 0.1;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_TRIGGER;

	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ) );
}

void CBaseBird :: DyingThink( void )
{
	StudioFrameAdvance( );

	if ( m_fSequenceFinished )
		SetSequenceByName( "falling" );

	// still in the world?
	if ( pev->origin.x >= -4096 && pev->origin.x <= 4096 && pev->origin.y >= -4096 && pev->origin.y <= 4096 && pev->origin.z >= -4096 && pev->origin.z <= 4096 )
	{
		pev->nextthink = gpGlobals->time + 0.1;
		pev->avelocity = pev->avelocity * 1.03;
	}
	else
	{
		UTIL_Remove( this );
	}
}


void CBaseBird::FlyTouch( CBaseEntity *pOther )
{
	// bounce if we hit something solid
	if ( pOther->pev->solid == SOLID_BSP)
	{
		TraceResult tr = UTIL_GetGlobalTrace( );

		// UNDONE, do a real bounce
		pev->velocity = pev->velocity + tr.vecPlaneNormal * (pev->velocity.Length() + 200);
	}
}


void CBaseBird::CrashTouch( CBaseEntity *pOther )
{
	SetSequenceByName( "dead" );

	// only land if we hit something solid
	if ( pOther->pev->solid == SOLID_BSP)
	{
		SetTouch( NULL );
		SetThink( NULL );
		pev->nextthink = gpGlobals->time;
		pev->angles.x = 0;
		pev->angles.y = 0;
		pev->angles.z = 0;
		pev->movetype = MOVETYPE_NONE;
		pev->solid = SOLID_TRIGGER;
		UTIL_SetOrigin( pev, pev->origin );
	}
}


void CBaseBird :: HuntThink( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	// generic speed up
	if (m_flGoalSpeed < 600)
		m_flGoalSpeed += 10;

	if ( m_tNextCry < gpGlobals->time )
	{
		m_tNextCry = gpGlobals->time + RANDOM_LONG( 20, 30 );
		CrySound( );
	}

	if ( m_fSequenceFinished )
	{
		if ( m_tNextFlap < gpGlobals->time && UTIL_PercentChance( 5 ) )
		{
			m_tNextFlap = gpGlobals->time + RANDOM_LONG( 3, 4 );
			SetSequenceByName( "glideflap" );
			FlapSound( );
		}
		else
		{
			SetSequenceByName( "glide" );
		}
	}

	if ( m_pGoalEnt == NULL && !FStringNull(pev->target) )// this monster has a target
	{
		m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
		if (m_pGoalEnt)
		{
			m_posDesired = m_pGoalEnt->pev->origin;
			UTIL_MakeAimVectors( m_pGoalEnt->pev->angles );
			m_vecGoal = gpGlobals->v_forward;
		}
	}

	float flLength = (pev->origin - m_posDesired).Length();

	if (m_pGoalEnt)
	{
		if (flLength < 100)
		{
			m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( m_pGoalEnt->pev->target ) );
			if (m_pGoalEnt)
			{
				m_posDesired = m_pGoalEnt->pev->origin;
				m_posTarget = m_pGoalEnt->pev->origin;
				UTIL_MakeAimVectors( m_pGoalEnt->pev->angles );
				m_vecGoal = gpGlobals->v_forward;
				flLength = (pev->origin - m_posDesired).Length();
			}
		}
	}
	else
	{
		m_posDesired = pev->origin;
	}

	if (flLength > 100)
	{
		// sic: nothing sets m_flLastSeen, so this only happens in a level's first 90 seconds
		if (m_flLastSeen + 90 > gpGlobals->time && DotProduct( (m_posTarget - pev->origin).Normalize(), (m_posDesired - pev->origin).Normalize( )) > 0.25)
		{
			m_vecDesired = (m_posTarget - pev->origin).Normalize( );
		}
		else
		{
			m_vecDesired = (m_posDesired - pev->origin).Normalize( );
		}
	}
	else
	{
		m_vecDesired = m_vecGoal;
	}

	Flight( );

	UTIL_MakeAimVectors( pev->angles );
	Vector vecEst = (gpGlobals->v_up * 800 + pev->velocity).Normalize( );
}


void CBaseBird :: Flight( void )
{
	// tilt model 8 degrees
	Vector vecAdj = Vector( 8.0, 0, 0 );

	// estimate where I'll be facing in one seconds
	UTIL_MakeAimVectors( pev->angles + pev->avelocity * 2 + vecAdj);

	float flSide = DotProduct( m_vecDesired, gpGlobals->v_right );

	if (flSide < 0)
	{
		if (pev->avelocity.y < 60)
		{
			pev->avelocity.y += 8; // 9 * (3.0/2.0);
		}
	}
	else
	{
		if (pev->avelocity.y > -60)
		{
			pev->avelocity.y -= 8; // 9 * (3.0/2.0);
		}
	}

	// estimate where I'll be in two seconds
	UTIL_MakeAimVectors( pev->angles + pev->avelocity * 1 + vecAdj);
	Vector vecEst = pev->origin + pev->velocity * 2.0 + gpGlobals->v_up * m_flForce * 20 - Vector( 0, 0, 384 * 2 );

	// add immediate force
	UTIL_MakeAimVectors( pev->angles + vecAdj);
	pev->velocity.x += gpGlobals->v_up.x * m_flForce;
	pev->velocity.y += gpGlobals->v_up.y * m_flForce;
	pev->velocity.z += gpGlobals->v_up.z * m_flForce;
	// add gravity
	pev->velocity.z -= 38.4; // 32ft/sec


	float flSpeed = pev->velocity.Length();

	// flap when slow
	if ( flSpeed < 400 && m_tNextFlap < gpGlobals->time )
	{
		m_tNextFlap = gpGlobals->time + RANDOM_LONG( 3, 4 );
		SetSequenceByName( "glideflap" );
		FlapSound( );
	}

	float flDir = DotProduct( Vector( gpGlobals->v_forward.x, gpGlobals->v_forward.y, 0 ), Vector( pev->velocity.x, pev->velocity.y, 0 ) );
	if (flDir < 0)
		flSpeed = -flSpeed;

	float flDist = DotProduct( m_posDesired - vecEst, gpGlobals->v_forward );

	float flSlip = -DotProduct( m_posDesired - vecEst, gpGlobals->v_right );

	// fly sideways
	if (flSlip > 0)
	{
		if (pev->angles.z > -30 && pev->avelocity.z > -15)
			pev->avelocity.z -= 4;
		else
			pev->avelocity.z += 2;
	}
	else
	{

		if (pev->angles.z < 30 && pev->avelocity.z < 15)
			pev->avelocity.z += 4;
		else
			pev->avelocity.z -= 2;
	}

	// sideways drag
	pev->velocity.x = pev->velocity.x * (1.0 - fabs( gpGlobals->v_right.x ) * 0.05);
	pev->velocity.y = pev->velocity.y * (1.0 - fabs( gpGlobals->v_right.y ) * 0.05);
	pev->velocity.z = pev->velocity.z * (1.0 - fabs( gpGlobals->v_right.z ) * 0.05);

	// general drag
	pev->velocity = pev->velocity * 0.995;

	// apply power to stay correct height
	if (m_flForce < 80 && vecEst.z < m_posDesired.z)
	{
		m_flForce += 12;
	}
	else if (m_flForce > 30)
	{
		if (vecEst.z > m_posDesired.z)
			m_flForce -= 8;
	}

	// pitch forward or back to get to target
	if (flDist > 0 && flSpeed < m_flGoalSpeed && pev->angles.x + pev->avelocity.x > -40)
	{
		// lean forward
		pev->avelocity.x -= 12.0;
	}
	else if (flDist < 0 && flSpeed > -50 && pev->angles.x + pev->avelocity.x  < 20)
	{
		// lean backward
		pev->avelocity.x += 12.0;
	}
	else if (pev->angles.x + pev->avelocity.x > 0)
	{
		pev->avelocity.x -= 4.0;
	}
	else if (pev->angles.x + pev->avelocity.x < 0)
	{
		pev->avelocity.x += 4.0;
	}
}


int CBaseBird :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if (pevInflictor->owner == edict())
		return 0;

	return CBaseEntity::TakeDamage(  pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_NEVERGIB );
}


//=========================================================
// decore_eagle
//=========================================================
class CEagle : public CBaseBird
{
public:
	void Spawn( void );
	void Precache( void );
	void CrySound( void );
	void FlapSound( void );
};
LINK_ENTITY_TO_CLASS( decore_eagle, CEagle );

void CEagle :: Spawn( void )
{
	Precache( );
	SET_MODEL( ENT(pev), "models/eagle.mdl" );
	CBaseBird::Spawn( );
}

void CEagle :: FlapSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:	EMIT_SOUND( ENT(pev), CHAN_VOICE, "eagle/eagle_flap1.wav", 1, 0.3 ); break;
	case 1:	EMIT_SOUND( ENT(pev), CHAN_VOICE, "eagle/eagle_flap2.wav", 1, 0.3 ); break;
	case 2:	EMIT_SOUND( ENT(pev), CHAN_VOICE, "eagle/eagle_flap3.wav", 1, 0.3 ); break;
	}
}

void CEagle :: CrySound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:	EMIT_SOUND( ENT(pev), CHAN_VOICE, "eagle/eagle_cry1.wav", 1, 0.3 ); break;
	case 1:	EMIT_SOUND( ENT(pev), CHAN_VOICE, "eagle/eagle_cry2.wav", 1, 0.3 ); break;
	case 2:	EMIT_SOUND( ENT(pev), CHAN_VOICE, "eagle/eagle_cry3.wav", 1, 0.3 ); break;
	}
}

void CEagle :: Precache( void )
{
	PRECACHE_MODEL( "models/eagle.mdl" );

	PRECACHE_SOUND( "eagle/eagle_cry1.wav" );
	PRECACHE_SOUND( "eagle/eagle_cry2.wav" );
	PRECACHE_SOUND( "eagle/eagle_cry3.wav" );

	PRECACHE_SOUND( "eagle/eagle_flap1.wav" );
	PRECACHE_SOUND( "eagle/eagle_flap2.wav" );
	PRECACHE_SOUND( "eagle/eagle_flap3.wav" );
}


//=========================================================
// decore_pteradon: a silent bird
//=========================================================
class CPteradon : public CBaseBird
{
public:
	void Spawn( void );
	void Precache( void );
	void CrySound( void );
	void FlapSound( void );
};
LINK_ENTITY_TO_CLASS( decore_pteradon, CPteradon );

void CPteradon :: Spawn( void )
{
	Precache( );
	SET_MODEL( ENT(pev), "models/pteradon2.mdl" );
	CBaseBird::Spawn( );
}

void CPteradon :: FlapSound( void )
{
}

void CPteradon :: CrySound( void )
{
}

void CPteradon :: Precache( void )
{
	PRECACHE_MODEL( "models/pteradon2.mdl" );
}
