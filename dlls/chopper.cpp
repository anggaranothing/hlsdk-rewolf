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
// Gunman's helicopters, made from the apache: the bandits'
// chopper (monster_human_chopper, also monster_apache), which
// strafes the player in bursts of 12 rounds, and the manta
// (monster_manta), which flies its path and drops clusters
// of bombs (grenade_mantabomb) when it is triggered
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "soundent.h"
#include "decals.h"
#include "dmlrocket.h"

class CMantaBomb : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void ) { return CLASS_NONE; }

	static CMantaBomb *CreateBomb( CBaseEntity *pOwner );

	void EXPORT MantaBombTouch( CBaseEntity *pOther );
};
LINK_ENTITY_TO_CLASS( grenade_mantabomb, CMantaBomb );


class CChopper : public CBaseMonster
{
public:
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void Spawn( void );
	void Precache( void );
	int  Classify( void ) { return CLASS_HUMAN_MILITARY; };
	int  BloodColor( void ) { return DONT_BLEED; }
	void Killed( entvars_t *pevAttacker, int iGib );
	void PreRemove( void );
	void GibMonster( void ) {}

	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin + Vector( -300, -300, -172);
		pev->absmax = pev->origin + Vector(300, 300, 8);
	}

	void EXPORT HuntThink( void );
	void EXPORT FlyTouch( CBaseEntity *pOther );
	void EXPORT CrashTouch( CBaseEntity *pOther );
	void EXPORT FallingThink( void );

	void ShowDamage( void );
	void Flight( void );
	void ResetStrafe( void );
	BOOL CanStrafe( void );
	BOOL FireGun( void );

	int  TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);

	float m_tNextStrafe;	// when the next burst may start
	int m_iBullets;			// rounds left in this burst

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

	int m_iDoSmokePuff;

	int m_iChopperFire;		// events/chopperfire.sc
};
LINK_ENTITY_TO_CLASS( monster_apache, CChopper );
LINK_ENTITY_TO_CLASS( monster_human_chopper, CChopper );

TYPEDESCRIPTION	CChopper::m_SaveData[] =
{
	DEFINE_FIELD( CChopper, m_flForce, FIELD_FLOAT ),
	DEFINE_FIELD( CChopper, m_posTarget, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CChopper, m_vecDesired, FIELD_VECTOR ),
	DEFINE_FIELD( CChopper, m_posDesired, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CChopper, m_vecGoal, FIELD_VECTOR ),
	DEFINE_FIELD( CChopper, m_angGun, FIELD_VECTOR ),
	DEFINE_FIELD( CChopper, m_flLastSeen, FIELD_TIME ),
	DEFINE_FIELD( CChopper, m_flPrevSeen, FIELD_TIME ),
	DEFINE_FIELD( CChopper, m_flGoalSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CChopper, m_iDoSmokePuff, FIELD_INTEGER ),
	DEFINE_FIELD( CChopper, m_tNextStrafe, FIELD_FLOAT ),
	DEFINE_FIELD( CChopper, m_iBullets, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CChopper, CBaseMonster );


void CChopper :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/chopper.mdl");
	UTIL_SetSize( pev, Vector( -96, -96, 0 ), Vector( 96, 96, 96 ) );
	UTIL_SetOrigin( pev, pev->origin );

	pev->flags |= FL_MONSTER;
	pev->effects |= EF_INVLIGHT;
	pev->takedamage		= DAMAGE_AIM;
	pev->health			= gSkillData.chopperHealth;

	m_flFieldOfView = -0.7;

	pev->sequence = 0;
	ResetSequenceInfo( );
	pev->frame = RANDOM_LONG(0, 0xFF);

	InitBoneControllers();

	SetThink( &CChopper::HuntThink );
	SetTouch( &CChopper::FlyTouch );
	pev->nextthink = gpGlobals->time + 1.0;

	ResetStrafe( );
}


