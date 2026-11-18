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
// the maggot: a crawling grub that leaps at its enemies
// and squashes underfoot, and the remains it leaves.
// Written from the cricket and the headcrab.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"squadmonster.h"
#include	"decals.h"
#include	"maggot.h"

extern DLL_GLOBAL int g_iSkillLevel;

//=========================================================
// maggot_gib - the dead maggot, lying where it was gibbed
// until its animation ends and it fades away
//=========================================================
class CMaggotGib : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void ) { PRECACHE_MODEL( "models/deadmaggot.mdl" ); }
	int  Classify( void ) { return CLASS_ALIEN_TUBE; }

	void EXPORT FlopAnim( void );
};

void CMaggotGib::FlopAnim( void )
{
	StudioFrameAdvance( );

	if ( m_fSequenceFinished )
	{
		pev->framerate = 0;
		SetThink( &CBaseEntity::SUB_StartFadeOut );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

LINK_ENTITY_TO_CLASS( maggot_gib, CMaggotGib );

void CMaggotGib::Spawn( void )
{
	pev->model = MAKE_STRING( "models/deadmaggot.mdl" );
	pev->classname = MAKE_STRING( "maggot_gib" );
	SET_MODEL( ENT(pev), "models/deadmaggot.mdl" );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;
	pev->effects = 0;
	pev->takedamage = DAMAGE_NO;

	SetThink( &CMaggotGib::FlopAnim );
	SetSequenceByName( "dead" );
	SetTouch( NULL );
	SetUse( NULL );
	SetBlocked( NULL );

	pev->nextthink = gpGlobals->time;
}

//=========================================================
// monster_maggot
//=========================================================
LINK_ENTITY_TO_CLASS( monster_maggot, CMaggot );

Task_t	tlMaggotRAFast[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_MELEE_ATTACK1,		(float)0		},
	{ TASK_SET_ACTIVITY_IDLE,	(float)0		},
};

Schedule_t	slMaggotRAFast[] =
{
	{
		tlMaggotRAFast,
		ARRAYSIZE ( tlMaggotRAFast ),
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED,
		0,
		"HCRAFast"
	},
};

TYPEDESCRIPTION	CMaggot::m_SaveData[] =
{
	DEFINE_FIELD( CMaggot, m_bCanDodge, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMaggot, m_iCritterKillTime, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CMaggot, CSquadMonster );

DEFINE_CUSTOM_SCHEDULES( CMaggot )
{
	slMaggotRAFast,
};

IMPLEMENT_CUSTOM_SCHEDULES( CMaggot, CSquadMonster );

const char *CMaggot::pMaggotSounds[] =
{
	"maggot/maggot_angry1.wav",
	"maggot/maggot_angry2.wav",
	"maggot/maggot_flinch1.wav",
	"maggot/maggot_flinch2.wav",
	"maggot/maggot_idle1.wav",
	"maggot/maggot_idle2.wav",
	"maggot/maggot_idle3.wav",
	"maggot/maggot_walk1.wav",
	"maggot/maggot_run1.wav",
	"cricket/cricket_splat1.wav",
	"cricket/cricket_splat2.wav",
	"cricket/cricket_splat3.wav",
	"debris/bustflesh2.wav",
};

void CMaggot::PrecacheSounds( void )
{
	for ( int i = 0; i < ARRAYSIZE( pMaggotSounds ); i++ )
		PRECACHE_SOUND( (char *)pMaggotSounds[i] );

	m_iSplashSprite = PRECACHE_MODEL( "sprites/greensplash.spr" );
}

void CMaggot::IdleSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "maggot/maggot_idle1.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "maggot/maggot_idle2.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	case 2:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "maggot/maggot_idle3.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	}
}

void CMaggot::AlertSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "maggot/maggot_angry1.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "maggot/maggot_angry2.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	}
}

void CMaggot::PainSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "maggot/maggot_flinch1.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "maggot/maggot_flinch2.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	}
}

void CMaggot::DeathSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "cricket/cricket_splat1.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "cricket/cricket_splat2.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	case 2:
		EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "cricket/cricket_splat3.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
		break;
	}
}

