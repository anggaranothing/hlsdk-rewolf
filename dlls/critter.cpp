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
// the critter: a small spitting creature, and the
// remains it leaves. It's the SDK's bullsquid, reworked:
// it spits maggots instead of acid and swerves when its
// enemy looks at it.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"nodes.h"
#include	"soundent.h"
#include	"squadmonster.h"
#include	"maggot.h"

#define		CRITTER_SPRINT_DIST	512 // how close the critter has to get before starting to sprint and refusing to swerve

int			iCritterSpitSprite;

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_CRITTER_SEECRAB = LAST_COMMON_SCHEDULE + 1,
	SCHED_CRITTER_EAT,
	SCHED_CRITTER_SNIFF_AND_EAT,
	SCHED_CRITTER_WALLOW,
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		CRITTER_AE_BITE		( 1 )
#define		CRITTER_AE_SPIT		( 2 )

#define		CRITTER_ATTN_IDLE	(float)1.5

//=========================================================
// critter_gib - the critter's remains, flopping until
// their sequence ends
//=========================================================
class CCritterGib : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void ) { PRECACHE_MODEL( "models/crittergib.mdl" ); }
	int  Classify( void ) { return CLASS_ALIEN_TUBE; }
	void EXPORT FlopAnim( void );
};

void CCritterGib::FlopAnim( void )
{
	StudioFrameAdvance( );

	if ( m_fSequenceFinished )
	{
		SetThink( NULL );
		pev->framerate = 0;
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

LINK_ENTITY_TO_CLASS( critter_gib, CCritterGib );

void CCritterGib::Spawn( void )
{
	pev->model = MAKE_STRING( "models/crittergib.mdl" );
	pev->classname = MAKE_STRING( "critter_gib" );
	SET_MODEL( ENT( pev ), "models/crittergib.mdl" );
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	pev->effects = 0;
	pev->takedamage = DAMAGE_NO;
	SetThink( &CCritterGib::FlopAnim );
	SetSequenceByName( "flop" );
	SetTouch( NULL );
	SetUse( NULL );
	SetBlocked( NULL );
	pev->nextthink = gpGlobals->time;
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

// primary range attack
Task_t	tlCritterRangeAttack1[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_SET_ACTIVITY_IDLE,	(float)0		},
};

Schedule_t	slCritterRangeAttack1[] =
{
	{ 
		tlCritterRangeAttack1,
		ARRAYSIZE ( tlCritterRangeAttack1 ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED,
		0,
		"Squid Range Attack1"
	},
};

// Chase enemy schedule
Task_t tlCritterChaseEnemy1[] = 
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_RANGE_ATTACK1	},// !!!OEM - this will stop nasty squid oscillation.
	{ TASK_GET_PATH_TO_ENEMY,	(float)0					},
	{ TASK_RUN_PATH,			(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0					},
};

Schedule_t slCritterChaseEnemy[] =
{
	{ 
		tlCritterChaseEnemy1,
		ARRAYSIZE ( tlCritterChaseEnemy1 ),
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_SMELL_FOOD		|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK2	|
		bits_COND_TASK_FAILED		|
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER			|
		bits_SOUND_MEAT,
		"Squid Chase Enemy"
	},
};

Task_t tlCritterSeeCrab[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_SOUND_WAKE,			(float)0			},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_EXCITED	},
	{ TASK_FACE_ENEMY,			(float)0			},
};

Schedule_t slCritterSeeCrab[] =
{
	{
		tlCritterSeeCrab,
		ARRAYSIZE ( tlCritterSeeCrab ),
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE,
		0,
		"SquidSeeCrab"
	}
};

// critter walks to something tasty and eats it.
Task_t tlCritterEat[] =
{
	{ TASK_STOP_MOVING,				(float)0				},
	{ TASK_EAT,						(float)10				},// this is in case the critter can't get to the food
	{ TASK_STORE_LASTPOSITION,		(float)0				},
	{ TASK_GET_PATH_TO_BESTSCENT,	(float)0				},
	{ TASK_WALK_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			},
	{ TASK_EAT,						(float)50				},
	{ TASK_GET_PATH_TO_LASTPOSITION,(float)0				},
	{ TASK_WALK_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_CLEAR_LASTPOSITION,		(float)0				},
};

