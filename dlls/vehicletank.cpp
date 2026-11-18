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
// the drivable tank: vehicle_tank drives, and its brush parts
// vehicle_tank_body, _turret and _barrel follow it; the shells
// and rockets it fires, and the triggers that react to it
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "effects.h"
#include "soundent.h"
#include "gamerules.h"
#include "plane.h"
#include "animation.h"
#include "decals.h"
#include "vehicletank.h"
#include "tankshots.h"

extern int gmsgTank;
extern int gmsgTankBody;
extern int gmsgTurretPos;

// sic: retail's vehicletank.obj has 4 bytes of .bss before MoveTank's
// s_iStuck that nothing reads or writes (name ours)
static int s_iUnknown;

//=========================================================
// vehicle_tank
//=========================================================
LINK_ENTITY_TO_CLASS( vehicle_tank, CVehicleTank );

TYPEDESCRIPTION	CVehicleTank::m_SaveData[] =
{
	DEFINE_FIELD( CVehicleTank, m_pPlayer, FIELD_CLASSPTR ),
	DEFINE_FIELD( CVehicleTank, m_iVehicleID, FIELD_INTEGER ),
	DEFINE_FIELD( CVehicleTank, m_vecLastValidPos, FIELD_VECTOR ),
	DEFINE_FIELD( CVehicleTank, m_vecLastValidAng, FIELD_VECTOR ),
	DEFINE_FIELD( CVehicleTank, m_pBarrelBSP, FIELD_CLASSPTR ),
	DEFINE_FIELD( CVehicleTank, m_pTurretBSP, FIELD_CLASSPTR ),
	DEFINE_FIELD( CVehicleTank, m_pBodyBSP, FIELD_CLASSPTR ),
	DEFINE_FIELD( CVehicleTank, m_flSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CVehicleTank, m_fYawAccel, FIELD_FLOAT ),
	DEFINE_FIELD( CVehicleTank, m_flTimeHolder, FIELD_FLOAT ),
	DEFINE_FIELD( CVehicleTank, m_bCollisionLastFrame, FIELD_BOOLEAN ),
	DEFINE_FIELD( CVehicleTank, m_vecNormalLastFrame, FIELD_VECTOR ),
	DEFINE_FIELD( CVehicleTank, m_flAccelTime, FIELD_TIME ),
	DEFINE_FIELD( CVehicleTank, m_fNextShellAttack, FIELD_TIME ),
	DEFINE_FIELD( CVehicleTank, m_fNextRocketAttack, FIELD_TIME ),
	DEFINE_FIELD( CVehicleTank, m_fNextMGunAttack, FIELD_TIME ),
	DEFINE_FIELD( CVehicleTank, m_iGasState, FIELD_INTEGER ),
	DEFINE_FIELD( CVehicleTank, m_sTurretHUDPos, FIELD_FLOAT ),
	DEFINE_FIELD( CVehicleTank, m_iVehicleLength, FIELD_INTEGER ),
	DEFINE_FIELD( CVehicleTank, m_iVehicleWidth, FIELD_INTEGER ),
	DEFINE_FIELD( CVehicleTank, m_flNextDebris, FIELD_TIME ),
	DEFINE_FIELD( CVehicleTank, m_flNextOjectionDeniedSound, FIELD_TIME ),
	DEFINE_FIELD( CVehicleTank, m_bFirstTimeInTank, FIELD_BOOLEAN ),
	DEFINE_FIELD( CVehicleTank, m_flNextHullCheck, FIELD_TIME ),
	DEFINE_FIELD( CVehicleTank, m_fTankSoundAdj, FIELD_FLOAT ),
	DEFINE_FIELD( CVehicleTank, m_flLastSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CVehicleTank, m_flNextBounceMove, FIELD_TIME ),
	DEFINE_FIELD( CVehicleTank, m_iTankHealth, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CVehicleTank, CBaseMonster );

void CVehicle::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "vehicle_id" ) )
	{
		m_iVehicleID = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "vehicle_volume" ) )
	{
		m_fTankSoundAdj = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "vehicle_volume" ) )	// sic: never reached, so m_iTankHealth keeps 1000
	{
		m_iTankHealth = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		pkvd->fHandled = FALSE;
}

CVehicleTank::CVehicleTank( void )
{
	m_pBarrelBSP = NULL;
	m_pTurretBSP = NULL;
	m_pBodyBSP = NULL;
	m_fSequenceFinished = TRUE;
	m_iVehicleID = 1;
	m_bFirstTimeInTank = TRUE;
	m_iGasState = TANK_GAS_FULL;
	m_fTankSoundAdj = 1.0;
	m_iTankHealth = 1000;
}

void CVehicleTank::Precache( void )
{
	PRECACHE_MODEL( "models/tank.mdl" );
	PRECACHE_MODEL( "models/rocket.mdl" );
	PRECACHE_SOUND( "weapons/rocket1.wav" );
	PRECACHE_SOUND( "tank/scrapewall.wav" );
	PRECACHE_SOUND( "tank/glug.wav" );
	PRECACHE_SOUND( "tank/startup.wav" );
	PRECACHE_SOUND( "tank/turn.wav" );
	PRECACHE_SOUND( "tank/ramwall.wav" );
	PRECACHE_SOUND( "tank/outofgas.wav" );
	PRECACHE_SOUND( "tank/powerdown.wav" );
	PRECACHE_SOUND( "tank/engineidle.wav" );
	PRECACHE_SOUND( "weapons/hks1.wav" );
	PRECACHE_SOUND( "weapons/hks2.wav" );
	PRECACHE_SOUND( "weapons/hks3.wav" );
	UTIL_PrecacheOther( "vehicle_tank_shell" );
	UTIL_PrecacheOther( "vehicle_tank_rocket" );

	m_usTankMGuns = PRECACHE_EVENT( 1, "events/tankmguns.sc" );
	m_iRockGibs = PRECACHE_MODEL( "models/rockgibs.mdl" );
	m_iMetalGibs = PRECACHE_MODEL( "models/metalgibs.mdl" );
	m_iWoodGibs = PRECACHE_MODEL( "models/woodgibs.mdl" );

	m_iTankMsgs = 0;
	m_bStopOnThink = TRUE;
}

void CVehicleTank::Spawn( void )
{
	Precache();

	pev->movetype = MOVETYPE_FLYMISSILE;
	pev->solid = SOLID_TRIGGER;
	pev->takedamage = DAMAGE_NO;

	SET_MODEL( ENT( pev ), "models/tank.mdl" );
	UTIL_SetSize( pev, Vector( -64, -64, 12 ), Vector( 64, 64, 48 ) );
	UTIL_SetOrigin( pev, pev->origin );

	pev->nextthink = gpGlobals->time + 0.1;
	SetThink( &CVehicleTank::BuildVehicleThink );

	m_pPlayer = NULL;
	m_pBarrelBSP = NULL;
	m_pTurretBSP = NULL;
	m_pBodyBSP = NULL;
	m_flTimeHolder = gpGlobals->time;
	m_vecNormalLastFrame = Vector( 0, 0, 0 );
	m_bCollisionLastFrame = FALSE;
	m_fNextShellAttack = gpGlobals->time;
	m_fNextRocketAttack = gpGlobals->time;
	m_fNextMGunAttack = gpGlobals->time;
	m_flNextRamSound = gpGlobals->time;
	m_flNextDebris = gpGlobals->time;
	m_flNextOjectionDeniedSound = gpGlobals->time;
	m_flNextHullCheck = gpGlobals->time;
	m_iGasState = TANK_GAS_FULL;
	m_sTurretHUDPos = 0;
	m_flSpeed = 0;
	m_fYawAccel = 0;
	m_flAccelTime = gpGlobals->time;
	pev->velocity.x = pev->velocity.y = 0;

	m_iVehicleLength = 116;
	m_iVehicleWidth = 78;

	if ( g_pGameRules->IsMultiplayer() )
	{
		pev->absmin = pev->origin - Vector( 400, 400, 0 );
		pev->absmax = pev->origin + Vector( 400, 400, 400 );
		pev->health = 1000;
		pev->takedamage = DAMAGE_AIM;
		pev->health = m_iTankHealth;
	}
}