void CMaggot::AttackHitSound( CBaseMonster *pVictim )
{
	EMIT_SOUND_DYN( ENT(pVictim->pev), CHAN_VOICE, "debris/bustflesh2.wav", 0.4, ATTN_IDLE, 0, 120 );
}

void CMaggot::GibSound( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "common/smallsplat.wav", 0.3, ATTN_NORM, 0, PITCH_NORM );
}

//=========================================================
// Center - the maggot's middle, a little above its belly
//=========================================================
Vector CMaggot::Center( void )
{
	return Vector( pev->origin.x, pev->origin.y, pev->origin.z + 10 );
}

void CMaggot::SetYawSpeed( void )
{
	pev->yaw_speed = 60;
}

//=========================================================
// TakeDamage - any hit gibs the maggot
//=========================================================
int CMaggot::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, DMG_ALWAYSGIB );
}

//=========================================================
// GibMonster - a maggot that crawled leaves its body; one
// the critter spat leaves nothing
//=========================================================
void CMaggot::GibMonster( void )
{
	if ( m_iCritterKillTime == -1 )
	{
		CMaggotGib *pGib = GetClassPtr( (CMaggotGib *)NULL );
		pGib->Spawn( );
		pGib->pev->origin = pev->origin;
		pGib->pev->angles = pev->angles;
	}

	CBaseMonster::GibMonster( );
}

void CMaggot::Precache( void )
{
	PRECACHE_MODEL( "models/maggot.mdl" );
	PRECACHE_MODEL( "models/larva.mdl" );
	UTIL_PrecacheOther( "maggot_gib" );
	CBaseMonster::Precache( );
}

void CMaggot::Spawn( void )
{
	Precache( );

	pev->spawnflags |= SF_MONSTER_FALL_TO_GROUND;
	SET_MODEL( ENT(pev), "models/maggot.mdl" );
	pev->solid = SOLID_SLIDEBOX;
	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 8 ) );

	pev->health = gSkillData.maggotHealth;
	m_flNextDodgeTime = gpGlobals->time;
	m_bCanDodge = UTIL_PercentChance( 50 );
	m_iCritterKillTime = -1;
	m_fLaunched = FALSE;
	pev->owner = NULL;

	MonsterInit( );

	pev->view_ofs = Vector( 0, 0, 20 );
	m_afCapability |= bits_CAP_MELEE_ATTACK1;

	SetTouch( &CMaggot::SquashTouch );
}

//=========================================================
// RunAI - a launched maggot dies after a while and trails
// slime while it flies; when its enemy looks straight at
// it, a maggot that can dodge jumps aside, or, nearly
// straight, detours on its way
//=========================================================
void CMaggot::RunAI( void )
{
	CBaseMonster::RunAI( );

	if ( m_iCritterKillTime >= 0 )
	{
		if ( m_iCritterKillTime == 0 )
			TakeDamage( pev, pev, 1000, DMG_CRUSH );
		else
			m_iCritterKillTime--;
	}

	if ( m_fLaunched )
	{
		Vector vecDir;
		vecDir = pev->velocity.Normalize( );

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SPRITE_SPRAY );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_COORD( vecDir.x );
			WRITE_COORD( vecDir.y );
			WRITE_COORD( vecDir.z );
			WRITE_SHORT( m_iSplashSprite );
			WRITE_BYTE( 1 );
			WRITE_BYTE( pev->velocity.Length( ) * 1.25 );
			WRITE_BYTE( 10 );
		MESSAGE_END( );
	}

	if ( gpGlobals->time < m_flNextDodgeTime )
		return;

	if ( m_hEnemy != NULL )
	{
		Vector vecDir;
		vecDir = m_vecEnemyLKP - pev->origin;
		UTIL_MakeAimVectors( m_hEnemy->pev->angles );
		Vector vecForward = gpGlobals->v_forward;
		vecDir = vecDir.Normalize( );
		vecDir.x = vecDir.x * 1;	// sic: a no-op; the named vecDir.x gives retail's operand order in the dot product
		vecForward = vecForward.Normalize( );
		vecForward = vecForward * -1;
		float flDot = DotProduct( vecForward, vecDir );
		Vector vecApex;

		if ( flDot > 0.97 && m_bCanDodge == TRUE )
		{
			// jump aside, turning 30 degrees past the enemy
			Schedule_t *pSchedule = GetScheduleOfType( SCHED_FAIL );
			m_flNextDodgeTime = gpGlobals->time + RANDOM_LONG( 2, 4 );

			float flYaw = VecToYaw( m_hEnemy->pev->origin - pev->origin ) - pev->angles.y;

			if ( flYaw > 180 )
				flYaw -= 360;
			if ( flYaw < -180 )
				flYaw += 360;

			if ( flYaw < 0 )
			{
				flYaw -= 30;
				if ( flYaw >= 0 )
					flYaw = 0;
			}
			else
			{
				flYaw += 30;
				if ( flYaw <= 0 )
					flYaw = 0;
			}

			pev->ideal_yaw = flYaw + pev->angles.y;

			if ( pSchedule )
				ChangeSchedule( pSchedule );
		}
		else if ( flDot > 0.9 && m_bCanDodge == TRUE && m_hEnemy != NULL && IsMoving( ) &&
			m_hEnemy->pev == m_hEnemy->pev &&	// sic: always true
			( pev->origin - m_hEnemy->pev->origin ).Length2D( ) > 128 )
		{
			if ( FTriangulate( pev->origin, m_Route[ m_iRouteIndex ].vecLocation, ( pev->origin - m_Route[ m_iRouteIndex ].vecLocation ).Length2D( ) * 0.5, m_hEnemy, &vecApex ) )
			{
				InsertWaypoint( vecApex, bits_MF_TO_DETOUR | bits_MF_DONT_SIMPLIFY );
			}
		}
	}
}

