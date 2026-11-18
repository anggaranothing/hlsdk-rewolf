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
// the demoman: a bandit with a shotgun and a rocket
// launcher, who drops proximity mines when you get close
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"animation.h"
#include	"weapons.h"
#include	"effects.h"
#include	"soundent.h"
#include	"bandit.h"

extern DLL_GLOBAL int g_iSkillLevel;

//=========================================================
// the demoman's proximity mine
//=========================================================
class CDemomanMine : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void ) { return CLASS_HUMAN_MILITARY; }
	void Look( int iDistance );
	void Killed( entvars_t *pevAttacker, int iGib );

	void EXPORT WaitToExplode( void );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	CBaseEntity *m_pLed;
	float m_fNextBlink;
};

LINK_ENTITY_TO_CLASS( demoman_mine, CDemomanMine );

TYPEDESCRIPTION	CDemomanMine::m_SaveData[] =
{
	DEFINE_FIELD( CDemomanMine, m_pLed, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDemomanMine, m_fNextBlink, FIELD_FLOAT ),
	DEFINE_FIELD( CDemomanMine, m_fRegisteredSound, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CDemomanMine, CGrenade );

void CDemomanMine::Precache( void )
{
	PRECACHE_MODEL( "models/demomine.mdl" );
	PRECACHE_MODEL( "sprites/camled.spr" );
	PRECACHE_SOUND( "weapons/dml_lock.wav" );
}

void CDemomanMine::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_TOSS;
	pev->classname = MAKE_STRING( "demoman_mine" );

	SET_MODEL( ENT(pev), "models/demomine.mdl" );
	UTIL_SetSize( pev, Vector( -4, -4, 0 ), Vector( 4, 4, 8 ) );
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_AIM;
	UTIL_SetOrigin( pev, pev->origin );

	pev->dmg = gSkillData.plrDmgMine;

	SetThink( &CDemomanMine::WaitToExplode );
	pev->nextthink = gpGlobals->time + 0.1;

	m_pLed = NULL;
	m_fRegisteredSound = FALSE;
}

//=========================================================
// Look - arms the mine when something the demoman hates
// comes into view
//=========================================================
void CDemomanMine::Look( int iDistance )
{
	Vector vecSrc = pev->origin;
	vecSrc.z += 1;
	BOOL fInWater = UTIL_PointContents( vecSrc ) == CONTENTS_WATER;
	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, vecSrc, iDistance ) ) != NULL )
	{
		if ( pEntity->pev->takedamage == DAMAGE_NO )
			continue;

		// don't look through water
		if ( fInWater )
		{
			if ( pEntity->pev->waterlevel == 0 )
				continue;
		}
		else if ( pEntity->pev->waterlevel == 3 )
			continue;

		TraceResult tr;
		Vector vecTarget;
		vecTarget = pEntity->BodyTarget( vecSrc );
		UTIL_TraceLine( vecSrc, vecTarget, dont_ignore_monsters, NULL, &tr );

		if ( ( tr.flFraction == 1.0 || tr.pHit == pEntity->edict() ) && IRelationship( pEntity ) > R_NO )
		{
			m_fRegisteredSound = TRUE;
			CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin, pev->dmg * 2.5, 0.3 );

			switch ( g_iSkillLevel )
			{
			case SKILL_MEDIUM:
				pev->dmgtime = gpGlobals->time + 1.5;
				break;
			case SKILL_HARD:
				pev->dmgtime = gpGlobals->time + 1;
				break;
			default:
				pev->dmgtime = gpGlobals->time + 2.5;
				break;
			}

			m_fNextBlink = ( pev->dmgtime - gpGlobals->time ) / 20;
			CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin, 400, 2.0 );
			return;
		}
	}
}