int CVehicleTank::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( g_pGameRules->IsMultiplayer() && m_pPlayer )
	{
		if ( bitsDamageType & DMG_BULLET )
			flDamage /= 4;
		if ( bitsDamageType & DMG_BLAST )
			flDamage /= 2;

		pev->health -= flDamage;
		if ( pev->health < 0 )
		{
			// the tank is wrecked: throw the driver out and crush him
			pev->health = m_iTankHealth;
			CBasePlayer *pPlayer = m_pPlayer;
			EjectPlayer();
			return pPlayer->TakeDamage( pev, pev, 1000000, DMG_CRUSH );
		}
	}
	return 0;
}

//=========================================================
// BuildVehicleThink - links the brush parts with the same
// vehicle_id, then starts driving
//=========================================================
void CVehicleTank::BuildVehicleThink( void )
{
	int iLinked = 0;

	while ( ( m_pBarrelBSP = (CVehicleTankBSP *)UTIL_FindEntityByClassname( m_pBarrelBSP, "vehicle_tank_barrel" ) ) != NULL )
	{
		if ( m_pBarrelBSP->m_iVehicleID == m_iVehicleID )
		{
			ALERT( at_console, "VehicleID %d Linked BarrelBSP\n", m_iVehicleID );
			iLinked = 1;
			break;
		}
	}

	while ( ( m_pTurretBSP = (CVehicleTankBSP *)UTIL_FindEntityByClassname( m_pTurretBSP, "vehicle_tank_turret" ) ) != NULL )
	{
		if ( m_pTurretBSP->m_iVehicleID == m_iVehicleID )
		{
			ALERT( at_console, "VehicleID %d Linked TurretBSP\n", m_iVehicleID );
			iLinked++;
			break;
		}
	}

	while ( ( m_pBodyBSP = (CVehicleTankBSP *)UTIL_FindEntityByClassname( m_pBodyBSP, "vehicle_tank_body" ) ) != NULL )
	{
		if ( m_pBodyBSP->m_iVehicleID == m_iVehicleID )
		{
			ALERT( at_console, "VehicleID %d Linked BodyBSP\n", m_iVehicleID );
			iLinked++;
			break;
		}
	}

	if ( iLinked == 3 )
	{
		pev->nextthink = gpGlobals->time + 0.1;

		// restored with a driver: find him again
		if ( !m_pPlayer )
		{
			m_pPlayer = NULL;
			SetThink( &CVehicleTank::WithoutPlayerThink );
			SetUse( &CVehicleTank::UseVehicle );
		}
		else
		{
			m_pPlayer = (CBasePlayer *)UTIL_FindEntityByClassname( NULL, "player" );
			if ( m_pPlayer )
			{
				m_pPlayer->m_pVehicle = this;
				SetThink( &CVehicleTank::WithPlayerThink );
				SetUse( NULL );
			}
			else
			{
				SetThink( &CVehicleTank::WithoutPlayerThink );
				SetUse( &CVehicleTank::UseVehicle );
			}
		}

		UTIL_SetOrigin( m_pBodyBSP->pev, pev->origin );
		UTIL_SetOrigin( m_pTurretBSP->pev, pev->origin + Vector( 0, 0, 70 ) );
		UTIL_SetOrigin( m_pBarrelBSP->pev, pev->origin + Vector( 0, 0, 82 ) );

		m_pBarrelBSP->pev->angles = pev->angles;
		m_pTurretBSP->pev->angles = m_pBarrelBSP->pev->angles;
		m_pBodyBSP->pev->angles = m_pTurretBSP->pev->angles;

		m_pBarrelBSP->pev->owner = ENT( pev );
		m_pTurretBSP->pev->owner = m_pBarrelBSP->pev->owner;
		m_pBodyBSP->pev->owner = m_pTurretBSP->pev->owner;

		void *pmodel = GET_MODEL_PTR( ENT( pev ) );
		SetController( pmodel, pev, 3, m_pBarrelBSP->pev->angles.x );
	}
	else
	{
		pev->nextthink = gpGlobals->time + 0.5;
		ALERT( at_console, "WARNING: VehicleID %d did NOT Link ALL Parts\n", m_iVehicleID );
	}
}

//=========================================================
// WithoutPlayerThink - parked: keep the parts in place
//=========================================================
void CVehicleTank::WithoutPlayerThink( void )
{
	pev->nextthink = gpGlobals->time;

	if ( !m_pBodyBSP )
	{
		SetThink( &CVehicleTank::BuildVehicleThink );
		return;
	}

	Vector vecUp;
	UTIL_MakeVectorsPrivate( m_pBodyBSP->pev->angles, NULL, NULL, vecUp );

	UTIL_SetOrigin( m_pBodyBSP->pev, pev->origin );
	UTIL_SetOrigin( m_pTurretBSP->pev, pev->origin + vecUp * 70 );
	UTIL_SetOrigin( m_pBarrelBSP->pev, pev->origin + vecUp * 82 );
}

//=========================================================
// WithPlayerThink - driving
//=========================================================
void CVehicleTank::WithPlayerThink( void )
{
	int iState = 1;

	pev->nextthink = gpGlobals->time;

	if ( m_bStopOnThink == TRUE )
	{
		m_flSpeed = 0;
		SetVelocity( g_vecZero );
		m_bStopOnThink = FALSE;
	}

	if ( m_iTankMsgs < 2 )
	{
		if ( m_iGasState == TANK_GAS_EMPTY )
			iState = 2;

		MESSAGE_BEGIN( MSG_ONE, gmsgTank, NULL, m_pPlayer->pev );
			WRITE_SHORT( iState );
		MESSAGE_END();
		m_iTankMsgs++;
	}

	if ( m_iGasState == TANK_GAS_REFUEL )
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_STATIC, "tank/startup.wav", m_fTankSoundAdj, 0.8 );
		m_iGasState = TANK_GAS_FULL;
		m_pPlayer->pev->view_ofs = g_vecZero;
	}

	pev->effects |= EF_NOINTERP;
	StudioFrameAdvance( );

	if ( m_fSequenceFinished )
		SetTankAnim( TANK_ENGINE );

	if ( !m_pBodyBSP )
	{
		SetThink( &CVehicleTank::BuildVehicleThink );
		return;
	}

	if ( !TankControls() )
		return;

	CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin, 400, 0.1 );

	float flInterval = gpGlobals->time - m_flTimeHolder;
	m_flTimeHolder = gpGlobals->time;

	MoveTank( flInterval );
	UpdatePlayer( flInterval );

	if ( gpGlobals->time > m_flNextHullCheck )
	{
		m_flNextHullCheck = gpGlobals->time + 0.1;
		HullCheck();
	}

	pev->angles = m_pBodyBSP->pev->angles;
	pev->angles.x *= -1;
	pev->avelocity = m_pBodyBSP->pev->avelocity;
	pev->avelocity.x *= -1;
	SetVelocity( pev->velocity );

	// the parts move with velocities that carry them onto the tank
	m_pBodyBSP->pev->velocity = pev->origin - m_pBodyBSP->pev->origin + m_pBodyBSP->pev->velocity;
	m_pTurretBSP->pev->velocity = pev->origin + Vector( 0, 0, 70 ) - m_pTurretBSP->pev->origin + m_pTurretBSP->pev->velocity;
	m_pBarrelBSP->pev->velocity = pev->origin + Vector( 0, 0, 82 ) - m_pBarrelBSP->pev->origin + m_pBarrelBSP->pev->velocity;
}