Schedule_t slCritterEat[] =
{
	{
		tlCritterEat,
		ARRAYSIZE( tlCritterEat ),
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_NEW_ENEMY	,
		
		// even though HEAR_SOUND/SMELL FOOD doesn't break this schedule, we need this mask
		// here or the monster won't detect these sounds at ALL while running this schedule.
		bits_SOUND_MEAT			|
		bits_SOUND_CARCASS,
		"SquidEat"
	}
};

// this is a bit different than just Eat. We use this schedule when the food is far away, occluded, or behind
// the critter. This schedule plays a sniff animation before going to the source of food.
Task_t tlCritterSniffAndEat[] =
{
	{ TASK_STOP_MOVING,				(float)0				},
	{ TASK_EAT,						(float)10				},// this is in case the critter can't get to the food
	{ TASK_PLAY_SEQUENCE,			(float)ACT_DETECT_SCENT },
	{ TASK_STORE_LASTPOSITION,		(float)0				},
	{ TASK_GET_PATH_TO_BESTSCENT,	(float)0				},
	{ TASK_WALK_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			},
	{ TASK_EAT,						(float)50				},
	{ TASK_GET_PATH_TO_LASTPOSITION,(float)0				},
	{ TASK_WALK_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_CLEAR_LASTPOSITION,		(float)0				},
};

Schedule_t slCritterSniffAndEat[] =
{
	{
		tlCritterSniffAndEat,
		ARRAYSIZE( tlCritterSniffAndEat ),
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_NEW_ENEMY	,
		
		// even though HEAR_SOUND/SMELL FOOD doesn't break this schedule, we need this mask
		// here or the monster won't detect these sounds at ALL while running this schedule.
		bits_SOUND_MEAT			|
		bits_SOUND_CARCASS,
		"SquidSniffAndEat"
	}
};

// critter does this to stinky things. 
Task_t tlCritterWallow[] =
{
	{ TASK_STOP_MOVING,				(float)0				},
	{ TASK_EAT,						(float)10				},// this is in case the critter can't get to the stinkiness
	{ TASK_STORE_LASTPOSITION,		(float)0				},
	{ TASK_GET_PATH_TO_BESTSCENT,	(float)0				},
	{ TASK_WALK_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_INSPECT_FLOOR},
	{ TASK_EAT,						(float)50				},// keeps critter from eating or sniffing anything else for a while.
	{ TASK_GET_PATH_TO_LASTPOSITION,(float)0				},
	{ TASK_WALK_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_CLEAR_LASTPOSITION,		(float)0				},
};

Schedule_t slCritterWallow[] =
{
	{
		tlCritterWallow,
		ARRAYSIZE( tlCritterWallow ),
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_NEW_ENEMY	,
		
		// even though HEAR_SOUND/SMELL FOOD doesn't break this schedule, we need this mask
		// here or the monster won't detect these sounds at ALL while running this schedule.
		bits_SOUND_GARBAGE,

		"SquidWallow"
	}
};

//=========================================================
// monster_critter
//=========================================================
class CCritter : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void PrecacheSounds( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	int  Classify( void ) { return CLASS_ALIEN_CRITTER; }
	int  MonsterType( void ) { return 17; }
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void IdleSound( void );
	void AlertSound( void );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckMeleeAttack2( float flDot, float flDist );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	void RunAI( void );
	BOOL FValidateHintType( short sHint );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void GibMonster( void );

	int	Save( CSave &save ); 
	int Restore( CRestore &restore );

	CUSTOM_SCHEDULES;
	static TYPEDESCRIPTION m_SaveData[];

	static const char *pCritterSounds[];

	float m_flLastHurtTime;		// saved, but nothing sets it any more
	float m_flNextSpitTime;		// last time the critter used the spit attack.
	float m_flLastSwerveTime;	// when the critter may swerve away from its enemy's aim again
};
LINK_ENTITY_TO_CLASS( monster_critter, CCritter );

