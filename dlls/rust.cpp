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
// the rust robots' base, CBaseRust, and their projectiles:
// rustbitshot (the rustbit's and the rust flier's bolt),
// gunner_rocket (a rocket that homes in on players) and
// gunner_mortar (a shell that bursts into cluster bombs)
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "talkmonster.h"
#include "effects.h"
#include "decals.h"
#include "dmlrocket.h"
#include "rust.h"

// the robots' gib and gore spray sprites
int g_iGibBot;
int g_iGoreBot;


// the mortar's think and touch come first in retail's object
void CGunnerMortar :: MortarThink( void )
{
	Fragment( );
}


void CGunnerMortar :: FragmentTouch( CBaseEntity *pOther )
{
	Fragment( );
}


//=========================================================
// CBaseRust
//=========================================================
TYPEDESCRIPTION	CBaseRust::m_SaveData[] =
{
	DEFINE_FIELD( CBaseRust, m_iVoicePitch, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseRust, m_iSentence, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseRust, m_iModelIndexBotDeath, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CBaseRust, CSquadMonster );


//=========================================================
// SpeakSentence - sic: JustSpoke comes first, so this never
// says anything
//=========================================================
void CBaseRust :: SpeakSentence( void )
{
	JustSpoke( );

	if ( FOkToSpeak( ) && m_iSentence != RUST_SENT_NONE )
	{
		if ( FOkToSpeak( ) )
			SENTENCEG_PlayRndSz( ENT(pev), SentenceGroup( m_iSentence ), m_iSentenceVolume, ATTN_NORM, 0, m_iVoicePitch );
	}
}


// the subclasses' Spawn calls this before MonsterInit
void CBaseRust :: RustInit( void )
{
	m_flSpawnTime = gpGlobals->time;
}


void CBaseRust :: JustSpoke( void )
{
	CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT(1.5, 2.0);
	m_iSentence = RUST_SENT_NONE;
}


void CBaseRust :: Precache( void )
{
	m_iModelIndexBotDeath = PRECACHE_MODEL( "sprites/botdeath.spr" );
	g_iGoreBot = PRECACHE_MODEL( "sprites/gorebot.spr" );
	g_iGibBot = PRECACHE_MODEL( "sprites/gibbot.spr" );

	CBaseMonster::Precache( );
}


//=========================================================
// someone else is talking - don't speak
//=========================================================
BOOL CBaseRust :: FOkToSpeak( void )
{
// if someone else is talking, don't speak
	if (gpGlobals->time <= CTalkMonster::g_talkWaitTime)
		return FALSE;

	if ( pev->spawnflags & SF_MONSTER_GAG )
	{
		if ( m_MonsterState != MONSTERSTATE_COMBAT )
		{
			// no talking outside of combat if gagged.
			return FALSE;
		}
	}

	return TRUE;
}


void CBaseRust :: GibMonster( void )
{
	CBaseMonster::GibMonster( );
}


//=========================================================
// CallForHelp - the robots of the class nearby take on the
// enemy at once
//=========================================================
void CBaseRust :: CallForHelp( char *szClassname, float flDist, EHANDLE hEnemy, Vector &vecLocation )
{
	CBaseEntity *pEntity = NULL;

	while ((pEntity = UTIL_FindEntityByClassname( pEntity, szClassname )) != NULL)
	{
		float d = (pev->origin - pEntity->pev->origin).Length();
		if (d < flDist)
		{
			CBaseMonster *pMonster = pEntity->MyMonsterPointer( );
			if (pMonster)
			{
				pMonster->m_hEnemy = hEnemy;
				pMonster->m_afMemory |= bits_MEMORY_PROVOKED;
			}
		}
	}
}


//=========================================================
// rustbitshot
//=========================================================
LINK_ENTITY_TO_CLASS( rustbitshot, CRustbitShot );

TYPEDESCRIPTION	CRustbitShot::m_SaveData[] =
{
	DEFINE_FIELD( CRustbitShot, m_maxFrame, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CRustbitShot, CBaseEntity );


void CRustbitShot :: Precache( void )
{
	PRECACHE_MODEL( "sprites/bitboltsmall.spr" );
	PRECACHE_SOUND( "rustbit/RustBit_Projectile1.wav" );
	PRECACHE_SOUND( "rustbit/RustBit_Projectile2.wav" );
}


void CRustbitShot :: Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING( "rustbitshot" );

	SET_MODEL( ENT(pev), "sprites/bitboltsmall.spr" );
	UTIL_SetSize( pev, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ) );

	pev->solid = SOLID_BBOX;
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;
	pev->frame = 0;
	pev->scale = 0.5;
	pev->takedamage = DAMAGE_NO;

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;

	SetTouch( &CRustbitShot::ShotTouch );
}


