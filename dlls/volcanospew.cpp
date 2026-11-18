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
// entity_volcanospew: sprays lava sprites straight up six
// times, a tenth of a second apart, then removes itself
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

class CVolcanoSpew : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT SprayLava( void );

	int		m_iLavaSprite;
	int		m_iSprays;
};

LINK_ENTITY_TO_CLASS( entity_volcanospew, CVolcanoSpew );

void CVolcanoSpew::Spawn( void )
{
	Precache();

	pev->takedamage = DAMAGE_NO;
	SET_MODEL( ENT( pev ), "models/null.mdl" );
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	SetTouch( NULL );
	pev->nextthink = gpGlobals->time + 0.1;
	SetThink( &CVolcanoSpew::SprayLava );
	m_iSprays = 0;

	ResetSequenceInfo();
}

void CVolcanoSpew::Precache( void )
{
	PRECACHE_MODEL( "models/null.mdl" );
	m_iLavaSprite = PRECACHE_MODEL( "sprites/lavaslop.spr" );
}

void CVolcanoSpew::SprayLava( void )
{
	m_iSprays++;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE_SPRAY );
		WRITE_COORD( pev->origin.x );	// position
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( 0 );	// direction
		WRITE_COORD( 0 );
		WRITE_COORD( 1 );
		WRITE_SHORT( m_iLavaSprite );
		WRITE_BYTE( 10 );	// count
		WRITE_BYTE( 100 );	// speed
		WRITE_BYTE( 150 );	// noise
	MESSAGE_END();

	if ( m_iSprays > 5 )
		SetThink( &CBaseEntity::SUB_Remove );

	pev->nextthink = gpGlobals->time + 0.1;
}