void CMaggot::RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_MELEE_ATTACK1:
		if ( m_fSequenceFinished )
		{
			TaskComplete( );
			SetTouch( &CMaggot::SquashTouch );
			m_IdealActivity = ACT_IDLE;
		}
		break;
	default:
		CBaseMonster::RunTask( pTask );
		break;
	}
}

void CMaggot::Launch( void )
{
	SetTouch( &CMaggot::LaunchedTouch );
	SET_MODEL( ENT(pev), "models/larva.mdl" );
	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 8 ) );
	UTIL_SetOrigin( pev, pev->origin );
	pev->health = 1;
	m_iCritterKillTime = 40;
	m_fLaunched = TRUE;
}

//=========================================================
// LaunchedTouch - the spat larva bites what it hits, or
// splats on the wall; on hard, one in three lives on
//=========================================================
void CMaggot::LaunchedTouch( CBaseEntity *pOther )
{
	m_fLaunched = FALSE;

	if ( pOther && !FClassnameIs( pOther->pev, "monster_critter" ) && pOther->IsAlive( ) )
	{
		pOther->TakeDamage( pev, pev, gSkillData.critterDmgSpit, DMG_SLASH );

		CBaseMonster *pMonster = pOther->MyMonsterPointer( );
		if ( pMonster )
		{
			TraceResult tr;
			UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
			pMonster->TraceBleed( gSkillData.critterDmgSpit, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize( ), &tr, DMG_SLASH, tr.vecEndPos );

			if ( pMonster->IsPlayer( ) )
				AttackHitSound( pMonster );
		}

		SetTouch( NULL );
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	TraceResult tr;
	UTIL_TraceLine( pev->origin, pev->origin + pev->velocity * 128, ignore_monsters, ENT(pev), &tr );
	UTIL_DecalTrace( &tr, DECAL_MAGGOT );

	if ( g_iSkillLevel == SKILL_HARD && !UTIL_PercentChance( 65 ) )
	{
		pev->avelocity = g_vecZero;
		SetTouch( &CMaggot::SquashTouch );
		return;
	}

	SetTouch( NULL );
	TakeDamage( pev, pev, 1000, DMG_CRUSH );
}

//=========================================================
// SquashTouch - a player that touches the maggot while it
// is on the ground squashes it
//=========================================================
void CMaggot::SquashTouch( CBaseEntity *pOther )
{
	if ( pOther && pOther->IsPlayer( ) && FBitSet( pev->flags, FL_ONGROUND ) )
	{
		TakeDamage( pOther->pev, pOther->pev, 100, DMG_CRUSH );
	}
}

//=========================================================
// LeapTouch - the headcrab's bite
//=========================================================
void CMaggot::LeapTouch( CBaseEntity *pOther )
{
	m_fLaunched = FALSE;

	if ( !pOther->pev->takedamage )
	{
		return;
	}

	if ( pOther->Classify( ) == Classify( ) )
	{
		return;
	}

	// Don't hit if back on ground
	if ( !FBitSet( pev->flags, FL_ONGROUND ) )
	{
		pOther->TakeDamage( pev, pev, gSkillData.maggotDmg, DMG_SLASH );

		CBaseMonster *pMonster = pOther->MyMonsterPointer( );
		if ( pMonster )
		{
			TraceResult tr;
			UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT(pev), &tr );
			pMonster->TraceBleed( gSkillData.maggotDmg, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize( ), &tr, DMG_SLASH, tr.vecEndPos );

			if ( pMonster->IsPlayer( ) )
				AttackHitSound( pMonster );
		}
	}

	SetTouch( &CMaggot::SquashTouch );
}

