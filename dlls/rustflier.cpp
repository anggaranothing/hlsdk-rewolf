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
// monster_rustflier: the rust chapter's flying robot, made
// from the chopper. It flies its path behind a force field,
// spits rustbit shots and ball lightning, and only a single
// blow above its target damage brings it down, along its
// death path
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "sphereexplosion.h"
#include "rust.h"

// beamgun.cpp's ball lightning
class CBallLightning : public CGrenade
{
public:
	static CBallLightning *Shoot( Vector vecStart, Vector vecAngles, CBaseEntity *pOwner, int iSpeed );
};


class CRustFlier : public CBaseMonster
{
public:
	CRustFlier( void );

	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	int  Classify( void ) { return CLASS_ROBOT_GUNNER; }
	int  BloodColor( void ) { return DONT_BLEED; }
	void PreRemove( void );
	void GibMonster( void ) {}

	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin + Vector( -600, -600, -32 );
		pev->absmax = pev->origin + Vector( 600, 600, 32 );
	}

private:
	void EXPORT HuntThink( void );
	void EXPORT FlyTouch( CBaseEntity *pOther );
	void EXPORT CrashTouch( CBaseEntity *pOther );
	void EXPORT CrashingThink( void );
	void EXPORT FlierUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void StartCrash( void );
	void Flight( void );
	void ResetStrafe( void );
	BOOL CanStrafe( void );
	BOOL FireGun( void );
	BOOL FireBall( void );

	int  TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);

	float m_tNextStrafe;	// when the next burst may start
	float m_tNextBall;		// when the next ball lightning may fly
	int m_iBullets;			// shots left in this burst

	float m_flForce;

	Vector m_posTarget;

	Vector m_vecDesired;
	Vector m_posDesired;

	Vector m_vecGoal;

	float m_flGoalSpeed;

	Vector m_angGun;
	float m_flLastSeen;
	float m_flPrevSeen;

	int m_iSpriteTexture;
	int m_iExplode;
	int m_iBodyGibs;

	int m_iSoundState; // don't save this

	float m_flNextShot;		// when the burst's next shot may go

	CSprite *m_pFlyball;	// the glow it carries
	int m_iShellTime;		// thinks the shield's glow has shown
	float m_fTargetDamage;	// a single blow above this brings it down
	int m_iShieldOn;
	string_t m_iszDeathPath;	// the path_corner it crashes into
	string_t m_iszTriggerOnDeath;
	Vector m_vecDeathAngle;		// the angles it crashes with
	float m_flTimeTillCrash;	// seconds to the end of the death path
};
LINK_ENTITY_TO_CLASS( monster_rustflier, CRustFlier );

// sic: the shield, the target damage and the death path are in twice
TYPEDESCRIPTION	CRustFlier::m_SaveData[] =
{
	DEFINE_FIELD( CRustFlier, m_flForce, FIELD_FLOAT ),
	DEFINE_FIELD( CRustFlier, m_posTarget, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CRustFlier, m_vecDesired, FIELD_VECTOR ),
	DEFINE_FIELD( CRustFlier, m_posDesired, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CRustFlier, m_vecGoal, FIELD_VECTOR ),
	DEFINE_FIELD( CRustFlier, m_angGun, FIELD_VECTOR ),
	DEFINE_FIELD( CRustFlier, m_flLastSeen, FIELD_TIME ),
	DEFINE_FIELD( CRustFlier, m_flPrevSeen, FIELD_TIME ),
	DEFINE_FIELD( CRustFlier, m_flGoalSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CRustFlier, m_tNextBall, FIELD_FLOAT ),
	DEFINE_FIELD( CRustFlier, m_tNextStrafe, FIELD_FLOAT ),
	DEFINE_FIELD( CRustFlier, m_iBullets, FIELD_INTEGER ),
	DEFINE_FIELD( CRustFlier, m_pFlyball, FIELD_CLASSPTR ),
	DEFINE_FIELD( CRustFlier, m_iShellTime, FIELD_INTEGER ),
	DEFINE_FIELD( CRustFlier, m_fTargetDamage, FIELD_FLOAT ),
	DEFINE_FIELD( CRustFlier, m_iShieldOn, FIELD_INTEGER ),
	DEFINE_FIELD( CRustFlier, m_iszDeathPath, FIELD_STRING ),
	DEFINE_FIELD( CRustFlier, m_iszTriggerOnDeath, FIELD_STRING ),
	DEFINE_FIELD( CRustFlier, m_fTargetDamage, FIELD_FLOAT ),
	DEFINE_FIELD( CRustFlier, m_iShieldOn, FIELD_INTEGER ),
	DEFINE_FIELD( CRustFlier, m_iszDeathPath, FIELD_STRING ),
	DEFINE_FIELD( CRustFlier, m_vecDeathAngle, FIELD_VECTOR ),
	DEFINE_FIELD( CRustFlier, m_flTimeTillCrash, FIELD_FLOAT ),
};
IMPLEMENT_SAVERESTORE( CRustFlier, CBaseMonster );