void CDemomanMine::WaitToExplode( void )
{
	if ( !m_fRegisteredSound )
	{
		pev->nextthink = gpGlobals->time + 0.3;
		Look( 50 );
		return;
	}

	if ( !m_pLed )
	{
		CSprite *pLed = CSprite::SpriteCreate( "sprites/camled.spr", pev->origin, TRUE );
		m_pLed = pLed;
		pLed->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
		m_pLed->pev->scale = 0.4;
		((CSprite *)m_pLed)->TurnOn();

		UTIL_SetOrigin( m_pLed->pev, pev->origin + Vector( 0, 0, 4 ) );
	}

	if ( gpGlobals->time > m_fNextBlink )
	{
		EMIT_SOUND( ENT(pev), CHAN_ITEM, "weapons/dml_lock.wav", 1, ATTN_NORM );

		if ( m_pLed->pev->effects & EF_NODRAW )
			m_pLed->pev->effects &= ~EF_NODRAW;
		else
			m_pLed->pev->effects |= EF_NODRAW;

		// blink faster as the time runs out
		m_fNextBlink = gpGlobals->time + ( pev->dmgtime - gpGlobals->time ) / 20;
	}

	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->dmgtime <= gpGlobals->time )
	{
		if ( m_pLed )
		{
			UTIL_Remove( m_pLed );
			m_pLed = NULL;
		}
		Detonate( );
	}
}

void CDemomanMine::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( m_pLed )
	{
		UTIL_Remove( m_pLed );
		m_pLed = NULL;
	}
	CGrenade::Killed( pevAttacker, iGib );
}


//=========================================================
// the demoman's rocket: it steers toward players near its
// path, less and less as it flies
//=========================================================
class CDemomanRocket : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT RocketExplodeTouch( CBaseEntity *pOther );
	void EXPORT IgniteThink( void );
	void EXPORT AccelerateThink( void );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iTrail;
	CBaseEntity *m_pFlare;
	float m_fPercentSeek;
	int m_iWhiteTrail;
};

LINK_ENTITY_TO_CLASS( demoman_rocket, CDemomanRocket );

TYPEDESCRIPTION	CDemomanRocket::m_SaveData[] =
{
	DEFINE_FIELD( CDemomanRocket, m_pFlare, FIELD_CLASSPTR ),
	DEFINE_FIELD( CDemomanRocket, m_fPercentSeek, FIELD_INTEGER ),	// sic: a float
};

IMPLEMENT_SAVERESTORE( CDemomanRocket, CGrenade );

void CDemomanRocket::Precache( void )
{
	PRECACHE_MODEL( "models/dmlrocket.mdl" );
	m_iTrail = PRECACHE_MODEL( "sprites/firebeam.spr" );
	PRECACHE_MODEL( "sprites/flame.spr" );
	PRECACHE_SOUND( "weapons/rocket1.wav" );
	m_iWhiteTrail = PRECACHE_MODEL( "sprites/white2.spr" );
}

void CDemomanRocket::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->classname = MAKE_STRING( "demoman_rocket" );
	pev->dmg = gSkillData.demomanDmgRocket;

	SET_MODEL( ENT(pev), "models/dmlrocket.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CDemomanRocket::IgniteThink );
	SetTouch( &CDemomanRocket::RocketExplodeTouch );
	pev->nextthink = gpGlobals->time + 0.1;

	m_fPercentSeek = 0.5;
}

void CDemomanRocket::RocketExplodeTouch( CBaseEntity *pOther )
{
	if ( m_pFlare )
		UTIL_Remove( m_pFlare );

	ExplodeTouch( pOther );
}