void CChopper::Precache( void )
{
	PRECACHE_MODEL("models/chopper.mdl");

	m_iSpriteTexture = PRECACHE_MODEL( "sprites/white.spr" );

	PRECACHE_MODEL("models/choppergibs.mdl");
	PRECACHE_MODEL("models/chopperblade.mdl");

	PRECACHE_SOUND("turret/tu_fire1.wav");
	PRECACHE_SOUND("apache/ap_rotor2.wav");
	PRECACHE_SOUND("weapons/explode5.wav");

	m_iExplode	= PRECACHE_MODEL( "sprites/fexplo.spr" );
	m_iBodyGibs = PRECACHE_MODEL( "models/metalplategibs_green.mdl" );

	m_iChopperFire = PRECACHE_EVENT( 1, "events/chopperfire.sc" );
}


void CChopper :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = 0.3;

	STOP_SOUND( ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav" );

	UTIL_SetSize( pev, Vector( -32, -32, -64), Vector( 32, 32, 0) );
	SetThink( &CChopper::FallingThink );
	SetTouch( &CChopper::CrashTouch );
	pev->nextthink = gpGlobals->time + 0.1;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;
	pev->flags &= ~FL_ONGROUND;

	CBaseEntity *pOwner = CBaseEntity::Instance(pev->owner);
	if ( pOwner )
	{
		pOwner->DeathNotice( pev );
	}
}


void CChopper :: PreRemove( void )
{
	STOP_SOUND( ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav" );
}


void CChopper :: FallingThink( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	pev->gravity = 0.3;
	pev->avelocity.y = pev->avelocity.y * 1.2 + 10;
	pev->avelocity.z = pev->avelocity.z * 1.05;

	// smoke
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( pev->origin.x + RANDOM_FLOAT( -30, 30 ));
		WRITE_COORD( pev->origin.y + RANDOM_FLOAT( -30, 30 ));
		WRITE_COORD( pev->origin.z + RANDOM_FLOAT( -30, 30 ));
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 50 ); // scale * 10
		WRITE_BYTE( 10  ); // framerate
	MESSAGE_END();

	Vector vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_BREAKMODEL);

		// position
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );

		// size
		WRITE_COORD( 400 );
		WRITE_COORD( 400 );
		WRITE_COORD( 132 );

		// velocity
		WRITE_COORD( pev->velocity.x );
		WRITE_COORD( pev->velocity.y );
		WRITE_COORD( pev->velocity.z );

		// randomization
		WRITE_BYTE( 50 );

		// Model
		WRITE_SHORT( m_iBodyGibs );	//model id#

		// # of shards
		WRITE_BYTE( 4 );	// let client decide

		// duration
		WRITE_BYTE( 30 );// 3.0 seconds

		// flags

		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

	pev->nextthink = gpGlobals->time + 0.2;
}


void CChopper::FlyTouch( CBaseEntity *pOther )
{
	// bounce if we hit something solid
	if ( pOther->pev->solid == SOLID_BSP)
	{
		TraceResult tr = UTIL_GetGlobalTrace( );

		// UNDONE, do a real bounce
		pev->velocity = pev->velocity + tr.vecPlaneNormal * (pev->velocity.Length() + 200);
	}
}


// sic: crashes into whatever it touches, solid or not
void CChopper::CrashTouch( CBaseEntity *pOther )
{
	Vector vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;

	// big smoke
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z + 512 );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 250 ); // scale * 10
		WRITE_BYTE( 5  ); // framerate
	MESSAGE_END();

	RadiusDamage( pev->origin, pev, pev, 100, CLASS_NONE, DMG_BLAST );

	// gibs
	vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_BREAKMODEL);

		// position
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z + 64);

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
		WRITE_SHORT( m_iBodyGibs );	//model id#

		// # of shards
		WRITE_BYTE( 30 );

		// duration
		WRITE_BYTE( 200 );// 10.0 seconds

		// flags

		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

	// fireball
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_SPRITE );
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z + 256 );
		WRITE_SHORT( m_iExplode );
		WRITE_BYTE( 90 ); // scale * 10
		WRITE_BYTE( 255 ); // brightness
	MESSAGE_END();

	// the rotor flies off flat, then the wreckage
	CBaseMonster *pBlade = SpawnGib( "models/chopperblade.mdl", 2, 0, 0 );
	pBlade->pev->angles = pev->angles;
	pBlade->pev->avelocity.x = 0;
	pBlade->pev->avelocity.z = 0;

	SpawnGib( "models/choppergibs.mdl", 2, 0, 0 );
	SpawnGib( "models/choppergibs.mdl", 2, 0, 0 );
	SpawnGib( "models/choppergibs.mdl", 2, 0, 0 );
	SpawnGib( "models/choppergibs.mdl", 2, 0, 0 );
	SpawnGib( "models/choppergibs.mdl", 2, 1, 1 );
	SpawnGib( "models/choppergibs.mdl", 2, 2, 1 );
	SpawnGib( "models/choppergibs.mdl", 2, 3, 1 );
	SpawnGib( "models/choppergibs.mdl", 2, 4, 0 );
	SpawnGib( "models/choppergibs.mdl", 2, 4, 0 );
	SpawnGib( "models/choppergibs.mdl", 2, 4, 0 );
	SpawnGib( "models/choppergibs.mdl", 2, 4, 0 );
	SpawnGib( "models/choppergibs.mdl", 2, 5, 0 );
	SpawnGib( "models/choppergibs.mdl", 2, 5, 0 );
	SpawnGib( "models/choppergibs.mdl", 2, 5, 0 );
	SpawnGib( "models/choppergibs.mdl", 2, 5, 0 );

	UTIL_EmitAmbientSound( ENT(pev), pev->origin, "weapons/explode5.wav", 1, ATTN_NONE, 0, 120 );

	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}


