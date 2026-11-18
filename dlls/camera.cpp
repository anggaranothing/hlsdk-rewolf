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
// the rusty security camera: a turning base for props that
// track the player, the camera's lens flare and the camera
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "effects.h"

//=========================================================
// CBaseTurn - a model that turns its bone controllers
// toward a point, a few degrees per think (the camera's
// lens flare is one)
//=========================================================
class CBaseTurn : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int Relax( int iController );
	int TurnTo( int iController, const Vector &vecTarget );

	int m_iYaw;
	int m_iPitch;
};

TYPEDESCRIPTION	CBaseTurn::m_SaveData[] =
{
	DEFINE_FIELD( CBaseTurn, m_iYaw, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurn, m_iPitch, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CBaseTurn, CBaseMonster );

//=========================================================
// Relax - bring a controller back toward the middle. Nothing
// calls it.
//=========================================================
int CBaseTurn::Relax( int iController )
{
	int iAngle;
	int iNewAngle;

	switch ( iController )
	{
	case 0:
		iAngle = m_iYaw;
		break;
	case 1:
		iAngle = m_iPitch;
		break;
	default:
		return FALSE;
	}

	// sic: within 10 degrees of the middle it stops short of it
	if ( iAngle > 0 )
	{
		iNewAngle = iAngle - 10;
		if ( iNewAngle > 0 )
			iAngle = iNewAngle;
	}
	else if ( iAngle < 0 )
	{
		iNewAngle = iAngle + 10;
		if ( iNewAngle < 0 )
			iAngle = iNewAngle;
	}
	else
		return TRUE;

	switch ( iController )
	{
	case 0:
		m_iYaw = iAngle;
		SetBoneController( 0, iAngle );
		break;
	case 1:
		m_iPitch = iAngle;
		SetBoneController( 1, iAngle );
		break;
	}

	return FALSE;
}

//=========================================================
// TurnTo - turn a controller up to 10 degrees toward
// vecTarget. TRUE once it points there.
//=========================================================
int CBaseTurn::TurnTo( int iController, const Vector &vecTarget )
{
	int fDone = FALSE;
	Vector vecOrigin = pev->origin;
	Vector vecDir = vecTarget - vecOrigin;
	int *piAngle;	// sic: see below
	int iCurrent;
	float flCurrent;
	float flAngle;
	float flDiff;

	// sic: the current angle is read through piAngle and the sum from the
	// member, so VC6 loads the member twice (fild from it, mov into a
	// register) as retail does; read the same way twice it reuses one load
	switch ( iController )
	{
	case 0:
		piAngle = &m_iYaw;
		iCurrent = *piAngle;
		flCurrent = m_iYaw + pev->angles.y;
		flAngle = UTIL_VecToYaw( vecDir );
		flDiff = UTIL_AngleDiff( flAngle, flCurrent );
		break;
	case 1:
		piAngle = &m_iPitch;
		iCurrent = *piAngle;
		flCurrent = m_iPitch + pev->angles.x;
		flAngle = UTIL_VecToPitch( vecDir );
		flDiff = UTIL_AngleDiff( flAngle, flCurrent );
		break;
	default:
		return FALSE;
	}

	if ( flDiff >= 180 )
		flDiff -= 360;

	if ( flDiff > 10 )
		flDiff = 10;
	else if ( flDiff < -10 )
		flDiff = -10;
	else
		fDone = TRUE;

	iCurrent += (int)flDiff;
	if ( iCurrent >= 360 )
		iCurrent -= 360;
	if ( iCurrent <= -360 )
		iCurrent += 360;

	switch ( iController )
	{
	case 0:
		m_iYaw = iCurrent;
		SetBoneController( 0, iCurrent );
		break;
	case 1:
		m_iPitch = iCurrent;
		SetBoneController( 1, -iCurrent );
		break;
	}

	return fDone;
}

LINK_ENTITY_TO_CLASS( decore_camflare, CBaseTurn );

void CBaseTurn::Precache( void )
{
	PRECACHE_MODEL( "models/cameracone.mdl" );
}

void CBaseTurn::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/cameracone.mdl" );
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING( "decore_camflare" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	UTIL_SetOrigin( pev, pev->origin );
	MonsterInit( );
	pev->takedamage = DAMAGE_NO;
	pev->rendermode = kRenderTransAdd;
	pev->renderfx = kRenderFxGlowShell;
}


//=========================================================
// CRustCamera - the security camera: follows its target
// (or the player) with its lens flare, a blinking led and
// a glowing eye
//=========================================================
class CRustCamera : public CBaseTurn
{
public:
	void Spawn( void );
	void Precache( void );
	void RunAI( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CBaseTurn *m_pGlow;
	CSprite *m_pLed;
	CSprite *m_pFlare;

private:
	void EXPORT ToggleToOn( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};
LINK_ENTITY_TO_CLASS( decore_cam, CRustCamera );

TYPEDESCRIPTION	CRustCamera::m_SaveData[] =
{
	DEFINE_FIELD( CRustCamera, m_pGlow, FIELD_CLASSPTR ),
	DEFINE_FIELD( CRustCamera, m_pLed, FIELD_CLASSPTR ),
	DEFINE_FIELD( CRustCamera, m_pFlare, FIELD_CLASSPTR ),
};

IMPLEMENT_SAVERESTORE( CRustCamera, CBaseTurn );

void CRustCamera::Precache( void )
{
	PRECACHE_MODEL( "models/camera.mdl" );
}

void CRustCamera::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/camera.mdl" );
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;
	pev->view_ofs = Vector( 0, 0, -8 );
	m_flFieldOfView = -0.7;
	m_MonsterState = MONSTERSTATE_ALERT;
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	MonsterInit( );
	pev->takedamage = DAMAGE_NO;
	SetUse( &CRustCamera::ToggleToOn );

	m_pLed = CSprite::SpriteCreate( "sprites/camled.spr", pev->origin, TRUE );
	if ( m_pLed )
	{
		m_pLed->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
		m_pLed->SetAttachment( edict( ), 1 );
		m_pLed->SetScale( 0.2 );
		m_pLed->TurnOn( );
	}

	m_pFlare = CSprite::SpriteCreate( "sprites/cameye.spr", pev->origin, TRUE );
	if ( m_pFlare )
	{
		m_pFlare->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
		m_pFlare->SetScale( 0.3 );
		m_pFlare->SetAttachment( ENT( pev ), 3 );
		m_pFlare->pev->framerate = 9;
		m_pFlare->pev->spawnflags = 0;
		m_pFlare->TurnOn( );
		m_pFlare->pev->effects |= EF_NODRAW;
	}

	m_pGlow = GetClassPtr( (CBaseTurn *)NULL );
	if ( m_pGlow )
	{
		m_pGlow->Spawn( );
		m_pGlow->pev->origin = pev->origin;
		m_pGlow->pev->angles = pev->angles;
		m_pGlow->pev->renderamt = 120;
		m_pGlow->pev->effects |= EF_NODRAW;
	}
}

void CRustCamera::RunAI( void )
{
	CBaseMonster::RunAI( );

	Vector vecTarget = g_vecZero;

	if ( pev->target )
	{
		CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
		m_pGoalEnt = pTarget;
		if ( pTarget && pTarget->IsAlive( ) )
			vecTarget = pTarget->BodyTarget( pev->origin );
	}

	if ( vecTarget == g_vecZero )
	{
		CBaseEntity *pPlayer = CBaseEntity::Instance( INDEXENT( 1 ) );
		if ( pPlayer )
			vecTarget = pPlayer->BodyTarget( pev->origin );
	}

	if ( vecTarget == g_vecZero )
		vecTarget = pev->origin + gpGlobals->v_forward * 128;

	TurnTo( 1, vecTarget );
	if ( m_pGlow )
		m_pGlow->TurnTo( 1, vecTarget );

	pev->ideal_yaw = UTIL_VecToYaw( vecTarget - pev->origin );
	ChangeYaw( 40 );

	if ( m_pGlow )
	{
		m_pGlow->pev->ideal_yaw = UTIL_VecToYaw( vecTarget - m_pGlow->pev->origin );
		m_pGlow->ChangeYaw( 40 );
	}
}

void CRustCamera::ToggleToOn( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_pGlow )
	{
		if ( m_pGlow->pev->effects & EF_NODRAW )
			m_pGlow->pev->effects &= ~EF_NODRAW;
		else
			m_pGlow->pev->effects |= EF_NODRAW;
	}

	if ( m_pFlare )
	{
		if ( m_pFlare->pev->effects & EF_NODRAW )
			m_pFlare->pev->effects &= ~EF_NODRAW;
		else
			m_pFlare->pev->effects |= EF_NODRAW;
	}
}