void CDemomanRocket::IgniteThink( void )
{
	CSprite *pFlare = CSprite::SpriteCreate( "sprites/flame.spr", pev->origin, TRUE );
	m_pFlare = pFlare;
	pFlare->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
	((CSprite *)m_pFlare)->SetAttachment( edict(), 0 );
	m_pFlare->pev->frame = 0;
	m_pFlare->pev->framerate = 10;
	((CSprite *)m_pFlare)->TurnOn();

	EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0.5, 0.5 );

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 8 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iWhiteTrail );	// model
		WRITE_BYTE( 9 ); // life
		WRITE_BYTE( 1 );  // width
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 150 );	// brightness
	MESSAGE_END();

	SetThink( &CDemomanRocket::AccelerateThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CDemomanRocket::AccelerateThink( void )
{
	// check world boundaries
	if ( pev->origin.x >= -4096 && pev->origin.x <= 4096 && pev->origin.y >= -4096 && pev->origin.y <= 4096 && pev->origin.z >= -4096 && pev->origin.z <= 4096 )
	{
		CBaseEntity *pOther = NULL;
		Vector vecTarget = g_vecZero;

		// steer toward the players near the rocket
		while ( ( pOther = UTIL_FindEntityInSphere( pOther, pev->origin, 1024 ) ) != NULL )
		{
			if ( pOther->pev->takedamage != DAMAGE_NO && pOther->IsPlayer() && ENT( pOther->pev ) != pev->owner && this != pOther )
			{
				vecTarget = vecTarget + ( pOther->pev->origin - pev->origin ).Normalize();
				vecTarget.x = vecTarget.x * 0.7;
				vecTarget[1] = vecTarget[1] * 0.7;
				vecTarget.Normalize();	// sic: the result is thrown away
			}
		}

		m_fPercentSeek = m_fPercentSeek * 0.95;

		if ( vecTarget != g_vecZero )
		{
			pev->velocity = pev->velocity.Normalize() + vecTarget.Normalize() * m_fPercentSeek;
		}

		pev->velocity = pev->velocity + pev->velocity.Normalize() * 50;

		if ( pev->velocity.Length() > 1500 )
			pev->velocity = pev->velocity.Normalize() * 1400;

		if ( pev->velocity.Length() < 1200 )
			pev->velocity = pev->velocity.Normalize() * 1200;

		pev->angles = UTIL_VecToAngles( pev->velocity );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		if ( m_pFlare )
			UTIL_Remove( m_pFlare );
		UTIL_Remove( this );
	}
}


//=========================================================
// monster-specific DEFINE's
//=========================================================
#define	DEMOMAN_AE_LAUNCH_ROCKET	1
#define	DEMOMAN_AE_DROP_MINE		3
#define	DEMOMAN_AE_SHOTGUN			6
#define	DEMOMAN_AE_DROP_GUN			11

//=========================================================
// the demoman
//=========================================================
class CDemoman : public CBaseBandit
{
public:
	void Spawn( void );
	void Precache( void );
	void PrecacheSounds( void );
	void SetYawSpeed( void );
	int  Classify( void );
	int  MonsterType( void )
	{
		if ( g_Language == LANGUAGE_GERMAN || CVAR_GET_FLOAT( "violence_germany" ) != 0 )
			return 2;
		return 1;
	}
	BOOL HasUnlimitedAmmo( void ) { return FALSE; }
	BOOL HasGrenades( void ) { return TRUE; }
	int GetGunsFiring( void ) { return 0; }
	void GetHeadGib( BOOL *pfHeadGib, int *piGroup, int *piHeadless )
	{
		*pfHeadGib = TRUE;
		*piGroup = 1;
		*piHeadless = 3;
	}
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	BOOL FCanCheckAttacks( void );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack2( float flDot, float flDist );
	BOOL FVisible( CBaseEntity *pEntity );
	BOOL FVisible( const Vector &vecOrigin );
	void RunTask( Task_t *pTask );
	void SetActivity( Activity NewActivity );
	void PainSound( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	Vector GetGunPosition( void );
	CBaseEntity *Kick( void );
	CDemomanRocket *ShootRocket( Vector vecSrc );
	Schedule_t *GetScheduleOfType( int Type );

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	static const char *pDemomanSounds[];

	float m_fThrowGrenade;		// a float here
	float m_flNextGrenadeCheck;	// the base's isn't used
	float m_flNextRocketTime;
	BOOL m_bHasAmmo;
};

LINK_ENTITY_TO_CLASS( monster_human_demoman, CDemoman );

TYPEDESCRIPTION	CDemoman::m_SaveData[] =
{
	DEFINE_FIELD( CDemoman, m_iSentence, FIELD_INTEGER ),
	DEFINE_FIELD( CDemoman, m_bHasAmmo, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CDemoman, CBaseBandit );

const char *CDemoman::pDemomanSounds[] =
{
	"demoman/demo_flinch1.wav",
	"demoman/demo_flinch2.wav",
	"demoman/demo_flinch3.wav",
	"demoman/demo_kick.wav",
	"demoman/demo_diebackwards.wav",
	"demoman/demo_dieforwards.wav",
	"demoman/demo_dieheadshot.wav",
	"demoman/demo_diesimple.wav",
	"demoman/demo_dieviolent.wav",
	"demoman/demo_dropmine.wav",
	"demoman/demo_launchrocket.wav",
	"demoman/demo_run_footstep1.wav",
	"demoman/demo_run_footstep2.wav",
	"demoman/demo_throwgrenade.wav",
	"demoman/demo_victorydance.wav",
	"demoman/demo_walk_footstep1.wav",
	"demoman/demo_walk_footstep2.wav",
	"demoman/demo_idlelong.wav",
	"weapons/sbarrel1.wav",
};

void CDemoman::PrecacheSounds( void )
{
	for ( int i = 0; i < ARRAYSIZE( pDemomanSounds ); i++ )
		PRECACHE_SOUND( (char *)pDemomanSounds[i] );
}

void CDemoman::PainSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "demoman/demo_flinch1.wav", 1, ATTN_NORM, 0, m_iVoicePitch );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "demoman/demo_flinch2.wav", 1, ATTN_NORM, 0, m_iVoicePitch );
		break;
	case 2:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "demoman/demo_flinch3.wav", 1, ATTN_NORM, 0, m_iVoicePitch );
		break;
	}
}