void CChopper :: HuntThink( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	ShowDamage( );

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

	// if (m_hEnemy == NULL)
	{
		Look( 4092 );
		m_hEnemy = BestVisibleEnemy( );
	}

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

	UTIL_MakeAimVectors( pev->angles );
	Vector vecEst = (gpGlobals->v_up * 800 + pev->velocity).Normalize( );	// sic: not used

	if ( m_iBullets == 0 )
	{
		ResetStrafe( );
	}
	else if ( CanStrafe( ) )
	{
		if ((m_flLastSeen + 1 > gpGlobals->time) && (m_flPrevSeen + 2 < gpGlobals->time))
		{
			if (FireGun( ))
			{
				// slow down if we're fireing
				if (m_flGoalSpeed > 150)
					m_flGoalSpeed = 150;
			}
		}
	}
}


void CChopper :: Flight( void )
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
	if (m_iSoundState == 0)
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav", 1.0, 0.3, 0, 110 );
		// EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_whine1.wav", 0.5, 0.2, 0, 110 );

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

			EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav", 1.0, 0.3, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch);
		}
		// EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_whine1.wav", flVol, 0.2, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch);

		// ALERT( at_console, "%.0f %.2f\n", pitch, flVol );
	}
}


// a new burst of 12 rounds, three seconds from now
void CChopper :: ResetStrafe( void )
{
	m_tNextStrafe = gpGlobals->time + 3;
	m_iBullets = 12;
}


BOOL CChopper :: CanStrafe( void )
{
	if ( m_tNextStrafe > gpGlobals->time )
		return FALSE;
	return TRUE;
}


BOOL CChopper :: FireGun( )
{
	BOOL fPitchOnTarget = FALSE;
	BOOL fYawOnTarget = FALSE;

	UTIL_MakeAimVectors( pev->angles );

	Vector posGun, angGun;
	GetAttachment( 1, posGun, angGun );

	Vector vecTarget = (m_posTarget - posGun).Normalize( );

	Vector vecOut;

	vecOut.x = DotProduct( gpGlobals->v_forward, vecTarget );
	vecOut.y = -DotProduct( gpGlobals->v_right, vecTarget );
	vecOut.z = DotProduct( gpGlobals->v_up, vecTarget );

	Vector angles = UTIL_VecToAngles (vecOut);

	angles.x = -angles.x;
	if (angles.y > 180)
		angles.y = angles.y - 360;
	if (angles.y < -180)
		angles.y = angles.y + 360;
	if (angles.x > 180)
		angles.x = angles.x - 360;
	if (angles.x < -180)
		angles.x = angles.x + 360;

	// turn the gun at most 18 degrees a think; it fires only once it points at the target.
	// sic: flYaw and flPitch are not initialized. When the gun already has the target's
	// yaw it gets garbage; when it has its pitch, retail's code gives it the new yaw.
	float flYaw, flPitch;

	if (angles.y > m_angGun.y)
		flYaw = min( angles.y, m_angGun.y + 18 );
	if (angles.y < m_angGun.y)
		flYaw = max( angles.y, m_angGun.y - 18 );
	if (flYaw == angles.y)
		fYawOnTarget = TRUE;
	m_angGun.y = flYaw;

	if (angles.x > m_angGun.x)
		flPitch = min( angles.x, m_angGun.x + 18 );
	if (angles.x < m_angGun.x)
		flPitch = max( angles.x, m_angGun.x - 18 );
	if (flPitch == angles.x)
		fPitchOnTarget = TRUE;
	m_angGun.x = flPitch;

	m_angGun.y = SetBoneController( 0, m_angGun.y );
	m_angGun.x = SetBoneController( 1, m_angGun.x );

	if ( fPitchOnTarget && fYawOnTarget )
	{
		PLAYBACK_EVENT_FULL( FEV_RELIABLE, edict(), m_iChopperFire, 0.0, (float *)&posGun, (float *)&vecTarget, 0.0, 0.0, 0, 0, 0, 0 );
		FireBullets( 1, posGun, vecTarget, VECTOR_CONE_8DEGREES, 8192, BULLET_PLAYER_9MM, 0 );
		m_iBullets--;
		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/hks2.wav", 1, 0.8 );
		return TRUE;
	}
	return FALSE;
}