//=========================================================
// TankControls - the driver's buttons; returns 0 when the
// driver got out
//=========================================================
int CVehicleTank::TankControls( void )
{
	if ( ( m_pPlayer->pev->button & IN_JUMP ) && CanExit() )
	{
		EjectPlayer();
		return 0;
	}

	if ( ( m_pPlayer->pev->button & IN_ATTACK ) && !( pev->spawnflags & SF_TANK_NOCANNONS ) && gpGlobals->time > m_fNextShellAttack )
	{
		pev->effects |= EF_MUZZLEFLASH;
		SetTankAnim( TANK_TURRET );
		FireShell();
		m_fNextShellAttack = gpGlobals->time + 0.8;
	}

	if ( ( m_pPlayer->pev->button & IN_ATTACK2 ) && !( pev->spawnflags & SF_TANK_NOCANNONS ) && gpGlobals->time > m_fNextRocketAttack )
	{
		FireRocket();
		m_fNextRocketAttack = gpGlobals->time + 3.0;
	}

	if ( m_flAccelTime < gpGlobals->time )
	{
		m_flAccelTime = gpGlobals->time + 0.1;

		float flSpeed = ( ( m_flSpeed < 0 ? m_flSpeed * -1 : m_flSpeed ) / 250 );

		if ( m_iGasState )
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_VOICE, "tank/engineidle.wav", ( ( flSpeed * 0.3 ) + 0.3 ) * m_fTankSoundAdj, 0.8, SND_CHANGE_VOL, PITCH_NORM );
		else
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_VOICE, "tank/engineidle.wav", ( flSpeed * 0.3 ) * m_fTankSoundAdj, 0.8, SND_CHANGE_VOL, PITCH_NORM );

		float flTurn = m_fYawAccel / 100;
		if ( flTurn < 0 )
			flTurn *= -1;
		if ( flTurn < flSpeed )
			flTurn = flSpeed;
		if ( flTurn > 1 )
			flTurn = 1;
		EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_BODY, "tank/turn.wav", flTurn * m_fTankSoundAdj, 0.8, SND_CHANGE_VOL, PITCH_NORM );

		if ( m_bCollisionLastFrame )
		{
			if ( m_flSpeed > 0 )
				EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "tank/scrapewall.wav", flSpeed * m_fTankSoundAdj, 0.8, SND_CHANGE_VOL | SND_CHANGE_PITCH, 60 - (int)( flSpeed * -40 ) );
			else if ( m_flSpeed < 0 )
				EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "tank/scrapewall.wav", flSpeed * m_fTankSoundAdj, 0.8, SND_CHANGE_VOL | SND_CHANGE_PITCH, 60 - (int)( flSpeed * -40 ) );
		}
		else
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "tank/scrapewall.wav", 0, 0.8, SND_CHANGE_VOL, PITCH_NORM );

		if ( m_fNextMGunAttack < gpGlobals->time )
		{
			if ( ( m_pPlayer->pev->button & IN_DUCK ) && !( pev->spawnflags & SF_TANK_NOMGUNS ) )
			{
				m_fNextMGunAttack = gpGlobals->time + 0.1;
				SetTankAnim( TANK_MINGUNS );
				pev->effects |= EF_MUZZLEFLASH;
				FireMachineGuns();
			}
			else
			{
				SetTankAnim( TANK_ENGINE );
				m_fNextMGunAttack = gpGlobals->time + 0.1;
			}
		}

		if ( m_iGasState == TANK_GAS_FULL )
		{
			if ( m_pPlayer->pev->button & IN_FORWARD )
				m_flSpeed += 40;
			if ( m_pPlayer->pev->button & IN_BACK )
				m_flSpeed -= 40;
		}

		if ( !( m_pPlayer->pev->button & ( IN_FORWARD | IN_BACK ) ) || m_iGasState < TANK_GAS_FULL )
		{
			// roll to a stop
			if ( m_flSpeed > 25 )
				m_flSpeed -= 25;
			else if ( m_flSpeed < -25 )
				m_flSpeed += 25;
			else
				m_flSpeed = 0;
		}

		if ( m_flSpeed != 0 )
			m_flLastSpeed = m_flSpeed;

		m_flSpeed = min( m_flSpeed, 250 );
		m_flSpeed = max( m_flSpeed, -250 );

		if ( m_iGasState == TANK_GAS_FULL )
		{
			if ( m_pPlayer->pev->button & IN_MOVELEFT )
				m_fYawAccel += 15;
			if ( m_pPlayer->pev->button & IN_MOVERIGHT )
				m_fYawAccel -= 15;
		}

		if ( !( m_pPlayer->pev->button & ( IN_MOVELEFT | IN_MOVERIGHT ) ) || m_iGasState < TANK_GAS_FULL )
		{
			if ( m_fYawAccel > 25 )
				m_fYawAccel -= 25;
			else if ( m_fYawAccel < -25 )
				m_fYawAccel += 25;
			else
				m_fYawAccel = 0;
		}

		m_fYawAccel = min( m_fYawAccel, 100 );
		m_fYawAccel = max( m_fYawAccel, -100 );
	}

	return 1;
}

//=========================================================
// SetTankAnim - bumps rattle the driver's view and play the
// ram sound
//=========================================================
void CVehicleTank::SetTankAnim( int iAnim )
{
	if ( m_iGasState == TANK_GAS_EMPTY || m_iGasState == TANK_GAS_NONE )
	{
		pev->sequence = TANK_OFFLINE;
		pev->framerate = 0;
		ResetSequenceInfo( );
		return;
	}

	if ( pev->sequence == iAnim )
		return;

	switch ( iAnim )
	{
	case TANK_OFFLINE:
		pev->sequence = TANK_OFFLINE;
		pev->framerate = 0;
		ResetSequenceInfo( );
		break;

	case TANK_ENGINE:
		pev->sequence = TANK_ENGINE;
		ResetSequenceInfo( );
		break;

	case TANK_BUMPFRONT:
	case TANK_BUMPFRONTLEFT:
	case TANK_BUMPFRONTRIGHT:
		if ( m_flNextRamSound < gpGlobals->time )
		{
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_STATIC, "tank/ramwall.wav", ( fabs( (int)( m_flLastSpeed / 250 ) ) * 0.5 + 0.3 ) * m_fTankSoundAdj, 0.8, 0, PITCH_NORM );
			m_fNextMGunAttack = gpGlobals->time + 0.8;
			if ( m_pPlayer )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgTankBody, NULL, m_pPlayer->pev );
					WRITE_SHORT( 1 );
				MESSAGE_END();
			}
			pev->sequence = iAnim;
			pev->frame = 0;
			ResetSequenceInfo( );
			m_flNextRamSound = gpGlobals->time + 2;
		}
		break;

	case TANK_BUMPBACK:
	case TANK_BUMPBACKLEFT:
	case TANK_BUMPBACKRIGHT:
		if ( m_flNextRamSound < gpGlobals->time )
		{
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_STATIC, "tank/ramwall.wav", ( fabs( (int)( m_flLastSpeed / 250 ) ) * 0.5 + 0.3 ) * m_fTankSoundAdj, 0.8, 0, PITCH_NORM );
			m_fNextMGunAttack = gpGlobals->time + 0.8;
			if ( m_pPlayer )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgTankBody, NULL, m_pPlayer->pev );
					WRITE_SHORT( 2 );
				MESSAGE_END();
			}
			pev->sequence = iAnim;
			pev->frame = 0;
			ResetSequenceInfo( );
			m_flNextRamSound = gpGlobals->time + 2;
		}
		break;

	case TANK_BUMPLEFT:
	case TANK_BUMPRIGHT:
		if ( m_flNextRamSound < gpGlobals->time )
		{
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_STATIC, "tank/ramwall.wav", ( fabs( (int)( m_flLastSpeed / 250 ) ) * 0.5 + 0.3 ) * m_fTankSoundAdj, 0.8, 0, PITCH_NORM );
			m_fNextMGunAttack = gpGlobals->time + 0.8;
			pev->sequence = iAnim;
			pev->frame = 0;
			ResetSequenceInfo( );
			m_flNextRamSound = gpGlobals->time + 2;
		}
		break;

	case TANK_MINGUNS:
		m_iMGunSide = 0;
		pev->sequence = TANK_MINGUNS;
		pev->frame = 0;
		ResetSequenceInfo( );
		break;

	case TANK_TURRET:
		pev->sequence = TANK_TURRET;
		ResetSequenceInfo( );
		break;
	}
}