//=========================================================
// FVisible - as the monster's, but the demoman looks from
// his gun
//=========================================================
BOOL CDemoman::FVisible( CBaseEntity *pEntity )
{
	TraceResult tr;
	Vector		vecLookerOrigin;
	Vector		vecTargetOrigin;

	if ( FBitSet( pEntity->pev->flags, FL_NOTARGET ) )
		return FALSE;

	VisionFlags();	// sic: asked, then ignored

	// don't look through water
	if ((pev->waterlevel != 3 && pEntity->pev->waterlevel == 3)
		|| (pev->waterlevel == 3 && pEntity->pev->waterlevel == 0))
		return FALSE;

	vecLookerOrigin = GetGunPosition();
	vecTargetOrigin = pEntity->EyePosition();

	UTIL_TraceLine( vecLookerOrigin, vecTargetOrigin, ignore_monsters, ignore_glass, ENT(pev), &tr );

	if ( tr.flFraction != 1.0 )
		return FALSE;// Line of sight is not established
	else
		return TRUE;// line of sight is valid.
}

BOOL CDemoman::FVisible( const Vector &vecOrigin )
{
	TraceResult tr;
	Vector		vecLookerOrigin;

	vecLookerOrigin = GetGunPosition();

	UTIL_TraceLine( vecLookerOrigin, vecOrigin, ignore_monsters, ignore_glass, ENT(pev), &tr );

	if ( tr.flFraction != 1.0 )
		return FALSE;// Line of sight is not established
	else
		return TRUE;// line of sight is valid.
}

void CDemoman::Precache( void )
{
	PRECACHE_MODEL( "models/demolitionman.mdl" );
	UTIL_PrecacheOther( "ammo_dmlsingle" );
	UTIL_PrecacheOther( "demoman_rocket" );
	UTIL_PrecacheOther( "demoman_mine" );
	CBaseBandit::Precache( );
}