CRustFlier::CRustFlier( void )
{
	m_iShieldOn = TRUE;
	m_fTargetDamage = 10000;
	m_vecDeathAngle = g_vecZero;
	m_flTimeTillCrash = 10;
}


void CRustFlier::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "startshieldstate"))
	{
		m_iShieldOn = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "targetdamage"))
	{
		m_fTargetDamage = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "deathpath"))
	{
		m_iszDeathPath = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "deathangle"))
	{
		sscanf( pkvd->szValue, "%f %f %f\n", &m_vecDeathAngle.x, &m_vecDeathAngle.y, &m_vecDeathAngle.z );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "timetillcrash"))
	{
		m_flTimeTillCrash = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "triggerondeath"))
	{
		m_iszTriggerOnDeath = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}


void CRustFlier :: Spawn( void )
{
	Precache( );

	SET_MODEL(ENT(pev), "models/rustflyer.mdl");

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->effects = EF_INVLIGHT;
	pev->takedamage = DAMAGE_YES;
	pev->flags |= FL_MONSTER;
	pev->deadflag = DEAD_NO;
	pev->health = gSkillData.chopperHealth;

	UTIL_SetSize( pev, Vector( -64, -64, 0 ), Vector( 64, 64, 64 ) );
	UTIL_SetOrigin( pev, pev->origin );

	m_iShellTime = 0;
	m_flFieldOfView = -0.7;

	pev->sequence = 0;
	ResetSequenceInfo( );
	pev->frame = RANDOM_LONG(0, 0xFF);

	SetThink( &CRustFlier::HuntThink );
	SetTouch( &CRustFlier::FlyTouch );
	SetUse( &CRustFlier::FlierUse );
	pev->nextthink = gpGlobals->time + 1.0;

	ResetStrafe( );
	m_pFlyball = NULL;
}


void CRustFlier::Precache( void )
{
	PRECACHE_MODEL("models/rustflyer.mdl");
	PRECACHE_MODEL("models/rustflyer_crashed.mdl");
	PRECACHE_MODEL("sprites/flyball.spr");

	m_iSpriteTexture = PRECACHE_MODEL( "sprites/white.spr" );

	PRECACHE_SOUND("flier/ap_rotor2.wav");
	PRECACHE_SOUND("flier/ap_whine1.wav");
	PRECACHE_SOUND("flier/flyer_sick.wav");

	m_iExplode	= PRECACHE_MODEL( "sprites/fexplo.spr" );
	m_iBodyGibs = PRECACHE_MODEL( "models/metalplategibs_green.mdl" );

	UTIL_PrecacheOther( "ball_lightning" );
	UTIL_PrecacheOther( "rustbitshot" );
	UTIL_PrecacheOther( "sphere_explosion" );

	PRECACHE_SOUND("flier/field_impact1.wav");
	PRECACHE_SOUND("flier/field_impact2.wav");
	PRECACHE_SOUND("flier/field_impact3.wav");
}