TYPEDESCRIPTION	CCritter::m_SaveData[] = 
{
	DEFINE_FIELD( CCritter, m_flLastHurtTime, FIELD_TIME ),
	DEFINE_FIELD( CCritter, m_flNextSpitTime, FIELD_TIME ),
	DEFINE_FIELD( CCritter, m_flLastSwerveTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CCritter, CBaseMonster );

DEFINE_CUSTOM_SCHEDULES( CCritter ) 
{
	slCritterRangeAttack1,
	slCritterChaseEnemy,
	slCritterSeeCrab,
	slCritterEat,
	slCritterSniffAndEat,
	slCritterWallow
};

IMPLEMENT_CUSTOM_SCHEDULES( CCritter, CBaseMonster );

const char *CCritter::pCritterSounds[] =
{
	"critter/Critter_DieSimple.wav",
	"critter/Critter_GAttack_F0.wav",
	"critter/Critter_Gib_Flop.wav",
	"critter/Critter_Idle1_F0.wav",
	"critter/Critter_Idle2_F0.wav",
	"critter/Critter_LargeFlinch_F0.wav",
	"critter/Critter_Scurry.wav",
	"critter/Critter_SmallFlinch_F0.wav",
	"critter/Critter_SeePlayer.wav",
	"critter/Critter_SpitAttack_F0.wav",
};

void CCritter::PrecacheSounds( void )
{
	for ( int i = 0; i < ARRAYSIZE( pCritterSounds ); i++ )
		PRECACHE_SOUND( (char *)pCritterSounds[i] );
}

//=========================================================
// IdleSound 
//=========================================================
void CCritter::IdleSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:	
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "critter/Critter_Idle1_F0.wav", 1, CRITTER_ATTN_IDLE );	
		break;
	case 1:	
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "critter/Critter_Idle2_F0.wav", 1, CRITTER_ATTN_IDLE );	
		break;
	}
}

//=========================================================
// AlertSound
//=========================================================
void CCritter::AlertSound( void )
{
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "critter/Critter_SeePlayer.wav", 1, CRITTER_ATTN_IDLE );
}

//=========================================================
// TakeDamage - the critter swerves when its enemy hurts it
// from far enough away, and always gibs
//=========================================================
int CCritter::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	float flDist;
	Vector vecApex;

	// if the critter is running, has an enemy, was hurt by the enemy, hasn't been hurt in the last 3 seconds, and isn't too close to the enemy,
	// it will swerve. (whew).
	if ( m_hEnemy != NULL && IsMoving() && pevAttacker == m_hEnemy->pev && gpGlobals->time - m_flLastHurtTime > 3 )
	{
		flDist = ( pev->origin - m_hEnemy->pev->origin ).Length2D();
		
		if ( flDist > CRITTER_SPRINT_DIST )
		{
			flDist = ( pev->origin - m_Route[ m_iRouteIndex ].vecLocation ).Length2D();// reusing flDist. 

			if ( FTriangulate( pev->origin, m_Route[ m_iRouteIndex ].vecLocation, flDist * 0.5, m_hEnemy, &vecApex ) )
			{
				InsertWaypoint( vecApex, bits_MF_TO_DETOUR | bits_MF_DONT_SIMPLIFY );
			}
		}
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_ALWAYSGIB );
}

//=========================================================
// GibMonster - leaves its flopping remains behind
//=========================================================
void CCritter::GibMonster( void )
{
	CCritterGib *pGib = GetClassPtr( (CCritterGib *)NULL );
	pGib->Spawn( );
	pGib->pev->origin = pev->origin;
	pGib->pev->angles.x = pev->angles.x;
	pGib->pev->angles.y = pev->angles.y + 90;
	pGib->pev->angles.z = pev->angles.z;
	pGib->Use( NULL, NULL, USE_TOGGLE, 1 );

	EMIT_SOUND( ENT(pev), CHAN_BODY, "critter/critter_gib_flop.wav", 1, ATTN_IDLE );

	CBaseMonster::GibMonster( );
}