//=========================================================
// PrescheduleThink - squeak a little in combat
//=========================================================
void CMaggot::PrescheduleThink( void )
{
	if ( m_MonsterState == MONSTERSTATE_COMBAT && UTIL_PercentChance( 10 ) )
	{
		IdleSound( );
	}
}

void CMaggot::StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_MELEE_ATTACK1:
	case TASK_MELEE_ATTACK2:
		{
			m_IdealActivity = ACT_LEAP;
			SetTouch( &CMaggot::LeapTouch );

			// the headcrab's jump
			ClearBits( pev->flags, FL_ONGROUND );

			UTIL_SetOrigin( pev, pev->origin + Vector( 0, 0, 1 ) );// take him off ground so engine doesn't instantly reset onground
			UTIL_MakeVectors( pev->angles );

			Vector vecJumpDir;
			if ( m_hEnemy != NULL )
			{
				float gravity = CVAR_GET_FLOAT( "sv_gravity" );
				if ( gravity <= 1 )
					gravity = 1;

				// How fast does the maggot need to travel to reach that height given gravity?
				float height = ( m_hEnemy->pev->origin.z + m_hEnemy->pev->view_ofs.z - pev->origin.z );
				if ( height < 8 )
					height = 8;
				float speed = sqrt( 2 * gravity * height );
				float time = speed / gravity;

				// Scale the sideways velocity to get there at the right time
				vecJumpDir = ( m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs - pev->origin );
				vecJumpDir = vecJumpDir * ( 1.0 / time );

				// Speed to offset gravity at the desired height
				vecJumpDir.z = speed;

				// Don't jump too far/fast
				float distance = vecJumpDir.Length( );

				if ( distance > 350 )
				{
					vecJumpDir = vecJumpDir * ( 350.0 / distance );
				}
			}
			else
			{
				// jump hop, don't care where
				vecJumpDir = Vector( gpGlobals->v_forward.x, gpGlobals->v_forward.y, gpGlobals->v_up.z ) * 350;
			}

			int iSound = RANDOM_LONG( 0, 2 );
			if ( iSound != 0 )
				AlertSound( );

			pev->velocity = vecJumpDir;
			m_flNextAttack = gpGlobals->time + 2;
			break;
		}
	default:
		CBaseMonster::StartTask( pTask );
		break;
	}
}

BOOL CMaggot::CheckMeleeAttack1( float flDot, float flDist )
{
	if ( FBitSet( pev->flags, FL_ONGROUND ) && flDist <= 128 && flDot >= 0.65 )
	{
		return TRUE;
	}
	return FALSE;
}

Schedule_t *CMaggot::GetScheduleOfType( int Type )
{
	switch ( Type )
	{
	case SCHED_MELEE_ATTACK1:
	case SCHED_MELEE_ATTACK2:
		return slMaggotRAFast;

	case SCHED_FAIL:
		if ( m_hEnemy != NULL )
			return slMaggotRAFast;
		break;
	}

	return CSquadMonster::GetScheduleOfType( Type );
}

int CMaggot::Classify( void )
{
	return CLASS_ALIEN_CRITTER;
}

int CMaggot::MonsterType( void )
{
	return 21;
}