void CRustFlier::FlyTouch( CBaseEntity *pOther )
{
	// bounce if we hit something solid
	if ( pOther->pev->solid == SOLID_BSP)
	{
		TraceResult tr = UTIL_GetGlobalTrace( );

		// UNDONE, do a real bounce
		pev->velocity = pev->velocity + tr.vecPlaneNormal * (pev->velocity.Length() + 200);
	}
}


// nothing sets it
void CRustFlier::CrashTouch( CBaseEntity *pOther )
{
	// only crash if we hit something solid
	if ( pOther->pev->solid == SOLID_BSP)
	{
		SetTouch( NULL );
		pev->nextthink = gpGlobals->time;
	}
}


void CRustFlier :: HuntThink( void )
{
	StudioFrameAdvance( );

	if ( m_fSequenceFinished )
		SetSequenceByName( "idle" );

	if ( m_pFlyball == NULL )
	{
		m_pFlyball = CSprite::SpriteCreate( "sprites/flyball.spr", pev->origin, TRUE );
		m_pFlyball->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
		m_pFlyball->SetAttachment( edict(), 2 );
		m_pFlyball->pev->scale = 2.0;
		m_pFlyball->pev->framerate = 9.0;
		m_pFlyball->pev->spawnflags = 0;
		m_pFlyball->TurnOn( );
	}

	pev->nextthink = gpGlobals->time + 0.1;

	// the shield's glow goes after 20 thinks
	if ( m_iShellTime == 20 )
	{
		pev->renderfx = kRenderFxNone;
		m_iShellTime = 0;
	}
	else
		m_iShellTime++;

	if ( m_pGoalEnt == NULL && !FStringNull(pev->target) )// this monster has a target
	{
		m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
		if (m_pGoalEnt)
		{
			m_posDesired = m_pGoalEnt->pev->origin;
			UTIL_MakeAimVectors( m_pGoalEnt->pev->angles );
			m_vecGoal = gpGlobals->v_forward;
		}

		// sic: the target may not exist
		if ( !FStringNull( m_pGoalEnt->pev->message ) )
			FireTargets( STRING( m_pGoalEnt->pev->message ), this, this, USE_TOGGLE, 0 );
	}

	Look( 4092 );
	m_hEnemy = BestVisibleEnemy( );

	// generic speed up
	if (m_flGoalSpeed < 600)
		m_flGoalSpeed += 10;

	if (m_hEnemy != NULL)
	{
		if (FVisible( m_hEnemy ))
		{
			if (m_flLastSeen < gpGlobals->time - 5)
				m_flPrevSeen = gpGlobals->time;
			m_flLastSeen = gpGlobals->time;
			m_posTarget = m_hEnemy->Center( );
		}
		else
		{
			m_hEnemy = NULL;
		}
	}

	float flLength = (pev->origin - m_posDesired).Length();

	if (m_pGoalEnt)
	{
		if (flLength < 128)
		{
			m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( m_pGoalEnt->pev->target ) );
			if (m_pGoalEnt)
			{
				m_posDesired = m_pGoalEnt->pev->origin;
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

	if (flLength > 250) // 500
	{
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

	// sic: never runs; retail inlines the estimate below only with one
	// more call in the function, so some dead code like this was there
	if ( 0 )
		ResetStrafe( );

	// a gagged flier holds its fire
	if ( !(pev->spawnflags & SF_MONSTER_GAG) )
	{
		UTIL_MakeAimVectors( pev->angles );
		Vector vecEst = (gpGlobals->v_up * 800 + pev->velocity).Normalize( );	// sic: unused

		if ( m_iBullets == 0 )
		{
			ResetStrafe( );
			return;
		}

		if ( m_tNextBall < gpGlobals->time && FireBall( ) )
			m_tNextBall = gpGlobals->time + 2.85;

		if ( CanStrafe( ) && (m_flLastSeen + 1 > gpGlobals->time) && (m_flPrevSeen + 2 < gpGlobals->time) && FireGun( ) )
		{
			// slow down if we're fireing
			if (m_flGoalSpeed > 75)
				m_flGoalSpeed = 75;

			m_iBullets--;
			m_flNextShot = gpGlobals->time + 0.1;
		}
	}
}


// sic: unused; their empty constructors are all retail has of them. Const, so
// VC6 gives them no storage: retail's rustflier.obj has no .bss
const Vector g_vecFlierUnused1;
const Vector g_vecFlierUnused2;


// a blow above the target damage: down goes the shield, and
// the flier flies its death path to the crash
void CRustFlier :: StartCrash( void )
{
	Vector vecSrc = pev->origin;
	CSphereExplosion *pSphere = CSphereExplosion::CreateSphereExplosion( vecSrc, this, 200, 8, 255, 5, 1, FALSE, FALSE, 2.5, 1.0, 0.5 );
	if ( pSphere )
		pSphere->UseToggle( this, this, USE_ON, 0 );

	UTIL_Remove( m_pFlyball );
	m_pFlyball = NULL;
	pev->health = -1;

	if ( m_iShieldOn )
		FlierUse( this, this, USE_SET, 0 );

	FireTargets( STRING( m_iszTriggerOnDeath ), this, this, USE_TOGGLE, 0 );

	SetThink( &CRustFlier::CrashingThink );
	pev->nextthink = gpGlobals->time + 0.1;

	m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( m_iszDeathPath ) );
	if ( m_pGoalEnt )
	{
		Vector vecDir = m_pGoalEnt->pev->origin - pev->origin;

		pev->velocity = vecDir.Normalize() * (m_pGoalEnt->pev->origin - pev->origin).Length() / m_flTimeTillCrash;

		pev->angles.x = (int)pev->angles.x % 360;
		pev->angles.y = (int)pev->angles.y % 360;
		pev->angles.z = (int)pev->angles.z % 360;

		float flPitch = ( UTIL_AngleDistance( m_vecDeathAngle.x, pev->angles.x ) / m_flTimeTillCrash );
		float flYaw = ( UTIL_AngleDistance( m_vecDeathAngle.y, pev->angles.y ) / m_flTimeTillCrash );
		float flRoll = ( UTIL_AngleDistance( m_vecDeathAngle.z, pev->angles.z ) / m_flTimeTillCrash );

		pev->avelocity.x = flPitch;
		pev->avelocity.y = flYaw;
		pev->avelocity.z = flRoll;

		m_posDesired = m_pGoalEnt->pev->origin;
	}
}


// smoke all along the falling flier
void CRustFlier :: CrashingThink( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	Vector vecForward, vecRight;
	g_engfuncs.pfnAngleVectors( pev->angles, vecForward, vecRight, NULL );

	int cPuffs = RANDOM_LONG( 2, 4 );
	for ( int i = 0; i < cPuffs; i++ )
	{
		Vector vecSpot = pev->origin - vecForward * RANDOM_LONG( 0, 200 ) - vecRight * RANDOM_LONG( 0, 128 );

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecSpot.x );
			WRITE_COORD( vecSpot.y );
			WRITE_COORD( vecSpot.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( RANDOM_LONG(0,9) + 20 ); // scale * 10
			WRITE_BYTE( RANDOM_LONG(8,16) ); // framerate
		MESSAGE_END();
	}
}


void CRustFlier :: Flight( void )
{
	// tilt model 8 degrees
	Vector vecAdj = Vector( 8.0, 0, 0 );

	// estimate where I'll be facing in one seconds
	UTIL_MakeAimVectors( pev->angles + pev->avelocity * 2 + vecAdj);
	// Vector vecEst1 = pev->origin + pev->velocity + gpGlobals->v_up * m_flForce - Vector( 0, 0, 384 );
	// float flSide = DotProduct( m_posDesired - vecEst1, gpGlobals->v_right );

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
	pev->avelocity.y *= 0.98;

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
	float flDir = DotProduct( Vector( gpGlobals->v_forward.x, gpGlobals->v_forward.y, 0 ), Vector( pev->velocity.x, pev->velocity.y, 0 ) );
	if (flDir < 0)
		flSpeed = -flSpeed;

	float flDist = DotProduct( m_posDesired - vecEst, gpGlobals->v_forward );

	// float flSlip = DotProduct( pev->velocity, gpGlobals->v_right );
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
	if (flDist > 0 && flSpeed < m_flGoalSpeed /* && flSpeed < flDist */ && pev->angles.x + pev->avelocity.x > -40)
	{
		// ALERT( at_console, "F " );
		// lean forward
		pev->avelocity.x -= 12.0;
	}
	else if (flDist < 0 && flSpeed > -50 && pev->angles.x + pev->avelocity.x  < 20)
	{
		// ALERT( at_console, "B " );
		// lean backward
		pev->avelocity.x += 12.0;
	}
	else if (pev->angles.x + pev->avelocity.x > 0)
	{
		// ALERT( at_console, "f " );
		pev->avelocity.x -= 4.0;
	}
	else if (pev->angles.x + pev->avelocity.x < 0)
	{
		// ALERT( at_console, "b " );
		pev->avelocity.x += 4.0;
	}

	// make rotor, engine sounds
	if (m_iSoundState == 0 && !(pev->spawnflags & SF_MONSTER_GAG))
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "flier/ap_rotor2.wav", 1.0, 0.3, 0, 110 );

		m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions
	}
	else
	{
		CBaseEntity *pPlayer = NULL;

		pPlayer = UTIL_FindEntityByClassname( NULL, "player" );
		// UNDONE: this needs to send different sounds to every player for multiplayer.
		if (pPlayer)
		{

			float pitch = DotProduct( pev->velocity - pPlayer->pev->velocity, (pPlayer->pev->origin - pev->origin).Normalize() );

			pitch = (int)(100 + pitch / 50.0);

			if (pitch > 250)
				pitch = 250;
			if (pitch < 50)
				pitch = 50;
			if (pitch == 100)
				pitch = 101;

			float flVol = (m_flForce / 100.0) + .1;
			if (flVol > 1.0)
				flVol = 1.0;

			if ( !(pev->spawnflags & SF_MONSTER_GAG) )
			{
				EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "flier/ap_rotor2.wav", 1.0, 0.3, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch);
				EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "flier/ap_whine1.wav", flVol, 0.2, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch);

				// the wounded flier's engine
				if ( pev->health < 0 )
					EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "flier/flyer_sick.wav", flVol, 0.2, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch);
			}
		}
	}
}


