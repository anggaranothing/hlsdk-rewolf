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
// Monster Maker - this is an entity that creates monsters
// in the game.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "saverestore.h"
#include "effects.h"

// Monstermaker spawnflags
#define	SF_MONSTERMAKER_START_ON	1 // start active ( if has targetname )
#define	SF_MONSTERMAKER_CYCLIC		4 // drop one monster every time fired.
#define SF_MONSTERMAKER_MONSTERCLIP	8 // Children are blocked by monsterclip
#define SF_MONSTERMAKER_WARPIN		16 // warp effect before each child
#define SF_MONSTERMAKER_DONTFALL	32 // children don't fall to the ground

//=========================================================
// MonsterMaker - this ent creates monsters during the game.
//=========================================================
class CMonsterMaker : public CBaseMonster
{
public:
	CMonsterMaker();
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData* pkvd);
	void EXPORT ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT CyclicUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT MakerThink ( void );
	void EXPORT MakerWarpThink ( void );
	void DeathNotice ( entvars_t *pevChild );// monster maker children use this to tell the monster maker that they have died.
	void MakeMonster( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];
	
	string_t m_iszMonsterClassname;// classname of the monster(s) that will be created.
	
	int	 m_cNumMonsters;// max number of monsters this ent can create

	
	int  m_cLiveChildren;// how many monsters made by this monster maker that are currently alive
	int	 m_iMaxLiveChildren;// max number of monsters that this maker may have out at one time.

	float m_flGround; // z coord of the ground under me, used to make sure no monsters are under the maker when it drops a new child

	BOOL m_fActive;
	BOOL m_fFadeChildren;// should we make the children fadeout?

	int m_iSkinNumber;// skin of the children
	int m_iFadeStatus;// -1 default, 0 don't fade, 1 fade
	float m_flWarpInTime;// when the warp effect ends and the child is made
	int m_iWarpSprite;// the warp sprite is up
	int m_iBeamSprite;
};

LINK_ENTITY_TO_CLASS( monstermaker, CMonsterMaker );

TYPEDESCRIPTION	CMonsterMaker::m_SaveData[] = 
{
	DEFINE_FIELD( CMonsterMaker, m_iszMonsterClassname, FIELD_STRING ),
	DEFINE_FIELD( CMonsterMaker, m_cNumMonsters, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_cLiveChildren, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_flGround, FIELD_FLOAT ),
	DEFINE_FIELD( CMonsterMaker, m_iMaxLiveChildren, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_fActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMonsterMaker, m_fFadeChildren, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMonsterMaker, m_iSkinNumber, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_flWarpInTime, FIELD_TIME ),
	DEFINE_FIELD( CMonsterMaker, m_iWarpSprite, FIELD_INTEGER ),
};


IMPLEMENT_SAVERESTORE( CMonsterMaker, CBaseMonster );

CMonsterMaker::CMonsterMaker()
{
	m_iFadeStatus = -1;
}

void CMonsterMaker :: KeyValue( KeyValueData *pkvd )
{
	
	if ( FStrEq(pkvd->szKeyName, "monstercount") )
	{
		m_cNumMonsters = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "m_imaxlivechildren") )
	{
		m_iMaxLiveChildren = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "monstertype") )
	{
		m_iszMonsterClassname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "_skin") )
	{
		m_iSkinNumber = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "fadestatus") )
	{
		m_iFadeStatus = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}