void CChopper :: ShowDamage( void )
{
	if (m_iDoSmokePuff > 0 || RANDOM_LONG(0,99) > pev->health)
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z - 32 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( RANDOM_LONG(0,9) + 10 ); // scale * 10
			WRITE_BYTE( 12 ); // framerate
		MESSAGE_END();
	}
	if (m_iDoSmokePuff > 0)
		m_iDoSmokePuff--;
}


int CChopper :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if (pevInflictor->owner == edict())
		return 0;

	return CBaseEntity::TakeDamage(  pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_NEVERGIB );
}



void CChopper::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );

	if ( UTIL_PercentChance( 10 ) )
		m_iDoSmokePuff = 3 + (flDamage / 5.0);
}


//=========================================================
// monster_manta: flies its path at a steady 300 and drops
// three bombs, half a second apart, each time it is used
//=========================================================
class CMantaShip : public CChopper
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void ) { return CLASS_NONE; }
	void PreRemove( void );

	void Flight( void );

	void EXPORT MantaThink( void );
	void EXPORT DropBomb( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	float m_flNextBomb;		// when to drop the next bomb, 0 when not bombing
	int m_iBombs;			// bombs dropped in this run
	Vector m_vecDest;		// the first path entity's origin
	float m_flUnused[4];	// never touched
};
LINK_ENTITY_TO_CLASS( monster_manta, CMantaShip );


void CMantaShip :: Spawn( void )
{
	CChopper::Spawn( );

	SetThink( &CMantaShip::MantaThink );
	pev->takedamage = DAMAGE_NO;
	SetUse( &CMantaShip::DropBomb );

	SET_MODEL(ENT(pev), "models/mantadock.mdl");
	UTIL_SetSize( pev, Vector( -96, -96, 0 ), Vector( 96, 96, 96 ) );
	UTIL_SetOrigin( pev, pev->origin );

	m_flNextBomb = 0;
	m_iBombs = 0;

	m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
	if ( m_pGoalEnt )
	{
		m_vecDest = m_pGoalEnt->pev->origin;
		UTIL_MakeAimVectors( m_pGoalEnt->pev->angles );
		Vector vecDir = m_vecDest - pev->origin;
		pev->velocity = vecDir.Normalize() * 400;
		Vector vecAngles = UTIL_VecToAngles( pev->origin - m_pGoalEnt->pev->origin );
		pev->angles.y = vecAngles.y + 180;
	}

	EMIT_SOUND_DYN( ENT(pev), CHAN_STATIC, "manta/manta_idle2.wav", 1.0, 0.3, SND_CHANGE_PITCH | SND_CHANGE_VOL, 100 );
}


void CMantaShip :: PreRemove( void )
{
	STOP_SOUND( ENT(pev), CHAN_STATIC, "manta/manta_idle2.wav" );
}