//=========================================================
// MoveTank - turns and tilts the tank to the ground, then drives
//=========================================================
void CVehicleTank::MoveTank( float flInterval )
{
	static int s_iStuck = 0;
	BOOL bRestore = FALSE;

	pev->angles.y = UTIL_AngleNormalize( pev->angles.y );
	m_pBodyBSP->pev->angles.y = UTIL_AngleNormalize( m_pBodyBSP->pev->angles.y );
	m_pTurretBSP->pev->angles.y = UTIL_AngleNormalize( m_pTurretBSP->pev->angles.y );
	m_pBarrelBSP->pev->angles.y = UTIL_AngleNormalize( m_pBarrelBSP->pev->angles.y );

	pev->avelocity.y = m_fYawAccel;

	Vector vecGround = GroundAngles();
	vecGround.x = UTIL_AngleNormalize( vecGround.x );
	vecGround.y = UTIL_AngleNormalize( vecGround.y );

	Vector vecAvel;
	vecAvel = BodyAvelocity( UTIL_AngleDiff( pev->angles.y, vecGround.y ), vecGround );
	pev->avelocity.x = vecAvel.x;
	pev->avelocity.z = vecAvel.z;
	m_pBodyBSP->pev->avelocity.x = pev->avelocity.x * -1;
	m_pBodyBSP->pev->avelocity.y = pev->avelocity.y;
	m_pBodyBSP->pev->avelocity.z = pev->avelocity.z;

	vecAvel = TurretAvelocity( UTIL_AngleDiff( m_pTurretBSP->pev->angles.y, vecGround.y ), vecGround );
	m_pTurretBSP->pev->avelocity.x = vecAvel.x;
	m_pTurretBSP->pev->avelocity.z = vecAvel.z;
	m_pBarrelBSP->pev->avelocity.x = m_pTurretBSP->pev->avelocity.x;
	m_pBarrelBSP->pev->avelocity.z = m_pTurretBSP->pev->avelocity.z;

	if ( m_pBarrelBSP->m_bBlocked || m_pTurretBSP->m_bBlocked || m_pBodyBSP->m_bBlocked )
	{
		SetVelocity( g_vecZero );
		SetAvelocity( g_vecZero );
		bRestore = TRUE;
		m_pBarrelBSP->m_bBlocked = FALSE;
		m_pTurretBSP->m_bBlocked = FALSE;
		m_pBodyBSP->m_bBlocked = FALSE;
	}

	if ( SidesBlocked() )
	{
		if ( ++s_iStuck > 10 )
		{
			s_iStuck = 0;
			SetVelocity( g_vecZero );
			m_flSpeed = 0;
		}
		SetVelocityZ( 0 );
		bRestore = TRUE;
		m_bCollisionLastFrame = TRUE;
	}
	else
		s_iStuck = 0;

	Drive( flInterval );

	// float back to the ground
	int iHeight = (int)( ( vecGround.z - 32 ) * -2 );
	if ( iHeight > 0 )
		SetVelocityZ( ( iHeight / 3 + 1 ) * iHeight );
	else
		SetVelocityZ( ( 1 - iHeight / 3 ) * iHeight );

	m_pBodyBSP->pev->velocity = pev->velocity;

	// the turret and barrel sit on the body, along its up vector
	Vector vecUp;
	UTIL_MakeVectorsPrivate( pev->angles, NULL, NULL, vecUp );
	m_pTurretBSP->pev->velocity = pev->origin + vecUp * 70 - m_pTurretBSP->pev->origin + pev->velocity;
	m_pBarrelBSP->pev->velocity = pev->origin + vecUp * 82 - m_pBarrelBSP->pev->origin + pev->velocity;

	if ( bRestore )
		RestoreLastValid();
	else
		SaveLastValid();

	UpdateControllers();
}

//=========================================================
// Drive - the velocity for this frame: straight along the
// body, sliding along walls, bouncing off what it rams
//=========================================================
void CVehicleTank::Drive( float flInterval )
{
	Vector vecForward;
	Vector vecVelocity;
	Vector vecSlide;
	float flDot;

	UTIL_MakeVectorsPrivate( pev->angles, vecForward, NULL, NULL );

	if ( m_flSpeed < 0 || m_flSpeed > 0 )
	{
		vecVelocity = m_flSpeed * Vector( vecForward.x, vecForward.y, 0 );
	}
	else if ( m_flSpeed != 0 || pev->velocity.Make2D().Length() >= 5 )
	{
		vecVelocity = Vector( vecForward.x, vecForward.y, 0 );
	}
	else
	{
		vecVelocity = g_vecZero;
	}

	m_vecCollision = g_vecZero;
	m_bCanBounce = TRUE;

	if ( CornersBlocked( vecVelocity, flInterval, 18 ) )
	{
		if ( m_bCollisionLastFrame )
		{
			// sic: dead, overwritten below; it gives retail's inlining and x87 orders
			flDot = pev->velocity.x * m_vecNormalLastFrame.x + pev->velocity.y * m_vecNormalLastFrame.y + pev->velocity.z * m_vecNormalLastFrame.z;
			flDot = DotProduct( vecVelocity.Normalize(), m_vecNormalLastFrame );

			if ( pev->avelocity.y != 0 && pev->velocity.Length() < 10 )
			{
				Vector vecPush = PushAway();
				if ( vecPush != g_vecZero )
				{
					// turning in place against a wall: push off it
					vecVelocity = ( pev->origin - m_vecCollision ).Normalize() * 128;
					vecVelocity = vecVelocity + vecPush;
					goto turned;
				}
			}

			if ( m_bCanBounce && flDot >= -0.8 && flDot <= 0.8 && pev->avelocity.y == 0 )
			{
				if ( DotProduct( vecVelocity.Normalize(), m_vecNormalLastFrame ) < 0 )
				{
					// glancing blow: slide along the wall
					vecSlide = Slide( vecVelocity, m_vecNormalLastFrame );
					if ( vecSlide.Length() > 50 && ( m_flSpeed > 50 || m_flSpeed < -50 ) )
					{
						if ( m_flSpeed < 0 )
							vecSlide = vecSlide * -1;
						vecVelocity = vecSlide;

						if ( m_vecCollision != g_vecZero && m_flNextDebris < gpGlobals->time )
						{
							Debris( pev->origin + ( m_vecCollision - pev->origin ).Normalize() * 0.7 * ( m_vecCollision - pev->origin ).Length() );
							m_flNextDebris = gpGlobals->time + 0.2;
						}
						goto turned;
					}
				}
			}

			if ( m_flNextBounceMove < gpGlobals->time )
			{
				// rammed something: bounce back
				Vector vecFwd;
				UTIL_MakeVectorsPrivate( pev->angles, vecFwd, NULL, NULL );

				int iAnim = 0;
				if ( DotProduct( ( m_vecCollision - pev->origin ).Normalize(), vecFwd ) > 0 )
				{
					if ( !( pev->avelocity.y > 15 ) )
						iAnim = TANK_BUMPFRONT;
					else
						iAnim = TANK_BUMPFRONTLEFT;
				}
				else
				{
					if ( !( pev->avelocity.y > 15 ) )
						iAnim = TANK_BUMPBACKLEFT;
					else
						iAnim = TANK_BUMPBACKRIGHT;
				}
				SetTankAnim( iAnim );

				vecVelocity = ( pev->origin - m_vecCollision ).Normalize() * 128;
				vecVelocity.z = 0;
				SetAvelocity( -pev->avelocity );
				m_flSpeed = 0;
				m_fYawAccel = 0;
				m_flNextBounceMove = gpGlobals->time + 2;
			}
turned:
			if ( pev->avelocity.y != 0 && vecVelocity.Length() < 10 )
			{
				Vector vecPush = PushAway();
				if ( vecPush != g_vecZero )
				{
					vecVelocity = ( pev->origin - m_vecCollision ).Normalize() * 128;
					vecVelocity = vecPush + vecVelocity;
				}
			}
		}
	}
	else
		m_bCollisionLastFrame = FALSE;

	pev->velocity = vecVelocity;
}

//=========================================================
// UpdatePlayer - keeps the driver in the chase camera spot
// behind and above the tank
//=========================================================
int CVehicleTank::UpdatePlayer( float flInterval )
{
	TraceResult tr;

	if ( !m_pPlayer )
		return 0;

	m_pTurretBSP->pev->angles.y = m_pPlayer->pev->v_angle.y;
	m_pBarrelBSP->pev->angles.y = m_pPlayer->pev->v_angle.y;

	if ( m_pPlayer->pev->v_angle.x > 17 )
	{
		m_pPlayer->pev->fixangle = TRUE;
		m_pPlayer->pev->angles.x = 17;
		m_pPlayer->pev->v_angle.x = 17;
	}
	m_pBarrelBSP->pev->angles.x = m_pPlayer->pev->v_angle.x + m_pTurretBSP->pev->angles.x;

	UTIL_MakeVectors( m_pBarrelBSP->pev->angles );

	Vector vecEnd = pev->origin + gpGlobals->v_up * 140 + gpGlobals->v_forward * -250;
	Vector vecStart = pev->origin + gpGlobals->v_up * 140;

	UTIL_TraceLine( vecStart, vecEnd, ignore_monsters, NULL, &tr );

	m_pPlayer->pev->velocity = tr.vecEndPos + ( vecStart - vecEnd ).Normalize() * 50 - m_pPlayer->pev->origin;
	UTIL_SetOrigin( m_pPlayer->pev, tr.vecEndPos + ( vecStart - vecEnd ).Normalize() * 50 );

	return 1;
}