//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL CCritter::CheckRangeAttack1( float flDot, float flDist )
{
	if ( flDist > 64 && flDist <= 1024 && flDot >= 0.5 && gpGlobals->time >= m_flNextSpitTime )
	{
		if ( m_hEnemy != NULL )
		{
			if ( fabs( pev->origin.z - m_hEnemy->pev->origin.z ) > 256 )
			{
				// don't try to spit at someone up really high or down really low.
				return FALSE;
			}
		}

		m_flNextSpitTime = gpGlobals->time + 0.5;

		return TRUE;
	}

	return FALSE;
}

//=========================================================
// CheckMeleeAttack1 - the bite
//=========================================================
BOOL CCritter::CheckMeleeAttack1( float flDot, float flDist )
{
	if ( flDist <= 85 && flDot >= 0.7 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckMeleeAttack2 - the bullsquid's, which the critter
// has no animation for
//=========================================================
BOOL CCritter::CheckMeleeAttack2( float flDot, float flDist )
{
	if ( flDist <= 85 && flDot >= 0.7 && !HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )		// The player & bullsquid can be as much as their bboxes 
	{										// apart (48 * sqrt(3)) and he can still attack (85 is a little more than 48*sqrt(3))
		return TRUE;
	}
	return FALSE;
}  

//=========================================================
//  FValidateHintType 
//=========================================================
BOOL CCritter::FValidateHintType( short sHint )
{
	int i;

	static short sCritterHints[] =
	{
		HINT_WORLD_HUMAN_BLOOD,
	};

	for ( i = 0 ; i < ARRAYSIZE ( sCritterHints ) ; i++ )
	{
		if ( sCritterHints[ i ] == sHint )
		{
			return TRUE;
		}
	}

	ALERT ( at_aiconsole, "Couldn't validate hint type" );
	return FALSE;
}

//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. In the base class implementation,
// monsters care about all sounds, but no scents.
//=========================================================
int CCritter::ISoundMask( void )
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_CARCASS	|
			bits_SOUND_MEAT		|
			bits_SOUND_GARBAGE	|
			bits_SOUND_PLAYER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CCritter::SetYawSpeed( void )
{
	int ys;

	ys = 0;

	switch ( m_Activity )
	{
	case	ACT_WALK:			ys = 120;	break;
	case	ACT_RUN:			ys = 120;	break;
	case	ACT_IDLE:			ys = 120;	break;
	case	ACT_RANGE_ATTACK1:	ys = 120;	break;
	default:
		ys = 120;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CCritter::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case CRITTER_AE_SPIT:
		{
			if ( m_hEnemy == NULL )
			{
				ALERT( at_aiconsole, "Critter spit without an enemy\n" );
				return;
			}

			Vector vecToEnemy;
			vecToEnemy = m_vecEnemyLKP - m_hEnemy->pev->view_ofs;
			Vector vecDir = vecToEnemy.Normalize( );
			Vector vecSrc;
			vecSrc = pev->origin + Vector( 0, 0, 32 );

			TraceResult tr;
			UTIL_TraceHull( vecSrc, vecSrc, dont_ignore_monsters, head_hull, ENT( pev ), &tr );

			if ( tr.flFraction == 1.0 && tr.fStartSolid != 1.0 )
			{
				CBaseEntity *pMaggot = CBaseEntity::Create( "monster_maggot", vecSrc, pev->angles, edict( ) );

				Vector vecForward;
				UTIL_MakeVectorsPrivate( pev->angles, vecForward, NULL, NULL );

				pMaggot->pev->angles = pev->angles;

				CBaseMonster *pMonster = pMaggot->MyMonsterPointer( );
				pMonster->pev->owner = edict( );
				( (CMaggot *)pMonster )->Launch( );
				pMonster->pev->avelocity.y = 600;
				UTIL_SetOrigin( pMonster->pev, vecSrc );

				if ( pMonster )	// sic: checked after it's used
					pMonster->m_hEnemy = m_hEnemy;

				Vector vecToss = g_vecZero;

				if ( m_hEnemy != NULL )
					vecToss = VecCheckThrow( pev, vecSrc, m_hEnemy->pev->origin + Vector( 0, 0, 16 ), 800, 1.0 );

				if ( vecToss == g_vecZero )
				{
					// no throw reaches the enemy; spit it straight ahead
					float flGravity = CVAR_GET_FLOAT( "sv_gravity" );

					pMaggot->pev->velocity = gpGlobals->v_forward * 800;
					pMaggot->pev->velocity.z += ( 0.625 * flGravity ) * 0.2;
				}

				// sic: this undoes the straight spit above, which leaves the maggot still
				pMonster->pev->velocity = vecToss;

				Vector vecSpitDir = pMaggot->pev->velocity.Normalize( );

				// spew the spittle temporary ents.
				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
					WRITE_BYTE( TE_SPRITE_SPRAY );
					WRITE_COORD( vecSrc.x );	// pos
					WRITE_COORD( vecSrc.y );	
					WRITE_COORD( vecSrc.z );	
					WRITE_COORD( vecSpitDir.x );	// dir
					WRITE_COORD( vecSpitDir.y );	
					WRITE_COORD( vecSpitDir.z );	
					WRITE_SHORT( iCritterSpitSprite );	// model
					WRITE_BYTE ( 8 );			// count
					WRITE_BYTE ( 120 );			// speed
					WRITE_BYTE ( 15 );			// noise ( client will divide by 100 )
				MESSAGE_END();
			}
		}
		break;

		case CRITTER_AE_BITE:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, 15, DMG_SLASH );
			
			if ( pHurt )
			{
				Vector vecForward;

				UTIL_MakeVectorsPrivate( pev->angles, vecForward, NULL, NULL );

				pHurt->pev->punchangle.z = RANDOM_FLOAT( -10, 10 );
				pHurt->pev->punchangle.x = RANDOM_FLOAT( -10, 10 );
				pHurt->pev->velocity = pHurt->pev->velocity + vecForward * 100;
				pHurt->pev->velocity = pHurt->pev->velocity + Vector( 0, 0, 100 );

				CBaseMonster *pMonster = pHurt->MyMonsterPointer( );

				if ( pMonster )
				{
					TraceResult tr;

					UTIL_TraceLine( BodyTarget( g_vecZero ), pMonster->BodyTarget( g_vecZero ), dont_ignore_monsters, ENT( pev ), &tr );
					pMonster->TraceBleed( 15, ( pMonster->BodyTarget( g_vecZero ) - BodyTarget( g_vecZero ) ).Normalize( ), &tr, DMG_SLASH, tr.vecEndPos );

					if ( pMonster->IsPlayer( ) )
						AttackHitSound( pMonster );
				}
			}
		}
		break;

		default:
			CBaseMonster::HandleAnimEvent( pEvent );
	}
}