void CMantaShip :: Precache( void )
{
	CChopper::Precache( );

	UTIL_PrecacheOther( "grenade_mantabomb" );

	PRECACHE_MODEL( "models/mantadock.mdl" );
	PRECACHE_SOUND( "manta/manta_idle2.wav" );
}


// sic: the chopper's flight model without its rotor sound; nothing calls it
void CMantaShip :: Flight( void )
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


void CMantaShip :: MantaThink( void )
{
	if ( m_flNextBomb != 0 && m_flNextBomb < gpGlobals->time )
	{
		CMantaBomb::CreateBomb( this );

		if ( ++m_iBombs < 3 )
			m_flNextBomb = gpGlobals->time + 0.5;
		else
			m_flNextBomb = 0;
	}

	EMIT_SOUND_DYN( ENT(pev), CHAN_STATIC, "manta/manta_idle2.wav", 1.0, 0.3, SND_CHANGE_PITCH | SND_CHANGE_VOL, 100 );

	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_pGoalEnt == NULL && !FStringNull(pev->target) )// this monster has a target
	{
		m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
		if (m_pGoalEnt)
		{
			m_posDesired = m_pGoalEnt->pev->origin;
			UTIL_MakeAimVectors( m_pGoalEnt->pev->angles );
			m_vecGoal = gpGlobals->v_forward;
			Vector vecDir = m_pGoalEnt->pev->origin - pev->origin;
			pev->velocity = vecDir.Normalize() * 300;
			Vector vecAngles = UTIL_VecToAngles( pev->origin - m_pGoalEnt->pev->origin );
			pev->angles.y = vecAngles.y + 180;
		}
	}

	// generic speed up
	if (m_flGoalSpeed < 600)
		m_flGoalSpeed += 10;

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
				Vector vecDir = m_pGoalEnt->pev->origin - pev->origin;
				pev->velocity = vecDir.Normalize() * 300;
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

	UTIL_MakeAimVectors( pev->angles );
	Vector vecEst = (gpGlobals->v_up * 800 + pev->velocity).Normalize( );

	// turn to face the path entity, faster the more it is off
	if ( m_pGoalEnt )
	{
		Vector vecAngles = UTIL_VecToAngles( pev->origin - m_pGoalEnt->pev->origin );
		vecAngles.y += 180;

		if ( pev->angles.y < 0 )
			pev->angles.y *= -1;

		float flDiff = pev->angles.y - vecAngles.y;
		if ( flDiff < 0 )
			flDiff *= -1;

		if ( flDiff > 165 )
		{
			pev->angles.y = vecAngles.y;
		}
		else if ( flDiff > 100 )
		{
			if ( pev->angles.y < vecAngles.y )
				pev->avelocity.y = 120;
			else
				pev->avelocity.y = -100;	// sic
		}
		else if ( flDiff > 40 )
		{
			if ( pev->angles.y < vecAngles.y )
				pev->avelocity.y = 37;
			else
				pev->avelocity.y = -37;
		}
		else if ( flDiff > 15 )
		{
			if ( pev->angles.y < vecAngles.y )
				pev->avelocity.y = 12;
			else
				pev->avelocity.y = -12;
		}
		else if ( flDiff > 5 )
		{
			if ( pev->angles.y < vecAngles.y )
				pev->avelocity.y = 1;
			else
				pev->avelocity.y = -1;
		}
		else
		{
			pev->avelocity = g_vecZero;
		}
	}

	vecEst = pev->velocity;	// sic: unused
}


void CMantaShip :: DropBomb( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_flNextBomb = gpGlobals->time;
	m_iBombs = 0;
}


//=========================================================
// grenade_mantabomb: falls with a whistle and bursts into
// three DML cluster bombs
//=========================================================
void CMantaBomb :: Spawn( void )
{
	CGrenade::Spawn( );
	Precache( );

	SET_MODEL( ENT(pev), "models/chopperbomb.mdl" );

	SetThink( &CMantaBomb::TumbleThink );
	SetTouch( &CMantaBomb::MantaBombTouch );

	pev->takedamage = DAMAGE_NO;
	pev->gravity = 1.0;
	pev->dmg = 240;

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:	EMIT_SOUND( ENT(pev), CHAN_BODY, "manta/bomb_falls1.wav", 1, 1.0 ); break;
	case 1:	EMIT_SOUND( ENT(pev), CHAN_BODY, "manta/bomb_falls2.wav", 1, 1.0 ); break;
	case 2:	EMIT_SOUND( ENT(pev), CHAN_BODY, "manta/bomb_falls3.wav", 1, 1.0 ); break;
	}
}