//=========================================================
// HullCheck - crushes whatever is inside the tank's box
//=========================================================
void CVehicleTank::HullCheck( void )
{
	CPlane planeFront;
	CPlane planeBack;
	CPlane planeLeft;
	CPlane planeRight;
	CPlane planeTop;
	CPlane planeBottom;
	Vector vecRight, vecLeft, vecFront, vecBack, vecTop, vecBottom;

	UTIL_MakeVectors( m_pBodyBSP->pev->angles );

	float flExtra = pev->velocity.Length() / 5 + 16;

	vecRight = pev->origin + gpGlobals->v_right * ( VehicleWidth() + flExtra );
	vecLeft = pev->origin - gpGlobals->v_right * ( VehicleWidth() + flExtra );
	vecFront = pev->origin + gpGlobals->v_forward * ( VehicleLength() + flExtra );
	vecBack = pev->origin - gpGlobals->v_forward * ( VehicleLength() + flExtra );
	vecTop = pev->origin + gpGlobals->v_up * 24;
	vecBottom = pev->origin - Vector( 0, 0, 36 );

	planeFront.InitializePlane( -gpGlobals->v_forward, vecFront );
	planeBack.InitializePlane( gpGlobals->v_forward, vecBack );
	planeRight.InitializePlane( -gpGlobals->v_right, vecRight );
	planeLeft.InitializePlane( gpGlobals->v_right, vecLeft );
	planeTop.InitializePlane( -gpGlobals->v_up, vecBack );		// sic: vecTop was meant
	planeBottom.InitializePlane( gpGlobals->v_forward, vecBack );	// sic: v_up and vecBottom were meant

	float flRadius = (int)( ( VehicleWidth() + VehicleLength() ) * 1.5 );

	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, flRadius ) ) != NULL )
	{
		if ( pEntity == m_pBodyBSP || pEntity == m_pTurretBSP || pEntity == m_pBarrelBSP )
			continue;

		if ( pEntity && pEntity->pev->takedamage && pEntity != this
			&& planeFront.PointInFront( pEntity->pev->origin ) && planeBack.PointInFront( pEntity->pev->origin )
			&& planeRight.PointInFront( pEntity->pev->origin ) && planeLeft.PointInFront( pEntity->pev->origin )
			&& planeTop.PointInFront( pEntity->pev->origin ) && planeBottom.PointInFront( pEntity->pev->origin ) )
		{
			pEntity->TakeDamage( pev, m_pPlayer->pev, pEntity->pev->max_health + pEntity->pev->health + 1, DMG_CRUSH | DMG_ALWAYSGIB );
		}
	}
}

//=========================================================
// UpdateControllers - turns the model's turret and barrel to
// the brush parts, and tells the HUD where the turret points
//=========================================================
void CVehicleTank::UpdateControllers( void )
{
	void *pmodel = GET_MODEL_PTR( ENT( pev ) );

	float flTurret = m_pBarrelBSP->pev->angles.y - pev->angles.y;
	flTurret = UTIL_AngleNormalize( flTurret );
	if ( flTurret < 0 )
		flTurret += 360;

	// the HUD's turret indicator has 16 positions, centered on the compass points
	float flPos = flTurret - 11.25;
	if ( flPos < 0 )
		flPos += 360;
	float flHUDPos = 16 - flPos / 22.5;

	if ( m_pPlayer )
	{
		if ( (int)flHUDPos != m_sTurretHUDPos )
		{
			m_sTurretHUDPos = (int)flHUDPos;
			MESSAGE_BEGIN( MSG_ONE, gmsgTurretPos, NULL, m_pPlayer->pev );
				WRITE_SHORT( m_sTurretHUDPos );
			MESSAGE_END();
		}
	}

	// the turret turns on three controllers of 120 degrees each
	if ( flTurret <= 120 )
	{
		SetController( pmodel, pev, 0, flTurret );
		SetController( pmodel, pev, 1, 0 );
		SetController( pmodel, pev, 2, 0 );
	}
	else if ( flTurret <= 240 )
	{
		SetController( pmodel, pev, 0, 120 );
		SetController( pmodel, pev, 1, flTurret - 120 );
		SetController( pmodel, pev, 2, 0 );
	}
	else
	{
		SetController( pmodel, pev, 0, 120 );
		SetController( pmodel, pev, 1, 120 );
		SetController( pmodel, pev, 2, flTurret - 240 );
	}

	SetController( pmodel, pev, 3, UTIL_AngleDiff( m_pBarrelBSP->pev->angles.x, m_pTurretBSP->pev->angles.x ) );
}

void CVehicleTank::SetAvelocity( Vector vecAvelocity )
{
	pev->avelocity = vecAvelocity;
	if ( m_pBarrelBSP )
		m_pBarrelBSP->pev->avelocity = vecAvelocity;
	if ( m_pTurretBSP )
		m_pTurretBSP->pev->avelocity = vecAvelocity;
	if ( m_pBodyBSP )
		m_pBodyBSP->pev->avelocity = vecAvelocity;
}

void CVehicleTank::SetVelocity( Vector vecVelocity )
{
	pev->velocity = vecVelocity;
	if ( m_pBarrelBSP )
		m_pBarrelBSP->pev->velocity = vecVelocity;
	if ( m_pTurretBSP )
		m_pTurretBSP->pev->velocity = vecVelocity;
	if ( m_pBodyBSP )
		m_pBodyBSP->pev->velocity = vecVelocity;
}

void CVehicleTank::SetVelocityX( float x )
{
	pev->velocity.x = x;
	if ( m_pBarrelBSP )
		m_pBarrelBSP->pev->velocity.x = x;
	if ( m_pTurretBSP )
		m_pTurretBSP->pev->velocity.x = x;
	if ( m_pBodyBSP )
		m_pBodyBSP->pev->velocity.x = x;
}

void CVehicleTank::SetVelocityY( float y )
{
	pev->velocity.y = y;
	if ( m_pBarrelBSP )
		m_pBarrelBSP->pev->velocity.y = y;
	if ( m_pTurretBSP )
		m_pTurretBSP->pev->velocity.y = y;
	if ( m_pBodyBSP )
		m_pBodyBSP->pev->velocity.y = y;
}

void CVehicleTank::SetVelocityZ( float z )
{
	pev->velocity.z = z;
	if ( m_pBarrelBSP )
		m_pBarrelBSP->pev->velocity.z = z;
	if ( m_pTurretBSP )
		m_pTurretBSP->pev->velocity.z = z;
	if ( m_pBodyBSP )
		m_pBodyBSP->pev->velocity.z = z;
}