void CMonsterMaker :: Spawn( )
{
	pev->solid = SOLID_NOT;

	m_cLiveChildren = 0;
	Precache();
	if ( !FStringNull ( pev->targetname ) )
	{
		if ( pev->spawnflags & SF_MONSTERMAKER_CYCLIC )
		{
			SetUse ( &CMonsterMaker::CyclicUse );// drop one monster each time we fire
		}
		else
		{
			SetUse ( &CMonsterMaker::ToggleUse );// so can be turned on/off
		}

		if ( FBitSet ( pev->spawnflags, SF_MONSTERMAKER_START_ON ) )
		{// start making monsters as soon as monstermaker spawns
			pev->nextthink = gpGlobals->time + m_flDelay;
			m_fActive = TRUE;
			SetThink ( &CMonsterMaker::MakerThink );
		}
		else
		{// wait to be activated.
			m_fActive = FALSE;
			SetThink ( &CBaseEntity::SUB_DoNothing );
		}
	}
	else
	{// no targetname, just start.
			pev->nextthink = gpGlobals->time + m_flDelay;
			m_fActive = TRUE;
			SetThink ( &CMonsterMaker::MakerThink );
	}

	switch ( m_iFadeStatus )
	{
	case -1:
		if ( m_cNumMonsters == 1 )
		{
			m_fFadeChildren = FALSE;
		}
		else
		{
			m_fFadeChildren = TRUE;
		}
		break;
	case 1:
		m_fFadeChildren = TRUE;
		break;
	default:
		if ( m_iFadeStatus == 0 )
		{
			m_fFadeChildren = FALSE;
		}
		else
		{
			ALERT( at_console, "Invalid Monstermaker override: %d\n", m_iFadeStatus );
			m_fFadeChildren = FALSE;
		}
		break;
	}

	m_flGround = 0;
}

void CMonsterMaker :: Precache( void )
{
	CBaseMonster::Precache();

	UTIL_PrecacheOther( STRING( m_iszMonsterClassname ) );

	PRECACHE_MODEL( "sprites/warpin.spr" );
	PRECACHE_SOUND( "ambience/ammo_respawn.wav" );
	m_iBeamSprite = PRECACHE_MODEL( "sprites/xbeam1.spr" );
}

//=========================================================
// MakeMonster-  this is the code that drops the monster
//=========================================================
void CMonsterMaker::MakeMonster( void )
{
	edict_t	*pent;
	entvars_t		*pevCreate;

	if ( m_iMaxLiveChildren > 0 && m_cLiveChildren >= m_iMaxLiveChildren )
	{// not allowed to make a new one yet. Too many live ones out right now.
		ALERT ( at_console, "Too many live!\n" );
		return;
	}

	if ( !m_flGround )
	{
		// set altitude. Now that I'm activated, any breakables, etc should be out from under me. 
		TraceResult tr;

		UTIL_TraceLine ( pev->origin, pev->origin - Vector ( 0, 0, 2048 ), ignore_monsters, ENT(pev), &tr );
		m_flGround = tr.vecEndPos.z;
	}

	Vector mins = pev->origin - Vector( 34, 34, 0 );
	Vector maxs = pev->origin + Vector( 34, 34, 0 );
	maxs.z = pev->origin.z;
	mins.z = m_flGround;

	CBaseEntity *pList[2];
	int count = UTIL_EntitiesInBox( pList, 2, mins, maxs, FL_CLIENT|FL_MONSTER );
	if ( count && ( !FBitSet( pev->spawnflags, SF_MONSTERMAKER_WARPIN ) || m_iMaxLiveChildren != 1 ) )
	{
		// don't build a stack of monsters!
		return;
	}

	pent = CREATE_NAMED_ENTITY( m_iszMonsterClassname );

	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in MonsterMaker!\n" );
		ALERT ( at_console, "MAPPER!!  FIX THIS!!\n" );
		ALERT ( at_console, "PLEASE!!\n" );
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time;
		return;
	}
	
	// If I have a target, fire!
	if ( !FStringNull ( pev->target ) )
	{
		// delay already overloaded for this entity, so can't call SUB_UseTargets()
		FireTargets( STRING(pev->target), this, this, USE_TOGGLE, 0 );
	}

	pevCreate = VARS( pent );
	pevCreate->origin = pev->origin;
	pevCreate->angles = pev->angles;
	pevCreate->skin = m_iSkinNumber;

	if ( !FBitSet( pev->spawnflags, SF_MONSTERMAKER_DONTFALL ) )
		SetBits( pevCreate->spawnflags, SF_MONSTER_FALL_TO_GROUND );

	// Children hit monsterclip brushes
	if ( pev->spawnflags & SF_MONSTERMAKER_MONSTERCLIP )
		SetBits( pevCreate->spawnflags, SF_MONSTER_HITMONSTERCLIP );

	DispatchSpawn( ENT( pevCreate ) );
	pevCreate->owner = edict();

	if ( FBitSet( pev->spawnflags, SF_MONSTERMAKER_WARPIN ) )
	{
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "ambience/ammo_respawn.wav", 1, ATTN_NORM );

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_DLIGHT );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_BYTE( 16 );		// radius
			WRITE_BYTE( 0 );		// r
			WRITE_BYTE( 180 );		// g
			WRITE_BYTE( 0 );		// b
			WRITE_BYTE( 16 );		// life
			WRITE_BYTE( 10 );		// decay
		MESSAGE_END();
	}

	if ( !FStringNull( pev->netname ) )
	{
		// if I have a netname (overloaded), give the child monster that name as a targetname
		pevCreate->targetname = pev->netname;
	}

	m_cLiveChildren++;// count this monster
	m_cNumMonsters--;

	if ( m_cNumMonsters == 0 )
	{
		// Disable this forever.  Don't kill it because it still gets death notices
		SetThink( NULL );
		SetUse( NULL );
	}

	if ( !FBitSet( pev->spawnflags, SF_MONSTERMAKER_DONTFALL ) )
		DROP_TO_FLOOR( ENT( pevCreate ) );
}