// a new burst of 30 shots, two seconds from now
void CRustFlier :: ResetStrafe( void )
{
	m_flNextShot = gpGlobals->time + 0.3;
	m_tNextStrafe = gpGlobals->time + 2;
	m_iBullets = 30;
}


BOOL CRustFlier :: CanStrafe( void )
{
	if ( m_tNextStrafe > gpGlobals->time )
		return FALSE;
	if ( m_flNextShot > gpGlobals->time )
		return FALSE;
	return TRUE;
}


// a rustbit shot from the gun, when the flier faces its enemy
BOOL CRustFlier :: FireGun( void )
{
	Vector posGun, angGun;
	GetAttachment( 0, posGun, angGun );

	if ( m_hEnemy == NULL )
		return FALSE;

	if ( m_hEnemy != NULL && !m_hEnemy->IsAlive() )
		return FALSE;

	Vector vecTarget = m_posTarget - posGun;
	vecTarget = vecTarget.Normalize( );

	UTIL_MakeAimVectors( pev->angles );
	Vector vecForward = gpGlobals->v_forward;
	vecForward = vecForward.Normalize( );

	if ( DotProduct( vecForward, vecTarget ) > 0.9 )
	{
		CRustbitShot::Shoot( pev, posGun, vecTarget, VECTOR_CONE_3DEGREES, 1000 );
		return TRUE;
	}
	return FALSE;
}