//=========================================================
// SideBlocked - is anything solid along one side of the
// body, between two of its corners? Remembers the wall's
// normal for sliding.
//=========================================================
int CVehicleTank::SideBlocked( int iStartForward, int iEndForward, int iStartRight, int iEndRight )
{
	TraceResult tr;
	Vector vecForward, vecRight, vecUp;
	Vector vecHit;
	Vector vecStart, vecEnd;

	UTIL_MakeVectorsPrivate( m_pBodyBSP->pev->angles, vecForward, vecRight, vecUp );

	vecStart = pev->origin + iStartForward * vecForward * VehicleLength() + iStartRight * vecRight * VehicleWidth() + vecUp * 32;
	vecEnd = pev->origin + iEndForward * vecForward * VehicleLength() + iEndRight * vecRight * VehicleWidth() + vecUp * 32;

	BOOL bStartSolid = UTIL_PointContents( vecStart ) == CONTENTS_SOLID;
	BOOL bEndSolid = UTIL_PointContents( vecEnd ) == CONTENTS_SOLID;

	if ( bStartSolid )
	{
		if ( bEndSolid )
			return 0;

		UTIL_TraceLine( vecEnd, vecStart, ignore_monsters, ENT( m_pBodyBSP->pev ), &tr );
		vecHit = vecStart;
	}
	else
	{
		if ( !bEndSolid )
		{
			// both corners clear: is something between them?
			UTIL_TraceLine( vecStart, vecEnd, ignore_monsters, ENT( m_pBodyBSP->pev ), &tr );
			if ( tr.flFraction == 1.0 )
				return 0;

			vecStart = pev->origin + iStartForward * vecForward * VehicleLength() + iStartRight * vecRight * VehicleWidth() + vecUp * 32;
			vecEnd = pev->origin + iEndForward * vecForward * VehicleLength() + iEndRight * vecRight * VehicleWidth() + vecUp * 32;

			if ( UTIL_PointContents( vecStart ) != CONTENTS_SOLID && UTIL_PointContents( vecEnd ) != CONTENTS_SOLID )
			{
				UTIL_TraceLine( vecStart, vecEnd, ignore_monsters, ENT( m_pBodyBSP->pev ), &tr );
				if ( tr.flFraction == 1.0 )
				{
					RestoreLastValid();
					m_flSpeed = 0;
					return 1;
				}
			}
			pev->velocity.x = 0;
			pev->velocity.y = 0;
			m_flSpeed = 0;
			return 1;
		}
		UTIL_TraceLine( vecStart, vecEnd, ignore_monsters, ENT( m_pBodyBSP->pev ), &tr );
		vecHit = vecEnd;
	}

	if ( tr.flFraction != 1.0 )
	{
		vecStart = pev->origin + iStartForward * vecForward * VehicleLength() + iStartRight * vecRight * VehicleWidth() + vecUp * 32;
		vecEnd = pev->origin + iEndForward * vecForward * VehicleLength() + iEndRight * vecRight * VehicleWidth() + vecUp * 32;

		if ( UTIL_PointContents( vecStart ) != CONTENTS_SOLID || UTIL_PointContents( vecEnd ) != CONTENTS_SOLID )
		{
			// trace from the center toward the hit instead
			vecStart = pev->origin;
			vecEnd = pev->origin + ( vecHit - pev->origin ).Normalize() * 512;
			vecStart.z = vecEnd.z = pev->origin.z + 32;
			UTIL_TraceLine( vecStart, vecEnd, ignore_monsters, ENT( m_pBodyBSP->pev ), &tr );
		}

		m_vecNormalLastFrame = tr.vecPlaneNormal;
		return 1;
	}

	return 0;
}

int CVehicleTank::SidesBlocked( void )
{
	if ( SideBlocked( -1, 1, -1, -1 ) || SideBlocked( 1, -1, 1, 1 )
		|| SideBlocked( 1, 1, -1, 1 ) || SideBlocked( -1, -1, -1, 1 ) )
		return 1;
	return 0;
}

//=========================================================
// CornerBlocked - will a side, moved by this frame's
// velocity, end up in something solid? Pulls the side in by
// 2 units at a time until it fits.
//=========================================================
int CVehicleTank::CornerBlocked( int iStartForward, int iEndForward, int iStartRight, int iEndRight, Vector vecVelocity, float flInterval, float flDist )
{
	TraceResult tr;
	int iBlocked = 0;

	if ( flDist < 0 )
	{
		m_bCanBounce = FALSE;
		m_vecCollision = g_vecZero;
		return 1;
	}

	Vector vecForward, vecRight, vecUp;
	Vector vecStart, vecEnd;
	UTIL_MakeVectorsPrivate( m_pBodyBSP->pev->angles + pev->avelocity / 10, vecForward, vecRight, vecUp );

	vecStart = pev->origin + vecVelocity.Normalize() * vecVelocity.Length() * flInterval + iStartForward * vecForward * ( VehicleLength() + flDist ) + vecRight * iStartRight * ( VehicleWidth() + flDist ) + vecUp * 32;
	vecEnd = pev->origin + vecVelocity.Normalize() * vecVelocity.Length() * flInterval + iEndForward * vecForward * ( VehicleLength() + flDist ) + vecRight * iEndRight * ( VehicleWidth() + flDist ) + vecUp * 32;

	if ( UTIL_PointContents( vecStart ) == CONTENTS_SOLID && UTIL_PointContents( vecEnd ) == CONTENTS_SOLID )
		return CornerBlocked( iStartForward, iEndForward, iStartRight, iEndRight, vecVelocity, flInterval, flDist - 2 );

	if ( UTIL_PointContents( vecStart ) == CONTENTS_SOLID )
	{
		iBlocked = 1;
		m_vecCollision = vecStart;
	}
	else if ( UTIL_PointContents( vecEnd ) == CONTENTS_SOLID )
	{
		iBlocked = 1;
		m_vecCollision = vecEnd;
	}
	else
	{
		UTIL_TraceLine( vecStart, vecEnd, ignore_monsters, ENT( m_pBodyBSP->pev ), &tr );
		if ( tr.flFraction != 1.0 )
		{
			m_bCanBounce = FALSE;
			m_vecCollision = vecEnd;
			CornerBlocked( iStartForward, iEndForward, iStartRight, iEndRight, vecVelocity, flInterval, flDist - 2 );
			return 1;
		}
	}

	return iBlocked;
}

BOOL CVehicleTank::CornersBlocked( Vector vecVelocity, float flInterval, float flDist )
{
	if ( CornerBlocked( -1, 1, -1, -1, vecVelocity, flInterval, flDist ) )
		return TRUE;
	if ( CornerBlocked( 1, -1, 1, 1, vecVelocity, flInterval, flDist ) )
		return TRUE;
	if ( CornerBlocked( 1, 1, -1, 1, vecVelocity, flInterval, flDist ) )
		return TRUE;
	if ( CornerBlocked( -1, -1, -1, 1, vecVelocity, flInterval, flDist ) )
		return TRUE;
	return FALSE;
}

//=========================================================
// PushAway - turning in place against a wall: the push that
// moves the tank off it
//=========================================================
Vector CVehicleTank::PushAway( void )
{
	if ( m_vecCollision != g_vecZero )
	{
		UTIL_MakeVectors( pev->angles + pev->avelocity / 10 );

		Vector vecDir;
		vecDir = ( m_vecCollision - pev->origin ).Normalize();
		Vector vecPush;
		if ( DotProduct( vecDir, gpGlobals->v_forward ) > 0 && pev->avelocity.y >= 0 )
			vecPush = m_vecCollision - pev->origin;

		if ( pev->avelocity.y < 0 )
			vecPush = pev->origin - m_vecCollision;
		else
			vecPush = pev->origin - m_vecCollision;

		if ( pev->avelocity.y < 0 )
			vecPush = m_vecCollision - pev->origin;

		Vector vecTurn = UTIL_VecToAngles( vecPush.Normalize() );
		Vector vecForward, vecRight;
		UTIL_MakeVectorsPrivate( vecTurn, vecForward, vecRight, NULL );

		SetAvelocity( pev->avelocity / 4 );

		return -vecRight * vecPush.Length() / 4;
	}

	return g_vecZero;
}

//=========================================================
// Slide - the part of the velocity along a wall
//=========================================================
Vector CVehicleTank::Slide( Vector vecVelocity, Vector vecNormal )
{
	float flDot = DotProduct( vecVelocity.Normalize(), vecNormal.Normalize() );
	float flSpeed = vecVelocity.Length();

	vecVelocity = vecVelocity.Normalize() * flSpeed * ( 1 - flDot );
	// sic: z scaled by 1 here and in vecAlong below, no-ops; the named components
	// give retail's x87 operand orders in the inlined Normalize, Length and dot products
	vecVelocity.z = vecVelocity.z * 1;

	Vector vecSide = CrossProduct( Vector( 0, 0, 1 ), vecNormal );
	vecSide = vecSide.Normalize();
	float flNormal = DotProduct( vecVelocity, vecNormal );
	Vector vecProj = flNormal * vecNormal;
	Vector vecAlong = vecVelocity - vecProj;
	vecAlong.z = vecAlong.z * 1;	// sic: see above
	vecVelocity = vecAlong - CrossProduct( vecNormal, vecSide ) * flNormal;

	if ( m_flSpeed > 0 )
		flSpeed = vecVelocity.Length();
	else if ( m_flSpeed < 0 )
		flSpeed = -vecVelocity.Length();

	if ( flSpeed > 125 )
		flSpeed = 125;
	else if ( flSpeed < -125 )
		flSpeed = -125;

	return vecVelocity.Normalize() * flSpeed;
}