//=========================================================
// Shoot - a shot along vecDir, spread like a bullet in the
// cone of vecSpread around gpGlobals' aim vectors
//=========================================================
void CRustbitShot :: Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecDir, Vector vecSpread, int iSpeed )
{
	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;

	// get circular gaussian spread
	float x, y, z;
	do {
		x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		z = x*x+y*y;
	} while (z > 1);

	vecDir = vecDir + x * vecSpread.x * vecRight + y * vecSpread.y * vecUp;
	vecDir = vecDir * iSpeed;

	CRustbitShot *pShot = GetClassPtr( (CRustbitShot *)NULL );
	UTIL_SetOrigin( pShot->pev, vecStart );
	pShot->Spawn( );
	pShot->pev->velocity = vecDir;
	pShot->pev->owner = ENT( pevOwner );
	pShot->SetThink( NULL );
}


void CRustbitShot :: ShotTouch( CBaseEntity *pOther )
{
	if ( pOther->edict() == pev->owner )
		return;

	// shots of one owner pass each other
	if ( pev->owner == pOther->pev->owner )
		return;

	if ( !pOther->pev->takedamage )
	{
		TraceResult tr;

		UTIL_TraceLine( pev->origin, pev->origin + pev->velocity * 8, ignore_monsters, ENT(pev), &tr );
		UTIL_DecalTrace( &tr, DECAL_PULSE );

		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "rustbit/RustBit_Projectile2.wav", 0.3, ATTN_NORM, 0, PITCH_NORM );
		UTIL_Remove( this );
		return;
	}

	pOther->TakeDamage( pev, pev, gSkillData.rustbitDmgShot, DMG_GENERIC );

	if ( pOther->IsPlayer( ) )
		EMIT_SOUND_DYN( pOther->edict(), CHAN_VOICE, "rustbit/RustBit_Projectile1.wav", 1.0, ATTN_NORM, 0, 130 );
	else
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "rustbit/RustBit_Projectile2.wav", 0.3, ATTN_NORM, 0, PITCH_NORM );

	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}


//=========================================================
// gunner_rocket
//=========================================================
LINK_ENTITY_TO_CLASS( gunner_rocket, CGunnerRocket );

TYPEDESCRIPTION	CGunnerRocket::m_SaveData[] =
{
	DEFINE_FIELD( CGunnerRocket, m_vecForward, FIELD_VECTOR ),
	DEFINE_FIELD( CGunnerRocket, m_pFlare, FIELD_CLASSPTR ),
};
IMPLEMENT_SAVERESTORE( CGunnerRocket, CGrenade );


void CGunnerRocket :: Precache( void )
{
	PRECACHE_MODEL( "models/rocket.mdl" );
	m_iTrail = PRECACHE_MODEL( "sprites/firebeam.spr" );
	PRECACHE_MODEL( "sprites/flame.spr" );
	PRECACHE_SOUND( "weapons/rocket1.wav" );
	PRECACHE_SOUND( "gunner/gunner_boom1.wav" );
	PRECACHE_SOUND( "gunner/gunner_boom2.wav" );
	PRECACHE_SOUND( "gunner/gunner_boom3.wav" );
}


void CGunnerRocket :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->classname = MAKE_STRING( "gunner_rocket" );

	SET_MODEL(ENT(pev), "models/rocket.mdl");
	UTIL_SetSize(pev, Vector( -4, -4, -4 ), Vector( 4, 4, 4 ));
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CGunnerRocket::IgniteThink );
	SetTouch( &CGunnerRocket::RocketExplodeTouch );

	UTIL_MakeAimVectors( pev->angles );
	m_vecForward = gpGlobals->v_forward;
	pev->gravity = 2.0;
	pev->dmg = 80;

	pev->nextthink = gpGlobals->time + 0.1;
}


void CGunnerRocket :: RocketExplodeTouch( CBaseEntity *pOther )
{
	// rockets don't blow each other up
	if ( FClassnameIs( pOther->pev, "gunner_rocket" ) )
		return;

	if ( m_pFlare )
		UTIL_Remove( m_pFlare );

	// sic: there's no case 0, and RANDOM_LONG never gives 3
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 1:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "gunner/gunner_boom1.wav", 1, ATTN_NORM );	break;
	case 2:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "gunner/gunner_boom2.wav", 1, ATTN_NORM );	break;
	case 3:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "gunner/gunner_boom3.wav", 1, ATTN_NORM );	break;
	}

	ExplodeTouch( pOther );
}