//=========================================================
// CyclicUse - drops one monster from the monstermaker
// each time we call this.
//=========================================================
void CMonsterMaker::CyclicUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FBitSet( pev->spawnflags, SF_MONSTERMAKER_WARPIN ) )
	{
		SetThink ( &CMonsterMaker::MakerWarpThink );
		m_flWarpInTime = gpGlobals->time + 1.67;
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	MakeMonster();
}

//=========================================================
// ToggleUse - activates/deactivates the monster maker
//=========================================================
void CMonsterMaker :: ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_fActive ) )
		return;

	if ( m_fActive )
	{
		m_fActive = FALSE;
		SetThink ( NULL );
	}
	else
	{
		m_fActive = TRUE;
		SetThink ( &CMonsterMaker::MakerThink );
	}

	pev->nextthink = gpGlobals->time;
}

//=========================================================
// MakerThink - creates a new monster every so often
//=========================================================
void CMonsterMaker :: MakerThink ( void )
{
	if ( FBitSet( pev->spawnflags, SF_MONSTERMAKER_WARPIN ) )
	{
		SetThink ( &CMonsterMaker::MakerWarpThink );
		m_flWarpInTime = gpGlobals->time + 1.67;
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	pev->nextthink = gpGlobals->time + m_flDelay;

	MakeMonster();
}

//=========================================================
// MakerWarpThink - shows the warp effect, then makes the
// monster
//=========================================================
void CMonsterMaker :: MakerWarpThink ( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_iMaxLiveChildren > 0 && m_cLiveChildren >= m_iMaxLiveChildren )
	{
		m_flWarpInTime = gpGlobals->time + 1.77;
		return;
	}

	if ( m_flWarpInTime < gpGlobals->time && m_iWarpSprite )
	{
		m_flWarpInTime = gpGlobals->time + m_flDelay + 1.67;
		m_iWarpSprite = FALSE;
		MakeMonster();

		if ( pev->spawnflags & SF_MONSTERMAKER_CYCLIC )
		{
			SetThink ( &CBaseEntity::SUB_DoNothing );
		}
		else
		{
			SetThink ( &CMonsterMaker::MakerThink );
			pev->nextthink = gpGlobals->time + m_flDelay;
		}
		return;
	}

	if ( !m_iWarpSprite )
	{
		CSprite *pSprite = CSprite::SpriteCreate( "sprites/warpin.spr", pev->origin, TRUE );
		pSprite->SetScale( 0.5 );
		pSprite->pev->frame = 0;
		pSprite->pev->framerate = 15;
		pSprite->SetTransparency( kRenderTransAdd, 255, 255, 0, 200, kRenderFxNoDissipation );
		pSprite->pev->spawnflags |= SF_SPRITE_TEMPORARY;
		pSprite->AnimateAndDie( 15 );
		m_iWarpSprite = TRUE;
	}

	if ( m_flWarpInTime - 0.1 >= gpGlobals->time )
	{
		UTIL_MakeVectors( pev->angles );

		Vector vecEnd = pev->origin + gpGlobals->v_forward * RANDOM_FLOAT( -30, 30 ) + gpGlobals->v_up * RANDOM_FLOAT( 12, 27 ) + gpGlobals->v_right * RANDOM_FLOAT( -27, -12 );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_BEAMPOINTS );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_COORD( vecEnd.x );
			WRITE_COORD( vecEnd.y );
			WRITE_COORD( vecEnd.z );
			WRITE_SHORT( m_iBeamSprite );
			WRITE_BYTE( 0 );		// framestart
			WRITE_BYTE( 0 );		// framerate
			WRITE_BYTE( 1 );		// life
			WRITE_BYTE( 12 );		// width
			WRITE_BYTE( 50 );		// noise
			WRITE_BYTE( 80 );		// r
			WRITE_BYTE( 255 );		// g
			WRITE_BYTE( 120 );		// b
			WRITE_BYTE( 100 );		// brightness
			WRITE_BYTE( 5 );		// speed
		MESSAGE_END();

		vecEnd = pev->origin + gpGlobals->v_forward * RANDOM_FLOAT( -30, 30 ) + gpGlobals->v_up * RANDOM_FLOAT( 12, 27 ) + gpGlobals->v_right * RANDOM_FLOAT( 12, 27 );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_BEAMPOINTS );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_COORD( vecEnd.x );
			WRITE_COORD( vecEnd.y );
			WRITE_COORD( vecEnd.z );
			WRITE_SHORT( m_iBeamSprite );
			WRITE_BYTE( 0 );		// framestart
			WRITE_BYTE( 0 );		// framerate
			WRITE_BYTE( 1 );		// life
			WRITE_BYTE( 12 );		// width
			WRITE_BYTE( 50 );		// noise
			WRITE_BYTE( 80 );		// r
			WRITE_BYTE( 255 );		// g
			WRITE_BYTE( 120 );		// b
			WRITE_BYTE( 100 );		// brightness
			WRITE_BYTE( 5 );		// speed
		MESSAGE_END();

		vecEnd = pev->origin + gpGlobals->v_forward * RANDOM_FLOAT( -30, 30 ) + gpGlobals->v_up * RANDOM_FLOAT( -27, -12 ) + gpGlobals->v_right * RANDOM_FLOAT( 12, 27 );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_BEAMPOINTS );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_COORD( vecEnd.x );
			WRITE_COORD( vecEnd.y );
			WRITE_COORD( vecEnd.z );
			WRITE_SHORT( m_iBeamSprite );
			WRITE_BYTE( 0 );		// framestart
			WRITE_BYTE( 0 );		// framerate
			WRITE_BYTE( 1 );		// life
			WRITE_BYTE( 12 );		// width
			WRITE_BYTE( 50 );		// noise
			WRITE_BYTE( 80 );		// r
			WRITE_BYTE( 255 );		// g
			WRITE_BYTE( 120 );		// b
			WRITE_BYTE( 100 );		// brightness
			WRITE_BYTE( 5 );		// speed
		MESSAGE_END();

		vecEnd = pev->origin + gpGlobals->v_forward * RANDOM_FLOAT( -30, 30 ) + gpGlobals->v_up * RANDOM_FLOAT( -27, -12 ) + gpGlobals->v_right * RANDOM_FLOAT( -27, -12 );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_BEAMPOINTS );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_COORD( vecEnd.x );
			WRITE_COORD( vecEnd.y );
			WRITE_COORD( vecEnd.z );
			WRITE_SHORT( m_iBeamSprite );
			WRITE_BYTE( 0 );		// framestart
			WRITE_BYTE( 0 );		// framerate
			WRITE_BYTE( 1 );		// life
			WRITE_BYTE( 12 );		// width
			WRITE_BYTE( 50 );		// noise
			WRITE_BYTE( 80 );		// r
			WRITE_BYTE( 255 );		// g
			WRITE_BYTE( 120 );		// b
			WRITE_BYTE( 100 );		// brightness
			WRITE_BYTE( 5 );		// speed
		MESSAGE_END();
	}
}


//=========================================================
//=========================================================
void CMonsterMaker :: DeathNotice ( entvars_t *pevChild )
{
	// ok, we've gotten the deathnotice from our child, now clear out its owner if we don't want it to fade.
	m_cLiveChildren--;

	if ( !m_fFadeChildren )
	{
		pevChild->owner = NULL;
	}
}