void CMantaBomb :: Precache( void )
{
	PRECACHE_MODEL( "models/chopperbomb.mdl" );

	PRECACHE_SOUND( "manta/bomb_falls1.wav" );
	PRECACHE_SOUND( "manta/bomb_falls2.wav" );
	PRECACHE_SOUND( "manta/bomb_falls3.wav" );

	PRECACHE_SOUND( "manta/carpetbomb1.wav" );
	PRECACHE_SOUND( "manta/carpetbomb2.wav" );
	PRECACHE_SOUND( "manta/carpetbomb3.wav" );
}


// drops a bomb from pOwner, moving as it moves
CMantaBomb *CMantaBomb :: CreateBomb( CBaseEntity *pOwner )
{
	CMantaBomb *pBomb = GetClassPtr( (CMantaBomb *)NULL );

	if ( pBomb == NULL )
		return NULL;

	pBomb->Spawn( );

	pBomb->pev->origin = pOwner->pev->origin;
	pBomb->pev->angles = pOwner->pev->angles;
	pBomb->pev->velocity = pOwner->pev->velocity;
	pBomb->pev->owner = pOwner->edict( );

	UTIL_SetOrigin( pBomb->pev, pOwner->pev->origin );

	return pBomb;
}


void CMantaBomb :: MantaBombTouch( CBaseEntity *pOther )
{
	TraceResult tr;
	Vector		vecSpot;// trace starts here!

	STOP_SOUND( edict(), CHAN_BODY, "manta/bomb_falls1.wav" );
	STOP_SOUND( edict(), CHAN_BODY, "manta/bomb_falls2.wav" );
	STOP_SOUND( edict(), CHAN_BODY, "manta/bomb_falls3.wav" );

	pev->enemy = pOther->edict();

	vecSpot = pev->origin - pev->velocity.Normalize() * 32;
	UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, ENT(pev), &tr );

	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible

	pev->takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	pev->origin = pev->origin + tr.vecPlaneNormal * 20;

	int iContents = UTIL_PointContents ( pev->origin );
	Vector vecPull = tr.vecPlaneNormal * 50;

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( pev->origin.x );	// Send to PAS because of the sound
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( RandomExplosionSprite() );
		WRITE_BYTE( 35 ); // scale * 10
		WRITE_BYTE( 20 ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	UTIL_MakeVectors( pev->angles );

	if ( RANDOM_LONG( 0, 1 ) )
		UTIL_DecalTrace( &tr, DECAL_SCORCH1 );
	else
		UTIL_DecalTrace( &tr, DECAL_SCORCH2 );

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, BIG_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner = pev->owner ? VARS( pev->owner ) : NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	RadiusDamage ( pev, pevOwner, pev->dmg, CLASS_NONE, DMG_BLAST );

	// the clusters fly up and out from the bomb
	pev->angles.x = 0;
	pev->angles.y = 0;
	pev->angles.z = 1;
	UTIL_MakeVectors( pev->angles );
	Vector vecForward = gpGlobals->v_forward;
	Vector vecEnd = pev->origin + vecForward * 2048;

	for ( int i = 0; i < 3; i++ )
	{
		CClusterGrenade *pCluster = CClusterGrenade::CreateCluster( );

		pCluster->pev->owner = pev->owner;	// sic: cleared above
		pCluster->pev->velocity = pev->angles * RANDOM_LONG( 200, 400 ) + gpGlobals->v_right * RANDOM_LONG( -400, 400 );
		pCluster->pev->origin = pev->origin + vecForward * RANDOM_LONG( 5, 20 ) + gpGlobals->v_right * RANDOM_LONG( -15, 15 );
		pCluster->m_iExplosion = 1;
		pCluster->pev->dmg = 200;
	}

	pev->effects |= EF_NODRAW;
	pev->movetype = MOVETYPE_NONE;
	pev->velocity = g_vecZero;

	UTIL_Remove( this );
}