void CDemoman::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT(pev), "models/demolitionman.mdl" );
	pev->health = gSkillData.demomanHealth;
	m_flNextRocketTime = m_flLastEnemySightTime = gpGlobals->time;

	if ( UTIL_PercentChance( 70 ) )
	{
		pev->skin = 0;
		pev->body = RANDOM_LONG( 0, 2 );
	}
	else
	{
		pev->skin = 1;
	}

	MonsterInit( );
	pev->view_ofs = Vector( 0, 0, 68 );
	CBaseBandit::Spawn( );

	switch ( g_iSkillLevel )
	{
	case SKILL_MEDIUM:
		m_cClipSize = 4;
		break;
	case SKILL_HARD:
		m_cClipSize = 6;
		break;
	default:
		m_cClipSize = 3;
		break;
	}

	m_bHasAmmo = TRUE;
	m_cAmmoLoaded = m_cClipSize;
}

void CDemoman::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( m_bHasAmmo )
	{
		m_bHasAmmo = FALSE;

		// sometimes drop a rocket
		if ( UTIL_PercentChance( 30 ) )
		{
			Vector vecGunPos;
			Vector vecGunAngles;

			GetAttachment( 0, vecGunPos, vecGunAngles );

			CBaseEntity *pGun = DropItem( "ammo_dmlsingle", vecGunPos, vecGunAngles );
			if ( pGun )
			{
				pGun->pev->velocity = Vector( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 ) );
				pGun->pev->avelocity = Vector( 0, RANDOM_FLOAT( 200, 400 ), 0 );
			}
		}
	}

	CBaseBandit::Killed( pevAttacker, iGib );
}

BOOL CDemoman::FCanCheckAttacks( void )
{
	if ( !HasConditions( bits_COND_ENEMY_TOOFAR ) )
		return TRUE;
	else
		return FALSE;
}

BOOL CDemoman::CheckMeleeAttack1( float flDot, float flDist )
{
	if ( m_hEnemy != NULL && flDist <= 64 && flDot >= 0.7 )
		return TRUE;
	return FALSE;
}

//=========================================================
// CheckRangeAttack1 - the shotgun and the rocket launcher;
// an enemy this close makes the demoman think of a mine
//=========================================================
BOOL CDemoman::CheckRangeAttack1( float flDot, float flDist )
{
	if ( flDist < 4096 && flDot > 0.5 && NoFriendlyFire() && flDist > 64 && ( HasUnlimitedAmmo() || !HasConditions( bits_COND_NO_AMMO_LOADED ) ) )
	{
		if ( flDist < 256 )
			m_flNextRocketTime = gpGlobals->time + 5;

		TraceResult tr;
		UTIL_TraceLine( GetGunPosition(), m_hEnemy->BodyTarget( GetGunPosition() ), dont_ignore_monsters, ignore_glass, ENT(pev), &tr );

		if ( tr.flFraction == 1 || tr.pHit == m_hEnemy->edict() )
			return TRUE;
	}

	return FALSE;
}