// ball lightning from below, at an enemy under the flier
BOOL CRustFlier :: FireBall( void )
{
	Vector posGun, angGun;
	GetAttachment( 1, posGun, angGun );

	if ( m_hEnemy == NULL )
		return FALSE;

	if ( m_hEnemy != NULL && !m_hEnemy->IsAlive() )
		return FALSE;

	Vector vecTarget = m_posTarget - posGun;
	vecTarget = vecTarget.Normalize( );

	UTIL_MakeAimVectors( pev->angles );
	Vector vecUp = gpGlobals->v_up;
	vecUp = vecUp.Normalize( );

	if ( DotProduct( vecUp, vecTarget ) < -0.8 )
	{
		vecTarget = UTIL_VecToAngles( vecTarget );
		vecTarget.x = vecTarget.x * -1;
		CBallLightning::Shoot( posGun, vecTarget, this, 600 );
		SetSequenceByName( "attack" );
		return TRUE;
	}
	return FALSE;
}


// the first use takes the shield down, the next removes the flier
void CRustFlier :: FlierUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_iShieldOn )
	{
		m_iShieldOn = FALSE;
		ALERT( at_console, "Flier sheild down!\n" );
		return;
	}

	ALERT( at_console, "Deleting Flier!\n" );
	PreRemove( );

	if ( m_pFlyball )
	{
		UTIL_Remove( m_pFlyball );
		m_pFlyball = NULL;
	}

	pev->health = -1;
	UTIL_Remove( this );
}