//=========================================================
// GroundAngles - pitch and yaw of the ground under the tank,
// and the tank's height above it
//=========================================================
Vector CVehicleTank::GroundAngles( void )
{
	TraceResult tr;
	Vector vecAngles;
	Vector vecUp;

	UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, 2048 ), ignore_monsters, ENT( m_pBodyBSP->pev ), &tr );

	vecAngles = UTIL_VecToAngles( tr.vecPlaneNormal );
	UTIL_MakeVectorsPrivate( vecAngles, NULL, NULL, vecUp );
	vecUp = UTIL_VecToAngles( vecUp );
	vecUp.x *= -1;
	vecUp.z = pev->origin.z - tr.vecEndPos.z;	// height above the ground

	return vecUp;
}

//=========================================================
// BodyAvelocity - pitches and rolls the body toward the
// ground's slope
//=========================================================
Vector CVehicleTank::BodyAvelocity( float flYawDiff, Vector vecGround )
{
	// how much of the slope's pitch is pitch and how much is roll
	float flPitch = flYawDiff / 180;
	if ( flPitch < 0 )
		flPitch *= -1;
	flPitch = ( 2 - flPitch * 2 - 1 ) * vecGround.x;

	flYawDiff = UTIL_AngleNormalize( flYawDiff - 90 ) / 180;
	if ( flYawDiff < 0 )
		flYawDiff *= -1;

	float flPitchDiff = UTIL_AngleDiff( flPitch, pev->angles.x );
	float flRollDiff = UTIL_AngleDiff( ( 2 - flYawDiff * 2 - 1 ) * vecGround.x * -1, pev->angles.z );

	if ( flPitchDiff > 2 || flPitchDiff < -2 )
		flPitchDiff *= 2;
	if ( flRollDiff > 2 || flRollDiff < -2 )
		flRollDiff *= 2;

	return Vector( flPitchDiff, 0, flRollDiff );
}

//=========================================================
// TurretAvelocity - the same for the turret, which turns on
// its own
//=========================================================
Vector CVehicleTank::TurretAvelocity( float flYawDiff, Vector vecGround )
{
	// how much of the slope's pitch is pitch and how much is roll
	float flPitch = flYawDiff / 180;
	if ( flPitch < 0 )
		flPitch *= -1;
	flPitch = ( 2 - flPitch * 2 - 1 ) * vecGround.x;

	flYawDiff = UTIL_AngleNormalize( flYawDiff - 90 ) / 180;
	if ( flYawDiff < 0 )
		flYawDiff *= -1;

	float flPitchDiff = UTIL_AngleDiff( flPitch * -1, m_pTurretBSP->pev->angles.x );
	float flRollDiff = UTIL_AngleDiff( ( 2 - flYawDiff * 2 - 1 ) * vecGround.x * -1, m_pTurretBSP->pev->angles.z );

	if ( flPitchDiff > 2 || flPitchDiff < -2 )
		flPitchDiff *= 2;
	if ( flRollDiff > 2 || flRollDiff < -2 )
		flRollDiff *= 2;

	return Vector( flPitchDiff, 0, flRollDiff );
}

void CVehicleTank::RestoreLastValid( void )
{
	pev->velocity = g_vecZero;
	pev->angles = m_vecLastValidAng;
	UTIL_SetOrigin( pev, m_vecLastValidPos );
	UTIL_SetOrigin( m_pBarrelBSP->pev, m_pBarrelBSP->m_vecLastValidPos );
	UTIL_SetOrigin( m_pTurretBSP->pev, m_pTurretBSP->m_vecLastValidPos );
	m_pBodyBSP->pev->angles = m_pBodyBSP->m_vecLastValidAng;
	UTIL_SetOrigin( m_pBodyBSP->pev, m_pBodyBSP->m_vecLastValidPos );
	m_flNextBounceMove = gpGlobals->time;
}

void CVehicleTank::SaveLastValid( void )
{
	m_vecLastValidPos = pev->origin;
	m_vecLastValidAng = pev->angles;
	m_pBarrelBSP->m_vecLastValidPos = m_pBarrelBSP->pev->origin;
	m_pBarrelBSP->m_vecLastValidAng = m_pBarrelBSP->pev->angles;
	m_pTurretBSP->m_vecLastValidPos = m_pTurretBSP->pev->origin;
	m_pTurretBSP->m_vecLastValidAng = m_pTurretBSP->pev->angles;
	m_pBodyBSP->m_vecLastValidPos = m_pBodyBSP->pev->origin;
	m_pBodyBSP->m_vecLastValidAng = m_pBodyBSP->pev->angles;
}

//=========================================================
// UseVehicle - the player gets in
//=========================================================
void CVehicleTank::UseVehicle( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator->Classify() != CLASS_PLAYER )
		return;

	if ( m_bFirstTimeInTank )
	{
		FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
		m_bFirstTimeInTank = FALSE;
	}

	m_pPlayer = (CBasePlayer *)pActivator;
	m_pBarrelBSP->m_pPlayer = (CBasePlayer *)pActivator;
	m_pTurretBSP->m_pPlayer = m_pPlayer;
	m_pBodyBSP->m_pPlayer = m_pPlayer;
	m_pPlayer->m_pVehicle = this;

	int iState = 1;
	if ( m_iGasState == TANK_GAS_EMPTY )
		iState = 2;

	MESSAGE_BEGIN( MSG_ONE, gmsgTank, NULL, m_pPlayer->pev );
		WRITE_SHORT( iState );
	MESSAGE_END();
	m_iTankMsgs = 1;

	SetUse( NULL );
	SetThink( &CVehicleTank::WithPlayerThink );

	UTIL_SetSize( m_pPlayer->pev, g_vecZero, g_vecZero );
	m_pPlayer->pev->movetype = MOVETYPE_NOCLIP;
	m_pPlayer->pev->solid = SOLID_NOT;
	m_pPlayer->pev->takedamage = DAMAGE_NO;
	m_pPlayer->pev->velocity = g_vecZero;
	m_pPlayer->pev->flags |= FL_IMMUNE_LAVA;	// sic: FL_IMMUNE_LAVA, not FL_NOTARGET
	m_pPlayer->pev->effects |= EF_NODRAW;
	CLIENT_COMMAND( ENT( m_pPlayer->pev ), "cl_bob 0\n" );
	m_pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_HEALTH;

	if ( m_pPlayer->m_pActiveItem )
	{
		m_pPlayer->m_pActiveItem->Holster( );
		m_pPlayer->m_pLastItem = m_pPlayer->m_pActiveItem;
		m_pPlayer->m_pActiveItem = NULL;
	}

	if ( m_pPlayer->m_bGasTank == true && m_iGasState == TANK_GAS_EMPTY )
	{
		// he brought a gas can: refuel
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_STATIC, "tank/glug.wav", m_fTankSoundAdj, 0.8 );
		m_pPlayer->m_bGasTank = false;
		m_pPlayer->m_bClientGasTank = false;
		m_iGasState = TANK_GAS_REFUEL;
		pev->nextthink = gpGlobals->time + 5.0;
	}
	else if ( m_iGasState == TANK_GAS_FULL )
	{
		EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_STATIC, "tank/startup.wav", m_fTankSoundAdj * 0.4, 1.0, 0, PITCH_NORM );
	}

	EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_VOICE, "tank/engineidle.wav", 0, 0.8, 0, PITCH_NORM );
	EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_BODY, "tank/turn.wav", 0, 0.8, 0, PITCH_NORM );
	EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "tank/scrapewall.wav", 0, 0.8, 0, PITCH_NORM );
}

//=========================================================
// CanExit - is there room above the tank for the driver?
//=========================================================
int CVehicleTank::CanExit( void )
{
	TraceResult tr;

	Vector vecTop = Vector( pev->origin.x, pev->origin.y, pev->origin.z + 180 );
	UTIL_TraceHull( vecTop - Vector( -16, -16, -36 ), vecTop + Vector( 16, 16, 36 ), dont_ignore_monsters, human_hull, ENT( pev ), &tr );

	if ( !tr.fStartSolid && tr.flFraction >= 1.0 )
		return 1;

	if ( m_flNextOjectionDeniedSound < gpGlobals->time )
		m_flNextOjectionDeniedSound = gpGlobals->time + 3;

	return 0;
}