//=========================================================
// CheckRangeAttack2 - drop a mine where the enemy will be
//=========================================================
BOOL CDemoman::CheckRangeAttack2( float flDot, float flDist )
{
	if ( flDist > 1024 )
	{
		m_flNextGrenadeCheck = gpGlobals->time + 1;
		return FALSE;
	}

	// assume things haven't changed too much since last time
	if ( gpGlobals->time < m_flNextGrenadeCheck )
		return m_fThrowGrenade;

	if ( !FBitSet( m_hEnemy->pev->flags, FL_ONGROUND ) && m_hEnemy->pev->waterlevel == 0 && m_vecEnemyLKP.z > pev->absmax.z )
	{
		// don't throw at anything that isn't on the ground!
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	Vector vecTarget;

	// find feet
	if ( RANDOM_LONG( 0, 1 ) )
	{
		// magically know where they are
		vecTarget = Vector( m_hEnemy->pev->origin.x, m_hEnemy->pev->origin.y, m_hEnemy->pev->absmin.z );
	}
	else
	{
		// toss it to where you last saw them
		vecTarget = m_vecEnemyLKP;
	}

	// are any of my squad members near the intended impact area?
	if ( InSquad() )
	{
		if ( SquadMemberInRange( vecTarget, 256 ) )
		{
			// crap, I might blow my own guy up. Don't throw and don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
			m_fThrowGrenade = FALSE;
		}
	}

	if ( ( vecTarget - pev->origin ).Length2D() <= 256 )
	{
		// crap, I don't want to blow myself up
		m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	if ( RANDOM_LONG( 0, 10 ) > 7 )
	{
		Vector vecToss = VecCheckToss( pev, GetGunPosition(), vecTarget, 0.5 );

		if ( vecToss != g_vecZero )
		{
			m_vecTossVelocity = vecToss;
			m_fThrowGrenade = TRUE;
			m_flNextGrenadeCheck = gpGlobals->time + 0.3;
		}
		else
		{
			m_fThrowGrenade = FALSE;
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		}
	}
	else
	{
		Vector vecToss = VecCheckThrow( pev, GetGunPosition(), vecTarget, gSkillData.humanGrenadeSpeed, 0.5 );

		if ( vecToss != g_vecZero )
		{
			m_vecTossVelocity = vecToss;
			m_fThrowGrenade = TRUE;
			m_flNextGrenadeCheck = gpGlobals->time + 0.3;
		}
		else
		{
			m_fThrowGrenade = FALSE;
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		}
	}

	return m_fThrowGrenade;
}

void CDemoman::SetYawSpeed( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
		ys = 100;
		break;
	case ACT_RUN:
		ys = 100;
		break;
	case ACT_WALK:
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 130;
		break;
	case ACT_RANGE_ATTACK1:
	case ACT_RANGE_ATTACK2:
	case ACT_MELEE_ATTACK1:
	case ACT_MELEE_ATTACK2:
		ys = 80;
		break;
	case ACT_GLIDE:
	case ACT_FLY:
		ys = 20;
		break;
	default:
		ys = 100;
		break;
	}

	pev->yaw_speed = ys;
}

int CDemoman::Classify( void )
{
	return CLASS_HUMAN_MILITARY;
}

//=========================================================
// ShootRocket - fires a rocket from the given spot
//=========================================================
CDemomanRocket *CDemoman::ShootRocket( Vector vecSrc )
{
	RangeAttackSound( );
	UTIL_MakeAimVectors( pev->angles );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecSrc.x );
		WRITE_COORD( vecSrc.y );
		WRITE_COORD( vecSrc.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 13 ); // scale * 10
		WRITE_BYTE( 12 ); // framerate
	MESSAGE_END();

	CDemomanRocket *pRocket = GetClassPtr( (CDemomanRocket *)NULL );
	pRocket->pev->angles = pev->angles;
	pRocket->Spawn( );
	UTIL_SetOrigin( pRocket->pev, vecSrc );
	pRocket->pev->velocity = pev->velocity + gpGlobals->v_forward * 100;

	return pRocket;
}

//=========================================================
// Kick - the base's, copied; nothing calls it
//=========================================================
CBaseEntity *CDemoman::Kick( void )
{
	TraceResult tr;

	UTIL_MakeVectors( pev->angles );
	Vector vecStart = pev->origin;
	vecStart.z += pev->size.z * 0.5;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * 70);

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr );

	if ( tr.pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		return pEntity;
	}

	return NULL;
}