//=========================================================
// Spawn
//=========================================================
void CCritter::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT(pev), "models/critter.mdl" );
	UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 64 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->health			= gSkillData.critterHealth;
	m_flNextSpitTime	= gpGlobals->time;

	MonsterInit( );

	pev->view_ofs = Vector( 0, 0, 30 );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CCritter::Precache( void )
{
	PRECACHE_MODEL( "models/critter.mdl" );

	UTIL_PrecacheOther( "critter_gib" );
	UTIL_PrecacheOther( "monster_maggot" );

	iCritterSpitSprite = PRECACHE_MODEL( "sprites/tinyspit.spr" );// client side spittle.

	g_iGibXeno = PRECACHE_MODEL( "sprites/gibxeno.spr" );
	g_iGoreXeno = PRECACHE_MODEL( "sprites/gorexeno.spr" );

	CBaseMonster::Precache( );
}

//========================================================
// RunAI - overridden for critter because there are things
// that need to be checked every think.
//========================================================
void CCritter::RunAI( void )
{
	// first, do base class stuff
	CBaseMonster::RunAI( );

	if ( m_hEnemy == NULL )
		return;

	if ( m_Activity == ACT_RUN )
	{
		// chasing enemy. Sprint for last bit
		if ( ( pev->origin - m_hEnemy->pev->origin ).Length2D( ) < CRITTER_SPRINT_DIST )
			pev->framerate = 1.75;
		else
			pev->framerate = 1.5;
	}

	if ( m_hEnemy != NULL && m_flLastSwerveTime < gpGlobals->time )
	{
		// swerve when the enemy aims at the critter
		Vector vecToLKP( m_vecEnemyLKP - pev->origin );

		UTIL_MakeAimVectors( m_hEnemy->pev->angles );

		Vector vecAim = gpGlobals->v_forward;
		Vector vecDir;
		vecDir = vecToLKP.Normalize( );
		vecAim = vecAim.Normalize( );
		vecAim = vecAim * -1;

		Vector vecApex;

		float flDot = DotProduct( vecAim, vecDir );
		if ( flDot > 0.95 )
		{
			float flDist;

			m_flLastSwerveTime = gpGlobals->time + 0.5;

			// sic: copied from TakeDamage, where it compared the attacker
			if ( m_hEnemy != NULL && IsMoving() && m_hEnemy->pev == m_hEnemy->pev )
			{
				flDist = ( pev->origin - m_hEnemy->pev->origin ).Length2D();
				m_flLastSwerveTime = gpGlobals->time + 2;

				if ( flDist > 128 )
				{
					flDist = ( pev->origin - m_Route[ m_iRouteIndex ].vecLocation ).Length2D();

					if ( FTriangulate( pev->origin, m_Route[ m_iRouteIndex ].vecLocation, flDist * 0.5, m_hEnemy, &vecApex ) )
					{
						InsertWaypoint( vecApex, bits_MF_TO_DETOUR | bits_MF_DONT_SIMPLIFY );
					}
				}
			}
		}
	}
}