void CGunnerRocket :: IgniteThink( void )
{
	m_pFlare = CSprite::SpriteCreate( "sprites/flame.spr", pev->origin, TRUE );
	m_pFlare->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
	m_pFlare->SetAttachment( edict(), 1 );
	m_pFlare->TurnOn( );

	// make rocket sound
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0.5, 0.5 );

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );

		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 15 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness

	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	// set to accelerate
	SetThink( &CGunnerRocket::AccelerateThink );
	pev->nextthink = gpGlobals->time + 0.1;
}


void CGunnerRocket :: AccelerateThink( void )
{
	// check world boundaries
	if (pev->origin.x < -4096 || pev->origin.x > 4096 || pev->origin.y < -4096 || pev->origin.y > 4096 || pev->origin.z < -4096 || pev->origin.z > 4096)
	{
		if ( m_pFlare )
			UTIL_Remove( m_pFlare );
		UTIL_Remove( this );
		return;
	}

	// home in on the players nearby
	CBaseEntity *pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, 1024 )) != NULL)
	{
		if ( pEntity->pev->takedamage && pEntity->IsPlayer( ) && pEntity->edict() != pev->owner && this != pEntity )
		{
			Vector vecDir, vecVelocity;
			vecVelocity = pev->velocity;
			vecDir = pEntity->pev->origin - pev->origin;
			vecDir.Normalize( );	// sic: the result is thrown away
			vecDir[0] = vecDir[0] * 0.7;	// sic: not z
			vecDir[1] = vecDir[1] * 0.7;
			pev->velocity = vecDir + vecVelocity;
		}
	}

	// accelerate
	float flSpeed = pev->velocity.Length();
	if (flSpeed < 1800)
	{
		pev->velocity = pev->velocity + m_vecForward * 200;
	}

	// re-aim
	pev->angles = UTIL_VecToAngles( pev->velocity );

	pev->nextthink = gpGlobals->time + 0.1;
}


//=========================================================
// gunner_mortar
//=========================================================
LINK_ENTITY_TO_CLASS( gunner_mortar, CGunnerMortar );


void CGunnerMortar :: Spawn( void )
{
	Precache( );

	SET_MODEL( ENT(pev), "models/dmlcluster.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	UTIL_SetOrigin( pev, pev->origin );

	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->nextthink = gpGlobals->time + 1;

	// trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 10 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 156 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();

	SetThink( &CGunnerMortar::MortarThink );
	SetTouch( &CGunnerMortar::FragmentTouch );
}


void CGunnerMortar :: Precache( void )
{
	m_iTrail = PRECACHE_MODEL( "sprites/firebeam.spr" );
	PRECACHE_MODEL( "models/dmlcluster.mdl" );
}


CGunnerMortar *CGunnerMortar :: MortarCreate( void )
{
	CGunnerMortar *pMortar = GetClassPtr( (CGunnerMortar *)NULL );
	pMortar->pev->classname = MAKE_STRING( "gunner_mortar" );
	pMortar->Spawn( );

	return pMortar;
}


//=========================================================
// Fragment - four cluster bombs fly on, back the way the
// mortar came
//=========================================================
void CGunnerMortar :: Fragment( void )
{
	for ( int i = 0; i < 4; i++ )
	{
		CClusterGrenade *pCluster = CClusterGrenade::CreateCluster( );

		pCluster->pev->origin = pev->origin;
		pCluster->pev->owner = pev->owner;
		pCluster->pev->velocity = pev->velocity * 0.5 + Vector( RANDOM_LONG( -60, 60 ), RANDOM_LONG( -60, 60 ), RANDOM_LONG( -60, 60 ) );

		// get circular gaussian spread
		float x, y, z;
		do {
			x = RANDOM_FLOAT(-0.8,0.8) + RANDOM_FLOAT(-0.8,0.8);
			y = RANDOM_FLOAT(-0.8,0.8) + RANDOM_FLOAT(-0.8,0.8);
			z = x*x+y*y;
		} while (z > 1);

		pCluster->pev->angles = -pev->angles + x * VECTOR_CONE_20DEGREES.x * gpGlobals->v_right + y * VECTOR_CONE_20DEGREES.y * gpGlobals->v_up;

		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/dml_fragment.wav", 1, ATTN_NORM );
	}

	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}