//=========================================================
// GetGunPosition - the gun's at the shoulder
//=========================================================
Vector CDemoman::GetGunPosition( void )
{
	UTIL_MakeVectors( pev->angles );
	return pev->origin + gpGlobals->v_right * 10 + gpGlobals->v_up * 40;
}

void CDemoman::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	Vector vecSrc;
	Vector vecAngles;

	switch ( pEvent->event )
	{
	case DEMOMAN_AE_SHOTGUN:
		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/sbarrel1.wav", 1, ATTN_NORM );
		Shotgun( );
		break;

	case DEMOMAN_AE_LAUNCH_ROCKET:
		GetAttachment( 0, vecSrc, vecAngles );
		if ( UTIL_PointContents( vecSrc ) == CONTENTS_EMPTY )
			ShootRocket( vecSrc );
		break;

	case DEMOMAN_AE_DROP_GUN:
		GetAttachment( 0, vecSrc, vecAngles );
		SetBodygroup( 2, 2 );
		break;

	case DEMOMAN_AE_DROP_MINE:
		GetAttachment( 0, vecSrc, vecAngles );
		if ( UTIL_PointContents( vecSrc ) == CONTENTS_EMPTY )
		{
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "demoman/demo_dropmine.wav", 1, ATTN_NORM );
			CBaseEntity *pMine = DropItem( "demoman_mine", vecSrc, vecAngles );
			pMine->pev->owner = edict();
			pMine->Spawn( );
		}
		break;

	default:
		CBaseBandit::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// GetScheduleOfType - rockets at a distance, when they're
// ready; the shotgun otherwise
//=========================================================
Schedule_t *CDemoman::GetScheduleOfType( int Type )
{
	if ( Type != SCHED_RANGE_ATTACK1 )
		return CBaseBandit::GetScheduleOfType( Type );

	if ( m_hEnemy != NULL && ( m_hEnemy->pev->origin - pev->origin ).Length() > 128 && m_flNextRocketTime < gpGlobals->time )
	{
		m_flNextRocketTime = gpGlobals->time + RANDOM_LONG( 5, 8 );
		pev->weapons = BANDIT_ROCKETLAUNCHER;
		return GetScheduleOfType( SCHED_BANDIT_RANGE_ROCKET );
	}

	pev->weapons = BANDIT_SHOTGUN;
	return GetScheduleOfType( SCHED_BANDIT_RANGE_SHOTGUN );
}

void CDemoman::RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_BANDIT_FACE_TOSS_DIR:
		{
			// project a point along the toss vector and turn to face that point.
			MakeIdealYaw( pev->origin + m_vecTossVelocity * 64 );
			ChangeYaw( pev->yaw_speed );

			if ( FacingIdeal() )
			{
				m_iTaskStatus = TASKSTATUS_COMPLETE;
			}
			break;
		}
	default:
		{
			CBaseBandit::RunTask( pTask );
			break;
		}
	}
}

void CDemoman::SetActivity( Activity NewActivity )
{
	int	iSequence;

	switch ( NewActivity )
	{
	case ACT_RANGE_ATTACK1:
		if ( pev->weapons == BANDIT_SHOTGUN )
			iSequence = LookupSequence( "shootshotgun" );
		else
			iSequence = LookupSequence( "launchrocket" );
		break;
	case ACT_WALK:
		if ( pev->health <= gSkillData.demomanLimpHealth )
		{
			// limp!
			iSequence = LookupActivity( ACT_WALK_HURT );
		}
		else
		{
			iSequence = LookupActivity( NewActivity );
		}
		break;
	case ACT_IDLE:
		if ( m_MonsterState == MONSTERSTATE_COMBAT )
		{
			NewActivity = ACT_IDLE_ANGRY;
		}
		iSequence = LookupActivity( NewActivity );
		break;
	default:
		iSequence = LookupActivity( NewActivity );
		break;
	}

	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		ALERT ( at_console, "%s has no sequence for act:%d\n", STRING(pev->classname), NewActivity );
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}