//=========================================================
// EjectPlayer - the driver gets out on top of the tank
//=========================================================
void CVehicleTank::EjectPlayer( void )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgTank, NULL, m_pPlayer->pev );
		WRITE_SHORT( 0 );
	MESSAGE_END();
	m_iTankMsgs = 0;

	UTIL_SetSize( m_pPlayer->pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );
	m_pPlayer->pev->movetype = MOVETYPE_WALK;
	m_pPlayer->pev->solid = SOLID_SLIDEBOX;
	m_pPlayer->pev->takedamage = DAMAGE_AIM;
	m_pPlayer->pev->view_ofs = Vector( 0, 0, 28 );
	m_pPlayer->pev->fixangle = TRUE;
	m_pPlayer->pev->v_angle.z = 0;
	m_pPlayer->pev->angles.z = 0;
	m_pPlayer->pev->v_angle.x = 0;
	m_pPlayer->pev->angles.x = 0;
	m_pPlayer->m_flFallVelocity = 0;
	m_pPlayer->pev->velocity = g_vecZero;
	m_pPlayer->m_pVehicle = NULL;
	m_pPlayer->m_bClientGasTank = false;
	m_pPlayer->pev->flags &= ~FL_IMMUNE_LAVA;
	CLIENT_COMMAND( ENT( m_pPlayer->pev ), "cl_bob 0.01\n" );
	m_pPlayer->pev->effects &= ~EF_NODRAW;

	if ( m_pPlayer->m_pLastItem && m_pPlayer->m_pLastItem->CanDeploy() )
	{
		m_pPlayer->m_pActiveItem = m_pPlayer->m_pLastItem;
		m_pPlayer->m_pLastItem = NULL;
		m_pPlayer->ResetAutoaim( );
		m_pPlayer->m_pActiveItem->Deploy( );
		m_pPlayer->m_pActiveItem->UpdateItemInfo( );
	}
	m_pPlayer->m_iHideHUD &= ~( HIDEHUD_WEAPONS | HIDEHUD_HEALTH );

	Vector vecExit = pev->origin + Vector( 0, 0, 180 );
	UTIL_SetOrigin( m_pPlayer->pev, vecExit );

	if ( m_iGasState == TANK_GAS_FULL )
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_STATIC, "tank/powerdown.wav", m_fTankSoundAdj, 0.8 );

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_VOICE, "common/null.wav", 0, 0.8 );
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_BODY, "common/null.wav", 0, 0.8 );
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "common/null.wav", 0, 0.8 );

	SetUse( &CVehicleTank::UseVehicle );
	SetThink( &CVehicleTank::WithoutPlayerThink );
	m_pPlayer = NULL;

	SetTankAnim( TANK_OFFLINE );
	SetVelocity( g_vecZero );
	SetAvelocity( g_vecZero );
	m_flSpeed = 0;
	m_fYawAccel = 0;
}

//=========================================================
// Debris - chunks of whatever the tank scrapes along
//=========================================================
void CVehicleTank::Debris( Vector vecPos )
{
	TraceResult tr;
	char chTextureType;
	char szbuffer[64];
	const char *pTextureName;
	float rgfl1[3];
	float rgfl2[3];

	Vector vecSrc = pev->origin;
	Vector vecEnd = m_vecCollision;

	UTIL_TraceLine( vecPos, pev->origin, ignore_monsters, ENT( pev ), &tr );

	CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

	chTextureType = 0;

	if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		chTextureType = CHAR_TEX_FLESH;
	else
	{
		vecSrc.CopyToArray( rgfl1 );
		vecEnd.CopyToArray( rgfl2 );

		if ( pEntity )
			pTextureName = TRACE_TEXTURE( ENT( pEntity->pev ), rgfl1, rgfl2 );
		else
			pTextureName = TRACE_TEXTURE( ENT( 0 ), rgfl1, rgfl2 );

		if ( pTextureName )
		{
			if ( *pTextureName == '-' || *pTextureName == '+' )
				pTextureName += 2;

			if ( *pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ' )
				pTextureName++;

			strcpy( szbuffer, pTextureName );
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;
			chTextureType = TEXTURETYPE_Find( szbuffer );
		}
	}

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecPos );
		WRITE_BYTE( TE_BREAKMODEL );
		WRITE_COORD( vecPos.x );
		WRITE_COORD( vecPos.y );
		WRITE_COORD( vecPos.z );
		WRITE_COORD( 1 );
		WRITE_COORD( 1 );
		WRITE_COORD( 1 );
		WRITE_COORD( RANDOM_LONG( -25, 25 ) );
		WRITE_COORD( RANDOM_LONG( -25, 25 ) );
		WRITE_COORD( RANDOM_LONG( 0, 25 ) );
		WRITE_BYTE( 20 );

		switch ( chTextureType )
		{
		case CHAR_TEX_CONCRETE:
		case CHAR_TEX_DIRT:
		case CHAR_TEX_TILE:
		case CHAR_TEX_VENT:
			WRITE_SHORT( m_iRockGibs );
			break;
		default:
			WRITE_SHORT( m_iMetalGibs );
			break;
		case CHAR_TEX_GRATE:
			WRITE_SHORT( m_iWoodGibs );
			break;
		}

		WRITE_BYTE( 8 );
		WRITE_BYTE( 1 );
		WRITE_BYTE( BREAK_CONCRETE );
	MESSAGE_END();
}

void CVehicleTank::FireShell( void )
{
	Vector vecSrc;
	Vector vecForward;

	GET_ATTACHMENT( ENT( pev ), 0, vecSrc, NULL );

	CVehicleTankShell *pShell = CVehicleTankShell::Create( vecSrc, m_pBarrelBSP->pev->angles, m_pPlayer );

	UTIL_MakeVectorsPrivate( m_pBarrelBSP->pev->angles, vecForward, NULL, NULL );
	pShell->pev->velocity = vecForward * 1800;
	pShell->pev->gravity = 0.5;

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/explode4.wav", m_fTankSoundAdj, 0.8 );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecSrc.x );
		WRITE_COORD( vecSrc.y );
		WRITE_COORD( vecSrc.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 20 );	// scale * 10
		WRITE_BYTE( 12 );	// framerate
	MESSAGE_END();

	UTIL_ScreenShake( pev->origin, 30.0, 175.0, 0.75, 600.0 );
}

void CVehicleTank::FireRocket( void )
{
	Vector vecSrc;

	GET_ATTACHMENT( ENT( pev ), 1, vecSrc, NULL );
	CVehicleTankRocket::Create( vecSrc, m_pBarrelBSP->pev->angles, m_pBarrelBSP, NULL );
}

void CVehicleTank::FireMachineGuns( void )
{
	Vector vecSrc;
	Vector vecForward, vecUp;

	// the guns take turns
	if ( m_iMGunSide )
	{
		GET_ATTACHMENT( ENT( pev ), 2, vecSrc, NULL );
		m_iMGunSide = 0;
	}
	else
	{
		GET_ATTACHMENT( ENT( pev ), 3, vecSrc, NULL );
		m_iMGunSide = 1;
	}

	Vector vecAngles = m_pBodyBSP->pev->angles;
	vecAngles.x += 4;
	UTIL_MakeVectorsPrivate( vecAngles, vecForward, NULL, vecUp );

	vecSrc = vecSrc + vecForward * 32;
	vecForward = ( vecForward * 2048 - vecUp * 16 ).Normalize();

	vecAngles = UTIL_VecToAngles( vecForward );
	vecAngles.x *= -1;

	PLAYBACK_EVENT_FULL( FEV_RELIABLE, ENT( pev ), m_usTankMGuns, 0.0, vecSrc - pev->origin, vecAngles, m_fTankSoundAdj, 0.0, 0, 0, 0, 0 );

	FireBullets( 1, vecSrc, vecForward, VECTOR_CONE_4DEGREES, 8192, BULLET_PLAYER_MP5, 0, 50, m_pBodyBSP->pev );
}