//=========================================================
// GetSchedule 
//=========================================================
Schedule_t *CCritter::GetSchedule( void )
{
	switch	( m_MonsterState )
	{
	case MONSTERSTATE_ALERT:
		{
			if ( HasConditions(bits_COND_SMELL_FOOD) )
			{
				CSound		*pSound;

				pSound = PBestScent();
				
				if ( pSound && (!FInViewCone ( &pSound->m_vecOrigin ) || !FVisible ( pSound->m_vecOrigin )) )
				{
					// scent is behind or occluded
					return GetScheduleOfType( SCHED_CRITTER_SNIFF_AND_EAT );
				}

				// food is right out in the open. Just go get it.
				return GetScheduleOfType( SCHED_CRITTER_EAT );
			}

			if ( HasConditions(bits_COND_SMELL) )
			{
				// there's something stinky. 
				CSound		*pSound;

				pSound = PBestScent();
				if ( pSound )
					return GetScheduleOfType( SCHED_CRITTER_WALLOW);
			}

			break;
		}
	case MONSTERSTATE_COMBAT:
		{
// dead enemy
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			{
				// call base class, all code to handle dead enemies is centralized there.
				return CBaseMonster :: GetSchedule();
			}

			if ( HasConditions(bits_COND_NEW_ENEMY) )
			{
				return GetScheduleOfType ( SCHED_RANGE_ATTACK1 );
			}

			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				return GetScheduleOfType ( SCHED_RANGE_ATTACK1 );
			}

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			{
				return GetScheduleOfType ( SCHED_MELEE_ATTACK1 );
			}

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
			{
				return GetScheduleOfType ( SCHED_MELEE_ATTACK2 );
			}
			
			return GetScheduleOfType ( SCHED_CHASE_ENEMY );

			break;
		}
	}

	return CBaseMonster :: GetSchedule();
}

//=========================================================
// GetScheduleOfType
//=========================================================
Schedule_t* CCritter::GetScheduleOfType( int Type ) 
{
	switch	( Type )
	{
	case SCHED_RANGE_ATTACK1:
		return &slCritterRangeAttack1[ 0 ];
		break;
	case SCHED_CRITTER_EAT:
		return &slCritterEat[ 0 ];
		break;
	case SCHED_CRITTER_SNIFF_AND_EAT:
		return &slCritterSniffAndEat[ 0 ];
		break;
	case SCHED_CRITTER_WALLOW:
		return &slCritterWallow[ 0 ];
		break;
	case SCHED_CHASE_ENEMY:
		return &slCritterChaseEnemy[ 0 ];
		break;
	}

	return CBaseMonster :: GetScheduleOfType ( Type );
}
