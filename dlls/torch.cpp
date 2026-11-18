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
// the wall torch and its flame
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

//=========================================================
// decore_torchflame - the flame sprite, animated by hand
//=========================================================
class CFlame : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps( ) & ~FCAP_ACROSS_TRANSITION; }

	void EXPORT Animate( void );

	static CFlame *FlameCreate( entvars_t *pevOwner, Vector vecOrigin );
};
LINK_ENTITY_TO_CLASS( decore_torchflame, CFlame );

void CFlame::Precache( void )
{
	pev->model = MAKE_STRING( "sprites/flames.spr" );
	pev->modelindex = PRECACHE_MODEL( (char *)STRING( pev->model ) );
}

void CFlame::Spawn( void )
{
	Precache( );
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_NO;
	pev->effects = 0;
	pev->scale = 0.75;
	pev->rendermode = kRenderTransAdd;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 255;
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( -4, -4, -8 ), Vector( 4, 4, 16 ) );
	SET_MODEL( ENT( pev ), "sprites/flames.spr" );
	pev->frame = rand( ) % 21;
	SetTouch( NULL );
	SetThink( &CFlame::Animate );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CFlame::Animate( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
	pev->frame += 1;
	if ( pev->frame >= 21 )
		pev->frame = 0;
}

CFlame *CFlame::FlameCreate( entvars_t *pevOwner, Vector vecOrigin )
{
	CFlame *pFlame = GetClassPtr( (CFlame *)NULL );
	pFlame->pev->classname = MAKE_STRING( "decore_torchflame" );
	pFlame->Spawn( );
	UTIL_SetOrigin( pFlame->pev, vecOrigin );
	pFlame->pev->owner = ENT( pevOwner );
	return pFlame;
}

//=========================================================
// decore_torch - the torch, with a flame 32 units above it
//=========================================================
class CTorch : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps( ) & ~FCAP_ACROSS_TRANSITION; }

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	void CreateFlame( void );

	CBaseEntity *m_pFlame;
};
LINK_ENTITY_TO_CLASS( decore_torch, CTorch );

TYPEDESCRIPTION CTorch::m_SaveData[] =
{
	DEFINE_FIELD( CTorch, m_pFlame, FIELD_CLASSPTR ),
};

IMPLEMENT_SAVERESTORE( CTorch, CBaseEntity );

void CTorch::Precache( void )
{
	PRECACHE_MODEL( "models/torch.mdl" );
	UTIL_PrecacheOther( "decore_torchflame" );
}

void CTorch::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/torch.mdl" );
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( -8, -8, -16 ), Vector( 8, 8, 8 ) );
	CreateFlame( );
	SetTouch( NULL );
	SetThink( NULL );
}

void CTorch::CreateFlame( void )
{
	Vector vecFlame = pev->origin;
	vecFlame.z += 32;
	m_pFlame = CFlame::FlameCreate( pev, vecFlame );
}

// sic: unused; its empty constructor is all retail has of it. Const, so VC6
// gives it no storage: retail's torch.obj has no .bss
const Vector g_vecTorchUnused;