void CRustFlier :: PreRemove( void )
{
	if ( !(pev->spawnflags & SF_MONSTER_GAG) )
	{
		STOP_SOUND( ENT(pev), CHAN_STATIC, "flier/ap_rotor2.wav" );
		STOP_SOUND( ENT(pev), CHAN_STATIC, "flier/ap_whine1.wav" );
		STOP_SOUND( ENT(pev), CHAN_STATIC, "flier/flyer_sick.wav" );
	}

	if ( m_pFlyball )
	{
		UTIL_Remove( m_pFlyball );
		m_pFlyball = NULL;
	}
}


// sic: the flier never takes the damage; only a blow above
// its target damage counts
int CRustFlier :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if ( pevAttacker == pev )
		return 0;

	if ( flDamage > m_fTargetDamage && pev->health > 0 )
	{
		StartCrash( );
	}
	else if ( m_iShieldOn )
	{
		// the shield glows
		m_iShellTime = 0;
		pev->renderfx = kRenderFxGlowShell;
		pev->renderamt = 255;
		pev->rendercolor.x = 150;
		pev->rendercolor.y = 0;
		pev->rendercolor.z = 255;

		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "flier/field_impact1.wav", 1, 0.8, 0, 80 + RANDOM_LONG( 0, 40 ) );
			break;
		case 1:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "flier/field_impact2.wav", 1, 0.8, 0, 80 + RANDOM_LONG( 0, 40 ) );
			break;
		case 2:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "flier/field_impact3.wav", 1, 0.8, 0, 80 + RANDOM_LONG( 0, 40 ) );
			break;
		}
	}

	return 0;
}


void CRustFlier::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	ALERT( at_console, "dam: %f, targetdam: %f\n", flDamage, m_fTargetDamage );

	if ( flDamage > m_fTargetDamage && pev->health > 0 )
	{
		StartCrash( );
		return;
	}

	if ( m_iShieldOn )
	{
		// the shield glows
		m_iShellTime = 0;
		pev->renderfx = kRenderFxGlowShell;
		pev->renderamt = 255;
		pev->rendercolor.x = 150;
		pev->rendercolor.y = 0;
		pev->rendercolor.z = 255;

		UTIL_Ricochet( ptr->vecEndPos, 2.0 );

		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "flier/field_impact1.wav", 1, 0.8, 0, 80 + RANDOM_LONG( 0, 40 ) );
			break;
		case 1:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "flier/field_impact2.wav", 1, 0.8, 0, 80 + RANDOM_LONG( 0, 40 ) );
			break;
		case 2:
			UTIL_EmitAmbientSound( ENT(pev), pev->origin, "flier/field_impact3.wav", 1, 0.8, 0, 80 + RANDOM_LONG( 0, 40 ) );
			break;
		}
	}
}
