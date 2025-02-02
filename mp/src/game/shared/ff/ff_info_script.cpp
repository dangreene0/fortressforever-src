// =============== Fortress Forever ==============
// ======== A modification for Half-Life 2 =======
//
// @file ff_info_script.cpp
// @author Patrick O'Leary (Mulchman)
// @date 07/13/2006
// @brief info_ff_script, the C++ class
//
// REVISIONS
// ---------
// 07/13/2006, Mulchman: 
//		This file First created
//		Rewriting "ff_item_flag"
//		CFF_InfoScript name will change once complete
//		BREAKINBENNY: We could attach the flags to the carrier's "ffAtt_flag" $attachment on their model, if one exists.

#include "cbase.h"
#include "ff_info_script.h"
#include "IEffects.h"
#include "movevars_shared.h"

#ifdef CLIENT_DLL
	#include "c_ff_player.h"
	#include "iinput.h"
#elif GAME_DLL
	#include "ff_scriptman.h"
	#include "ff_luacontext.h"
	#include "ff_player.h"
	#include "omnibot_interface.h"
	#include "ai_basenpc.h"

	// Lua includes
	extern "C"
	{
		#include "lua.h"
		#include "lualib.h"
		#include "lauxlib.h"
	}

	#undef MINMAX_H
	#undef min
	#undef max

	#include "LuaBridge/LuaBridge.h"
#endif // CLIENT_DLL

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//=============================================================================
//
// Class CFFInfoScript
//
//=============================================================================
IMPLEMENT_NETWORKCLASS_ALIASED( FFInfoScript, DT_FFInfoScript ) 

BEGIN_NETWORK_TABLE( CFFInfoScript, DT_FFInfoScript )
#ifdef GAME_DLL
	SendPropFloat( SENDINFO( m_flThrowTime ) ),
	SendPropVector( SENDINFO( m_vecOffset ), SPROP_NOSCALE ), // AfterShock: possibly SPROP_COORD this and remove SPROP_NOSCALE for optimisation?
	SendPropInt( SENDINFO( m_iGoalState ), 4 ),
	SendPropInt( SENDINFO( m_iPosState ), 4 ),
	SendPropInt( SENDINFO( m_iShadow ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iHasModel ), 1, SPROP_UNSIGNED ),
#elif CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flThrowTime ) ),
	RecvPropVector( RECVINFO( m_vecOffset ) ),
	RecvPropInt( RECVINFO( m_iGoalState ) ),
	RecvPropInt( RECVINFO( m_iPosState ) ),
	RecvPropInt( RECVINFO( m_iShadow ) ),
	RecvPropInt( RECVINFO( m_iHasModel ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CFFInfoScript )
	DEFINE_PRED_FIELD( m_flThrowTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vecOffset, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iGoalState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iPosState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iShadow, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iHasModel, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

BEGIN_DATADESC(CFFInfoScript)

#ifdef GAME_DLL
	DEFINE_ENTITYFUNC( OnTouch ),
	DEFINE_THINKFUNC( OnThink ),
	DEFINE_THINKFUNC( OnRespawn ),
	DEFINE_THINKFUNC( RemoveThink )
#endif // GAME_DLL

END_DATADESC()

LINK_ENTITY_TO_CLASS( info_ff_script, CFFInfoScript );
PRECACHE_REGISTER( info_ff_script );

#ifdef GAME_DLL

ConVar ffdev_visualize_infoscript_sizes("ffdev_visualize_infoscript_sizes", "0", FCVAR_CHEAT);
#define VISUALIZE_INFOSCRIPT_SIZES ffdev_visualize_infoscript_sizes.GetBool()

#define ITEM_PICKUP_BOX_BLOAT 12 // default; only used if no lua function exists
//ConVar ffdev_flag_throwup( "ffdev_flag_throwup", "1.6", FCVAR_FF_FFDEV );
#define FLAG_THROWUP 1.6f
//ConVar ffdev_flag_float_force( "ffdev_flag_float_force", "1.05", FCVAR_FF_FFDEV );
#define FLAG_FLOAT_FORCE 1.05f
//ConVar ffdev_flag_float_force2( "ffdev_flag_float_force2", "0.9", FCVAR_FF_FFDEV );
#define FLAG_FLOAT_FORCE2 0.9f
//ConVar ffdev_flag_float_drag( "ffdev_flag_float_drag", "1.0", FCVAR_FF_FFDEV );
#define FLAG_FLOAT_DRAG 1.0f
//ConVar ffdev_flag_float_offset( "ffdev_flag_float_offset", "48.0", FCVAR_FF_FFDEV );
#define FLAG_FLOAT_OFFSET 48.0f
//ConVar ffdev_flag_rotation( "ffdev_flag_rotation", "50.0", FCVAR_FF_FFDEV );
#define FLAG_ROTATION 50.0f

int ACT_INFO_RETURNED;
int ACT_INFO_CARRIED;
int ACT_INFO_DROPPED;

LINK_ENTITY_TO_CLASS( info_ff_script_animator, CFFInfoScriptAnimator );
PRECACHE_REGISTER( info_ff_script_animator );

BEGIN_DATADESC( CFFInfoScriptAnimator )
	DEFINE_THINKFUNC( OnThink ),
END_DATADESC()

#endif // GAME_DLL

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFFInfoScript::CFFInfoScript( void )
{
#ifdef CLIENT_DLL
	m_iShadow = 0;
	m_iHasModel = 0;
#elif GAME_DLL
	m_iHasModel = 0;
	m_pAnimator = NULL;
	m_pLastOwner = NULL;
	m_spawnflags = 0;
	m_vStartOrigin = Vector(0, 0, 0);
	m_bHasAnims = false;
	m_bUsePhysics = false;
	m_iShadow = 1;

	// Init the goal state
	m_iGoalState = GS_INACTIVE;
	// Init the position state
	m_iPosState = PS_RETURNED;

	m_allowTouchFlags = 0;
	m_disallowTouchFlags = 0;

	// bot info
	m_BotTeamFlags = 0;
	m_BotGoalType = Omnibot::kNone;
#endif // CLIENT_DLL
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFFInfoScript::~CFFInfoScript( void )
{
#ifdef GAME_DLL
	m_spawnflags = 0;
	m_vStartOrigin = Vector( 0, 0, 0 );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Precache
//-----------------------------------------------------------------------------
void CFFInfoScript::Precache( void )
{
	PrecacheModel( FLAG_MODEL );

#ifdef GAME_DLL
	CFFLuaSC hPrecache;
	_scriptman.RunPredicates_LUA( this, &hPrecache, "precache" );
#endif

	PrecacheScriptSound( "Flag.FloatDeploy" );
	PrecacheScriptSound( "Flag.FloatBubbles" );
}

//-----------------------------------------------------------------------------
// Purpose: Spawn
//-----------------------------------------------------------------------------
void CFFInfoScript::Spawn( void )
{
#ifdef GAME_DLL
	// Set the time we spawned.
	m_flSpawnTime = gpGlobals->curtime;
#endif // GAME_DLL

	Precache();

#ifdef GAME_DLL
	// Check if this object has an attachoffset function and get the value if it does
	CFFLuaSC hAttachOffset;
	if ( _scriptman.RunPredicates_LUA( this, &hAttachOffset, "attachoffset" ) )
		m_vecOffset.GetForModify() = hAttachOffset.GetVector();

	// See if object has a shadow
	CFFLuaSC hShadow;
	if ( _scriptman.RunPredicates_LUA( this, &hShadow, "hasshadow" ) )
	{
		if ( !hShadow.GetBool() )
			m_iShadow = 0;
	}
#endif // GAME_DLL

	SetModel( FLAG_MODEL );

	SetBlocksLOS( false );

#ifdef GAME_DLL
	// Run the spawn function
	CFFLuaSC hSpawn;
	bool bSpawnSuccessful = _scriptman.RunPredicates_LUA( this, &hSpawn, "spawn" );

	m_vStartOrigin = GetAbsOrigin();
	m_vStartAngles = GetAbsAngles();
	m_atStart = true;

	// See if the object uses animations
	CFFLuaSC hHasAnimation;
	_scriptman.RunPredicates_LUA( this, &hHasAnimation, "hasanimation" );
	m_bHasAnims = hHasAnimation.GetBool();

	// If using anims, set them up!
	// Only do this if the "spawn" call was successful so it won't break maps
	if (m_bHasAnims && bSpawnSuccessful)
	{
		ADD_CUSTOM_ACTIVITY( CFFInfoScript, ACT_INFO_RETURNED );
		ADD_CUSTOM_ACTIVITY( CFFInfoScript, ACT_INFO_DROPPED );
		ADD_CUSTOM_ACTIVITY( CFFInfoScript, ACT_INFO_CARRIED );

		// Set up the animator helper
		m_pAnimator = ( CFFInfoScriptAnimator* ) CreateEntityByName( "info_ff_script_animator" );
		if ( m_pAnimator )
		{
			m_pAnimator->Spawn();
			m_pAnimator->m_pFFScript = this;
		}
	}

	// Check to see if this object uses physics
	CFFLuaSC hUsePhysics;
	_scriptman.RunPredicates_LUA( this, &hUsePhysics, "usephysics" );
	m_bUsePhysics = hUsePhysics.GetBool();

	// Store off mins/maxs
	m_vecMins = CollisionProp()->OBBMins();
	m_vecMaxs = CollisionProp()->OBBMaxs();

	CreateItemVPhysicsObject();

	m_pLastOwner = NULL;
#endif // GAME_DLL

	m_flThrowTime = 0.0f;

#ifdef GAME_DLL
	PlayReturnedAnim();
#endif
}

void CFFInfoScript::UpdateOnRemove( void )
{
#ifdef GAME_DLL
	if (m_pAnimator)
		m_pAnimator->Remove();
#endif

	BaseClass::UpdateOnRemove();
}

#ifdef CLIENT_DLL

void CFFInfoScript::OnDataChanged( DataUpdateType_t updateType )
{
	// NOTE: We MUST call the base classes' implementation of this function
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
		SetNextClientThink(CLIENT_THINK_ALWAYS);

	UpdateVisibility();
}

int CFFInfoScript::DrawModel( int flags )
{
	if ( !m_iHasModel )
		return 0;

	if ( !ShouldDraw() )
		return 0;

	// Bug #0000590: You don't see flags you've thrown, or have, while in thirdperson
	// Always draw if thirdperson
	if ( input->CAM_IsThirdPerson() )
		return BaseClass::DrawModel( flags );

	// Bug #0001660: Flag visible in first-person spectator mode
	// Don't draw the FFInfoScript if we're in-eye spectating the same player the flag is hitched to |---> Defrag
	CBasePlayer* pLocalPlayer = C_FFPlayer::GetLocalFFPlayerOrObserverTarget();

	if ( pLocalPlayer && pLocalPlayer == GetFollowedEntity() )
		return 0;

	// Temporary fix until we sort out the interpolation business
	if ( m_flThrowTime + 0.2f > gpGlobals->curtime )
		return 0;

	return BaseClass::DrawModel( flags );
}

// Bug #0000508: Carried objects cast a shadow for the carrying player
ShadowType_t CFFInfoScript::ShadowCastType( void )
{
	if ( !m_iHasModel )
		return SHADOWS_NONE;

	if ( !m_iShadow )
		return SHADOWS_NONE;

	if ( !ShouldDraw() )
		return SHADOWS_NONE;

	if ( GetFollowedEntity() == C_FFPlayer::GetLocalFFPlayerOrObserverTarget() )
	{
		if ( r_selfshadows.GetInt() )
			return SHADOWS_RENDER_TO_TEXTURE;

		return SHADOWS_NONE;
	}
	else
		return SHADOWS_RENDER_TO_TEXTURE;
}

void CFFInfoScript::ClientThink( void )
{
	// Adjust offset relative to player
	C_FFPlayer* pPlayer = ToFFPlayer( GetFollowedEntity() );
	if ( pPlayer && !m_vecOffset.IsZero() )
	{
		// Adjust origin based on player's origin

		Vector vecOrigin, vecForward, vecRight, vecUp;
		vecOrigin = pPlayer->GetFeetOrigin();

		pPlayer->EyeVectors( &vecForward, &vecRight );

		// Level off
		vecForward.z = 0;
		VectorNormalize( vecForward );

		// Get straight up vector
		vecUp = Vector( 0, 0, 1.0f );

		// Get right vector (think this is correct, otherwise swap the shits)
		vecRight = CrossProduct( vecUp, vecForward );

		VectorNormalize( vecRight );

		bool bBalls;
		bBalls = false;

		Vector vecBallOrigin = vecOrigin + ( vecForward * m_vecOffset.x ) + ( vecRight * m_vecOffset.y ) + ( vecUp * m_vecOffset.z );

		SetAbsOrigin( vecBallOrigin );
		SetAbsAngles( QAngle( 0, GetFollowedEntity()->GetAbsAngles().y, 0 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFFInfoScript::ShouldDraw( void )
{
	return !IsRemoved();
}

#elif GAME_DLL

void CFFInfoScript::MakeTouchable()
{
	// SOLID_NONE is used instead of SOLID_BBOX so that dropped items do not block doors, etc
	SetSolid( SOLID_NONE );
	// FSOLID_USE_TRIGGER_BOUNDS is required to allow SOLID_NONE non-vphysics objects to be touched
	// FSOLID_NOT_STANDABLE makes vphysics objects non-solid to players
	AddSolidFlags( FSOLID_USE_TRIGGER_BOUNDS | FSOLID_NOT_STANDABLE | FSOLID_TRIGGER );
	SetCollisionGroup( COLLISION_GROUP_TRIGGERONLY );

	m_vecTouchMins = m_vecMins;
	m_vecTouchMaxs = m_vecMaxs;
	m_flHitboxBloat = 0.0f;
	
	CFFLuaSC TouchSizeContext;
	TouchSizeContext.PushRef(m_vecTouchMins);
	TouchSizeContext.PushRef(m_vecTouchMaxs);
	TouchSizeContext.CallFunction( this, "gettouchsize" );

	CFFLuaSC BloatSizeContext;
	if (BloatSizeContext.CallFunction( this, "getbloatsize" ))
		m_flHitboxBloat = BloatSizeContext.GetFloat();
	else
		m_flHitboxBloat = ITEM_PICKUP_BOX_BLOAT;

	// Reset size
	UTIL_SetSize( this, m_vecTouchMins, m_vecTouchMaxs );

	if (m_flHitboxBloat > 0.0f)
		CollisionProp()->UseTriggerBounds( true, m_flHitboxBloat );
	
	// make it respond to touch again
	SetTouch( &CFFInfoScript::OnTouch );		// |-- Mirv: Account for GCC strictness
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFFInfoScript::CreateItemVPhysicsObject( void )
{
	if( GetOwnerEntity() )
	{
		FollowEntity( NULL );
		SetOwnerEntity( NULL );
	}

	// move it to where it's supposed to respawn at
	m_atStart = true;
	SetAbsOrigin( m_vStartOrigin );
	SetLocalAngles( m_vStartAngles );

	MakeTouchable();

	SetMoveType( MOVETYPE_NONE );

	// Update goal state
	SetInactive();
	// Update position state
	SetReturned();

	CFFLuaSC hDropAtSpawn;
	_scriptman.RunPredicates_LUA( this, &hDropAtSpawn, "dropatspawn" );

	// See if the info_ff_script should drop to the ground or not
	if( hDropAtSpawn.GetBool() )
	{
		// If it's not physical, drop it to the floor
		if( UTIL_DropToFloor( this, MASK_SOLID ) == 0 )
		{
			Warning( "[InfoFFScript] Item %s fell out of level at %f,%f,%f\n", GetClassname(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);
			UTIL_Remove( this );
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Play an animation specific to this info_ff_script being dropped
//-----------------------------------------------------------------------------
void CFFInfoScript::PlayDroppedAnim( void )
{
	InternalPlayAnim( ( Activity )ACT_INFO_DROPPED );
}

//-----------------------------------------------------------------------------
// Purpose: Play an animation specific to this info_ff_script being carried
//-----------------------------------------------------------------------------
void CFFInfoScript::PlayCarriedAnim( void )
{
	InternalPlayAnim( ( Activity )ACT_INFO_CARRIED );
}

//-----------------------------------------------------------------------------
// Purpose: Play an animation specific to this info_ff_script being returned
//-----------------------------------------------------------------------------
void CFFInfoScript::PlayReturnedAnim( void )
{
	InternalPlayAnim( ( Activity )ACT_INFO_RETURNED );
}

//-----------------------------------------------------------------------------
// Purpose: Play an animation
//-----------------------------------------------------------------------------
void CFFInfoScript::InternalPlayAnim( Activity hActivity )
{
	if( m_bHasAnims )
	{
		ResetSequenceInfo();
		SetSequence( SelectWeightedSequence( hActivity ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFFInfoScript::CanEntityTouch(CBaseEntity* pEntity)
{
	if(!pEntity)
		return false;

	// early out if we dont pass the player filter
	if(m_allowTouchFlags & kAllowOnlyPlayers)
	{
		if(!pEntity->IsPlayer())
			return false;
	}
	if(m_disallowTouchFlags & kAllowOnlyPlayers)
	{
		if(pEntity->IsPlayer())
			return false;
	}

	bool bCanTouch = false;

	// first check the team touch flags
	int teamMask = kAllowRedTeam|kAllowBlueTeam|kAllowYellowTeam|kAllowGreenTeam;
	if(teamMask & m_allowTouchFlags || teamMask & m_disallowTouchFlags)
	{
		int iTeam = pEntity->GetTeamNumber();
		switch(iTeam)
		{
		case TEAM_BLUE:
			bCanTouch = (m_allowTouchFlags & kAllowBlueTeam) == kAllowBlueTeam
				&& (m_disallowTouchFlags & kAllowBlueTeam) != kAllowBlueTeam;
			break;

		case TEAM_RED:
			bCanTouch = (m_allowTouchFlags & kAllowRedTeam) == kAllowRedTeam
				&& (m_disallowTouchFlags & kAllowRedTeam) != kAllowRedTeam;
			break;

		case TEAM_GREEN:
			bCanTouch = (m_allowTouchFlags & kAllowGreenTeam) == kAllowGreenTeam
				&& (m_disallowTouchFlags & kAllowGreenTeam) != kAllowGreenTeam;
			break;

		case TEAM_YELLOW:
			bCanTouch = (m_allowTouchFlags & kAllowYellowTeam) == kAllowYellowTeam
				&& (m_disallowTouchFlags & kAllowYellowTeam) != kAllowYellowTeam;
			break;
		}
	}

	// now check the class touch flags
	int classMask = kAllowScout|kAllowSniper|kAllowSoldier|kAllowDemoman|kAllowMedic|kAllowHwguy|kAllowPyro|kAllowSpy|kAllowEngineer|kAllowCivilian;
	if(classMask & m_disallowTouchFlags && pEntity->IsPlayer() )
	{
		CFFPlayer *pPlayer = ToFFPlayer ( pEntity );
		int iClass = pPlayer->GetClassSlot();
		switch(iClass)
		{
		case CLASS_SCOUT:
			bCanTouch = (m_disallowTouchFlags & kAllowScout) != kAllowScout;
			break;

		case CLASS_SNIPER:
			bCanTouch = (m_disallowTouchFlags & kAllowSniper) != kAllowSniper;
			break;

		case CLASS_SOLDIER:
			bCanTouch = (m_disallowTouchFlags & kAllowSoldier) != kAllowSoldier;
			break;

		case CLASS_DEMOMAN:
			bCanTouch = (m_disallowTouchFlags & kAllowDemoman) != kAllowDemoman;
			break;

		case CLASS_MEDIC:
			bCanTouch = (m_disallowTouchFlags & kAllowMedic) != kAllowMedic;
			break;

		case CLASS_HWGUY:
			bCanTouch = (m_disallowTouchFlags & kAllowHwguy) != kAllowHwguy;
			break;

		case CLASS_PYRO:
			bCanTouch = (m_disallowTouchFlags & kAllowPyro) != kAllowPyro;
			break;

		case CLASS_SPY:
			bCanTouch = (m_disallowTouchFlags & kAllowSpy) != kAllowSpy;
			break;

		case CLASS_ENGINEER:
			bCanTouch = (m_disallowTouchFlags & kAllowEngineer) != kAllowEngineer;
			break;

		case CLASS_CIVILIAN:
			bCanTouch = (m_disallowTouchFlags & kAllowCivilian) != kAllowCivilian;
			break;
		}
	}

	return bCanTouch;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFFInfoScript::OnTouch( CBaseEntity *pEntity )
{
	if(!pEntity)
		return;

	// if allowed, notify script entity was touched
	if(CanEntityTouch(pEntity))
	{
		CFFLuaSC hTouch( 1, pEntity );
		_scriptman.RunPredicates_LUA( this, &hTouch, "touch" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the allow touch flags.
// All teams are disallowed by default, so they are allowed like this.
// All classes are allowed by default, so they aren't allowed like this.
//-----------------------------------------------------------------------------
void CFFInfoScript::SetTouchFlags(const luabridge::LuaRef& table)
{
	m_allowTouchFlags = 0;

	if(table.isValid() && table.isTable())
	{
		// Iterate through the table
		for(luabridge::Iterator ib(table); !ib.isNil(); ++ib)
		{
			luabridge::LuaRef val = table[ib.key()];

			if(val.isNumber())
			{
				try
				{
					int flag = static_cast<int>(val);
					switch(flag)
					{
					case kAllowOnlyPlayers:
						m_allowTouchFlags |= kAllowOnlyPlayers;
						break;

					case kAllowBlueTeam:
						m_allowTouchFlags |= kAllowBlueTeam;
						break;

					case kAllowRedTeam:
						m_allowTouchFlags |= kAllowRedTeam;
						break;

					case kAllowYellowTeam:
						m_allowTouchFlags |= kAllowYellowTeam;
						break;

					case kAllowGreenTeam:
						m_allowTouchFlags |= kAllowGreenTeam;
						break;
					}
				}
				catch(...)
				{
					// throw out exception
					// an invalid cast is not exceptional!!
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the disallow touch flags, so scripts can be simpler, e.g.
// ff_waterpolo.lua disallows goalies from using ALL packs with 1 line.
//-----------------------------------------------------------------------------
void CFFInfoScript::SetDisallowTouchFlags(const luabridge::LuaRef& table)
{
	m_disallowTouchFlags = 0;

	if(table.isValid() && table.isTable())
	{
		// Iterate through the table
		for(luabridge::Iterator ib(table); !ib.isNil(); ++ib)
		{
			luabridge::LuaRef val = table[ib.key()];

			if(val.isNumber())
			{
				try
				{
					int flag = static_cast<int>(val);
					switch(flag)
					{
					case kAllowOnlyPlayers:
						m_disallowTouchFlags |= kAllowOnlyPlayers;
						break;

					case kAllowBlueTeam:
						m_disallowTouchFlags |= kAllowBlueTeam;
						break;

					case kAllowRedTeam:
						m_disallowTouchFlags |= kAllowRedTeam;
						break;

					case kAllowYellowTeam:
						m_disallowTouchFlags |= kAllowYellowTeam;
						break;

					case kAllowGreenTeam:
						m_disallowTouchFlags |= kAllowGreenTeam;
						break;

					case kAllowScout:
						m_disallowTouchFlags |= kAllowScout;
						break;

					case kAllowSniper:
						m_disallowTouchFlags |= kAllowSniper;
						break;

					case kAllowSoldier:
						m_disallowTouchFlags |= kAllowSoldier;
						break;

					case kAllowDemoman:
						m_disallowTouchFlags |= kAllowDemoman;
						break;

					case kAllowMedic:
						m_disallowTouchFlags |= kAllowMedic;
						break;

					case kAllowHwguy:
						m_disallowTouchFlags |= kAllowHwguy;
						break;

					case kAllowPyro:
						m_disallowTouchFlags |= kAllowPyro;
						break;

					case kAllowSpy:
						m_disallowTouchFlags |= kAllowSpy;
						break;

					case kAllowEngineer:
						m_disallowTouchFlags |= kAllowEngineer;
						break;

					case kAllowCivilian:
						m_disallowTouchFlags |= kAllowCivilian;
						break;

					}
				}
				catch(...)
				{
					// throw out exception
					// an invalid cast is not exceptional!!
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFFInfoScript::OnOwnerDied( CBaseEntity *pEntity )
{
	if( GetOwnerEntity() == pEntity )
	{
		// Update position state
		SetDropped();
		CFFLuaSC hOwnerDie( 1, pEntity );
		_scriptman.RunPredicates_LUA( this, &hOwnerDie, "onownerdie" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Added so that players getting forced respawn by ApplyToAll/Team/Player
//			can give their objects a chance to be dropped so we don't run into
//			a situation where you respawn with an info_ff_script and you're not
//			supposed to because forcing a respawn used to not ask lua what to
//			do with objects players were carrying.
//-----------------------------------------------------------------------------
void CFFInfoScript::OnOwnerForceRespawn( CBaseEntity *pEntity )
{
	if( GetOwnerEntity() == pEntity )
	{
		// Update position state
		SetDropped();
		CFFLuaSC hOnOwnerForceRespawn( 1, pEntity );
		_scriptman.RunPredicates_LUA( this, &hOnOwnerForceRespawn, "onownerforcerespawn" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFFInfoScript::Pickup( CBaseEntity *pEntity )
{
	// Don't do anything if removed
	if( IsRemoved() )
		return;

	SetOwnerEntity( pEntity );
	SetTouch( NULL );

	if( m_bUsePhysics )
	{
		if( VPhysicsGetObject() )
			VPhysicsDestroyObject();
	}

	FollowEntity( pEntity, true );

	// Goal is active when carried	
	SetActive();
	// Update position state
	SetCarried();

	// stop the return timer
	SetThink( NULL );
	SetNextThink( gpGlobals->curtime );

	PlayCarriedAnim();

	Omnibot::Notify_ItemPickedUp(this, pEntity);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFFInfoScript::Respawn(float delay)
{
	// Don't do anything if removed
	if( IsRemoved() )
		return;

	CreateItemVPhysicsObject();

	SetTouch( NULL );
	AddEffects( EF_NODRAW );

	// During this period until
	// we respawn we're active
	SetActive();

	SetThink( &CFFInfoScript::OnRespawn );
	SetNextThink( gpGlobals->curtime + delay );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFFInfoScript::OnRespawn( void )
{
	// Don't do anything if removed
	if( IsRemoved() )
		return;

	CreateItemVPhysicsObject();

	if( IsEffectActive( EF_NODRAW ) )
	{
		// changing from invisible state to visible.
		RemoveEffects( EF_NODRAW );
		DoMuzzleFlash();

		CFFLuaSC hMaterialize;
		_scriptman.RunPredicates_LUA( this, &hMaterialize, "materialize" );
	}

	m_pLastOwner = NULL;
	m_flThrowTime = 0.0f;

	PlayReturnedAnim();

	Omnibot::Notify_ItemRespawned(this);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFFInfoScript::Drop( float delay, Vector pos, Vector velocity )
{
	// Don't do anything if removed
	if( IsRemoved() )
		return;

	CFFPlayer *pOwner = ToFFPlayer( GetOwnerEntity() );
	AssertMsg( pOwner, "Objects can only be attached to players currently!" );
	if( !pOwner )
		return;

	// stop following
	FollowEntity( NULL );

	m_vecPhysicsMins = m_vecMins;
	m_vecPhysicsMaxs = m_vecMaxs;
	
	CFFLuaSC PhysicsSizeContext;
	PhysicsSizeContext.PushRef(m_vecPhysicsMins);
	PhysicsSizeContext.PushRef(m_vecPhysicsMaxs);
	PhysicsSizeContext.CallFunction( this, "getphysicssize" );

	MakeTouchable();

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	CollisionRulesChanged();

	// Update goal state
	SetActive();
	// Update position state
	SetDropped();
	
	if( m_bUsePhysics )
	{
		// No physics object exists, create it
		if( !VPhysicsGetObject() )
			VPhysicsInitNormal( SOLID_VPHYSICS, GetSolidFlags(), false );

		// No physics object can be created, use normal stuff
		if( !VPhysicsGetObject() )
			m_bUsePhysics = false;
	}

	if( m_bUsePhysics )
	{
		IPhysicsObject *pPhysics = VPhysicsGetObject();

		// If we're here, pPhysics won't be NULL
		Assert( pPhysics );

		pPhysics->EnableGravity( true );
		pPhysics->EnableMotion( true );
		pPhysics->EnableCollisions( true );
		pPhysics->EnableDrag( true );

		Vector vecDir;
		AngleVectors( pOwner->EyeAngles(), &vecDir );

		// NOTE: This is the right angle shit... the ball just
		// doesn't want to be thrown that way
		//NDebugOverlay::Line( pOwner->EyePosition(), pOwner->EyePosition() + ( 256.0f * vecDir ), 0, 0, 255, false, 5.0f );

		//vecDir *= vel * vel;

		/*pPhysics->ApplyForceCenter( vecDir );
		pPhysics->AddVelocity( &vecDir, &vecDir );*/
		pPhysics->SetVelocity( &velocity, &velocity );

		// Stop the sequence if playing
		// NOTE: This doesn't seem to work yet either...
		if( m_bHasAnims )
		{
			ResetSequenceInfo();
			SetCycle( 0 );
			SetSequence( 0 );
		}		
	}
	else
	{	
		// Resize - only do for non physics though!
		//CollisionProp()->SetCollisionBounds( Vector( 0, 0, 0 ), Vector( 0, 0, 4 ) );

		// #0001026: backpacks & flags can be discarded through doors
		// When the bounding box was getting reset after coming to a rest, it was getting stuck in various surfaces -> Defrag

		SetAbsOrigin( pos ); /* + ( vecForward * m_vecOffset.GetX() ) + ( vecRight * m_vecOffset.GetY() ) + ( vecUp * m_vecOffset.GetZ() ) );

								   trace_t trHull;
								   UTIL_TraceHull( vecOrigin, vecOrigin + Vector( 0, 0, 1 ), Vector( 0, 0, 0 ), Vector( 0, 0, 1 ), MASK_PLAYERSOLID, pOwner, COLLISION_GROUP_PLAYER, &trHull );

								   // If the trace started in a solid, or the trace didn't finish, or if
								   // it hit the world then we want to move it back to the player's origin.
								   // This is to stop things like the push ball that sticks out in front of
								   // the players being trapped in the world if a player is standing staring
								   // into a wall
								   if( trHull.allsolid || ( trHull.fraction != 1.0f ) || trHull.DidHitWorld() )
								   SetAbsOrigin( vecOrigin - Vector( 0, 0, 2 ) );*/

		QAngle vecAngles = pOwner->EyeAngles();
		SetAbsAngles( QAngle( 0, vecAngles.y, 0 ) );

		SetAbsVelocity( velocity );
	}

	// set to respawn in [delay] seconds
	if( delay >= 0 )
	{
		SetThink( &CFFInfoScript::OnThink );	// |-- Mirv: Account for GCC strictness
		//SetNextThink( gpGlobals->curtime + delay );
		m_flReturnTime = gpGlobals->curtime + delay;
		m_bFloatActive = 0;
		SetNextThink( gpGlobals->curtime );
	}

	m_pLastOwner = pOwner;
	m_flThrowTime = gpGlobals->curtime;

	SetOwnerEntity( NULL );

	PlayDroppedAnim();

	CFFLuaSC hObject( 1, m_pLastOwner );
	_scriptman.RunPredicates_LUA( this, &hObject, "ondrop" );
	_scriptman.RunPredicates_LUA( this, &hObject, "onloseitem" );

	Omnibot::Notify_ItemDropped(this);
}

void CFFInfoScript::Drop( float delay, float speed )
{
	CFFPlayer *pOwner = ToFFPlayer( GetOwnerEntity() );
	AssertMsg( pOwner, "Objects can only be attached to players currently!" );
	if( !pOwner )
		return;

	Vector vecOrigin = pOwner->GetAbsOrigin();
	Vector vecForward, vecRight, vecUp;
	//pOwner->GetVectors( &vecForward, &vecRight, &vecUp );
	pOwner->EyeVectors( &vecForward, &vecRight, &vecUp );

	VectorNormalize( vecForward );
	VectorNormalize( vecRight );
	VectorNormalize( vecUp );

	// Bug #0000429: Flags dropped on death move with the same velocity as the dead player
	Vector vel = Vector( 0, 0, 30.0f ); // owner->GetAbsVelocity();

	if( speed )
	{
		vel += vecForward * speed;
		vel += vecUp * (speed / FLAG_THROWUP);
	}
	// Mirv: Don't allow a downwards velocity as this will make it float instead
	if( vel.z < 1.0f )
		vel.z = 1.0f;

	Drop(delay, vecOrigin, vel);
}

//-----------------------------------------------------------------------------
// Purpose: Returns & clears think function
//-----------------------------------------------------------------------------
void CFFInfoScript::ForceReturn( void )
{
	Return();
	SetThink( NULL );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFFInfoScript::Return( void )
{
	// Don't do anything if removed
	if( IsRemoved() )
		return;

	// #0000220: Capping flag simultaneously with gren explosion results in flag touch.
	// set this to curtime so that it will take a few seconds before it becomes
	// eligible to be picked up again.
	//*
	CFFPlayer *pOwner = ToFFPlayer( GetOwnerEntity() );

	if( pOwner )
	{
		m_flThrowTime = gpGlobals->curtime;
		m_pLastOwner = pOwner;

		CFFLuaSC hOnLoseItem( 1, m_pLastOwner );
		_scriptman.RunPredicates_LUA( this, &hOnLoseItem, "onloseitem" );
	}

	CreateItemVPhysicsObject();

	PlayReturnedAnim();

	Omnibot::Notify_ItemReturned(this);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFFInfoScript::OnThink( void )
{
	// return flag if timer expired
	if ( gpGlobals->curtime > m_flReturnTime )
	{
		CFFLuaSC hOnReturn;
		_scriptman.RunPredicates_LUA( this, &hOnReturn, "onreturn" );

		Return();
	}
	else
	{
		// flag in water
		if ( UTIL_PointContents( GetAbsOrigin() + Vector( 0.0f, 0.0f, FLAG_FLOAT_OFFSET ) ) & CONTENTS_WATER )
		{
			// activate float when flag touches bottom
			if ( ( GetGroundEntity() != NULL ) && !m_bFloatActive )
			{
				m_bFloatActive = 1;
				
				EmitSound( "Flag.FloatDeploy" );
				EmitSound( "Flag.FloatBubbles" );

				g_pEffects->Sparks( GetAbsOrigin() );
				UTIL_Bubbles( GetAbsOrigin(), GetAbsOrigin() + Vector( 0.0f, 0.0f, 64.0f ), 50 );

				SetAbsOrigin( GetAbsOrigin() + Vector( 0.0f, 0.0f, 1.0f ) );
			}

			// make flag float upwards
			if ( m_bFloatActive )
			{
				// very simple drag model to stop the flag carrying on accelerating
				float flFloatForce = ( FLAG_FLOAT_FORCE * sv_gravity.GetFloat() ) - ( FLAG_FLOAT_DRAG * float( GetAbsVelocity().z ) );
				float flFloatSpeed = float( GetAbsVelocity().z ) + flFloatForce * gpGlobals->interval_per_tick;
				SetAbsVelocity( Vector( 0.0f, 0.0f, flFloatSpeed ) );
			}
		}
		// flag not in water but float has been activated
		else if ( m_bFloatActive )
		{
			// apply a reduced upwards force if float is out of water
			float flFloatForce = ( FLAG_FLOAT_FORCE2 * sv_gravity.GetFloat() ) - ( FLAG_FLOAT_DRAG * float( GetAbsVelocity().z ) );
			float flFloatSpeed = float( GetAbsVelocity().z ) + flFloatForce * gpGlobals->interval_per_tick;
			SetAbsVelocity( Vector( 0.0f, 0.0f, flFloatSpeed ) );
		}

		// make flag rotate
		SetAbsAngles(GetAbsAngles() + QAngle(0, FLAG_ROTATION * gpGlobals->interval_per_tick, 0));
		// think next tick
		SetNextThink( gpGlobals->curtime + 0.01f );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFFInfoScript::SetSpawnFlags( int flags )
{
	m_spawnflags = flags;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CFFInfoScript::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	// Force sending even if no model. By default objects
	// without a model aren't sent to the client. And,
	// sometimes we don't want a model.
	return FL_EDICT_ALWAYS;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int	CFFInfoScript::UpdateTransmitState()
{
	if ( IsRemoved() )
		return SetTransmitState( FL_EDICT_DONTSEND );

	return BaseClass::UpdateTransmitState();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFFInfoScript::SetActive( void )
{
	m_iGoalState = GS_ACTIVE;
	DispatchUpdateTransmitState();

	CFFLuaSC hContext;
	_scriptman.RunPredicates_LUA( this, &hContext, "onactive" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFFInfoScript::SetInactive( void )
{
	m_iGoalState = GS_INACTIVE;
	DispatchUpdateTransmitState();

	CFFLuaSC hContext;
	_scriptman.RunPredicates_LUA( this, &hContext, "oninactive" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFFInfoScript::SetRemoved( void )
{
	m_iGoalState = GS_REMOVED;
	m_iPosState = PS_REMOVED;
	DispatchUpdateTransmitState();

	CFFLuaSC hContext;
	_scriptman.RunPredicates_LUA( this, &hContext, "onremoved" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFFInfoScript::SetCarried( void )
{
	m_iPosState = PS_CARRIED;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFFInfoScript::SetDropped( void )
{
	m_iPosState = PS_DROPPED;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFFInfoScript::SetReturned( void )
{
	m_iPosState = PS_RETURNED;
	DispatchUpdateTransmitState();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFFInfoScript::LUA_Remove( void )
{
	// Delay in this situation as weird stuff will
	// happen to the info_ff_script if you remove it
	// in the same frame as spawning it
	if( m_flSpawnTime == gpGlobals->curtime )
	{
		SetThink( &CFFInfoScript::RemoveThink );
		SetNextThink( gpGlobals->curtime + 0.3f );

		return;
	}

	// This sets the object as "removed"	
	FollowEntity( NULL );
	SetOwnerEntity( NULL );

	SetTouch( NULL );

	SetThink( NULL );
	SetNextThink( gpGlobals->curtime );

	SetRemoved();

	Omnibot::Notify_ItemRemove(this);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFFInfoScript::LUA_Restore( void )
{
	// This "restores" the item

	CFFLuaSC hContext;
	_scriptman.RunPredicates_LUA( this, &hContext, "onrestored" );

	// Set some flags so we can call respawn
	SetInactive();
	SetReturned();

	NetworkStateChanged();

	// Respawn the item back at it's starting spot
	Respawn( 0.0f );

	Omnibot::Notify_ItemRestore(this);
}

//-----------------------------------------------------------------------------
// Purpose: This is for when spawn time == curtime when in LUA_Remove code.
//			All this does is fake queue the LUA_Remove for a couple ms
//-----------------------------------------------------------------------------
void CFFInfoScript::RemoveThink( void )
{
	LUA_Remove();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFFInfoScript::SetBotGoalInfo(int _type)
{
	m_BotGoalType = _type;
	m_BotTeamFlags = 0;
	if(m_allowTouchFlags & kAllowBlueTeam && !(m_disallowTouchFlags & kAllowBlueTeam))
		m_BotTeamFlags |= (1<<Omnibot::TF_TEAM_BLUE);
	if(m_allowTouchFlags & kAllowRedTeam && !(m_disallowTouchFlags & kAllowRedTeam))
		m_BotTeamFlags |= (1<<Omnibot::TF_TEAM_RED);
	if(m_allowTouchFlags & kAllowYellowTeam && !(m_disallowTouchFlags & kAllowYellowTeam))
		m_BotTeamFlags |= (1<<Omnibot::TF_TEAM_YELLOW);
	if(m_allowTouchFlags & kAllowGreenTeam && !(m_disallowTouchFlags & kAllowGreenTeam))
		m_BotTeamFlags |= (1<<Omnibot::TF_TEAM_GREEN);
	// FF TODO: Sorry DrEvil, I'm too tired right now to add the class touch flags as well. - Jon
	Omnibot::Notify_GoalInfo(this, m_BotGoalType, m_BotTeamFlags);
}

void CFFInfoScript::SpawnBot(const char *_name, int _team, int _class)
{
	Omnibot::SpawnBotAsync(_name, _team, _class, this);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseEntity *CFFInfoScript::GetCarrier( void )
{
	return IsCarried() ? GetFollowedEntity() : NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseEntity *CFFInfoScript::GetDropper( void )
{
	return IsDropped() ? m_pLastOwner : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get the items origin
//-----------------------------------------------------------------------------
Vector CFFInfoScript::LUA_GetOrigin( void ) const
{
	IPhysicsObject *pObject = VPhysicsGetObject();
	if( pObject )
	{
		Vector vecOrigin;
		QAngle vecAngles;
		pObject->GetPosition( &vecOrigin, &vecAngles );

		return vecOrigin;
	}
	else
	{
		return GetAbsOrigin();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the items origin
//-----------------------------------------------------------------------------
void CFFInfoScript::LUA_SetOrigin( const Vector& vecOrigin )
{
	IPhysicsObject *pObject = VPhysicsGetObject();
	if( pObject )
	{
		pObject->SetPosition( vecOrigin, LUA_GetAngles(), true );
	}
	else
	{
		SetAbsOrigin( vecOrigin );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the items angles
//-----------------------------------------------------------------------------
QAngle CFFInfoScript::LUA_GetAngles( void ) const
{
	IPhysicsObject *pObject = VPhysicsGetObject();
	if( pObject )
	{
		Vector vecOrigin;
		QAngle vecAngles;
		pObject->GetPosition( &vecOrigin, &vecAngles );

		return vecAngles;
	}
	else
	{
		return GetAbsAngles();
	}
}

//-----------------------------------------------------------------------------
// Purpose: set the items angles
//-----------------------------------------------------------------------------
void CFFInfoScript::LUA_SetAngles( const QAngle& vecAngles )
{
	IPhysicsObject *pObject = VPhysicsGetObject();
	if( pObject )
	{
		pObject->SetPosition( LUA_GetOrigin(), vecAngles, true );
	}
	else
	{
		SetAbsAngles( vecAngles );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set a model. Doing this here and not baseentity to catch
//			when someone wants to NOT use a model
//-----------------------------------------------------------------------------
void CFFInfoScript::LUA_SetModel( const char *szModel )
{
	// length > 4 because you have to have ".mdl"...
	if( szModel && ( Q_strlen( szModel ) > 4 ) )
	{
		m_iHasModel = 1;
		SetModel( szModel );
	}
	else
	{
		// No model!
		m_iHasModel = 0;
	}
}

const char * CFFInfoScript::LUA_GetModel()
{
	const model_t *model = GetModel();
	if (model)
	{
		return modelinfo->GetModelName( model );
	}
	return NULL;
}

void CFFInfoScript::LUA_SetStartOrigin(const Vector& vecOrigin)
{
	m_vStartOrigin = vecOrigin;
}

Vector CFFInfoScript::LUA_GetStartOrigin() const
{
	return m_vStartOrigin;
}

void CFFInfoScript::LUA_SetStartAngles(const QAngle& vecAngles)
{
	m_vStartAngles = vecAngles;
}

QAngle CFFInfoScript::LUA_GetStartAngles() const
{
	return m_vStartAngles;
}

//-----------------------------------------------------------------------------
// Purpose: Custom fly code. No longer used
//-----------------------------------------------------------------------------
void CFFInfoScript::ResolveFlyCollisionCustom( trace_t& trace, Vector& vecVelocity )
{
	BaseClass::ResolveFlyCollisionBounce( trace, vecVelocity );
}

//-----------------------------------------------------------------------------
// Purpose: Custom fly code. Change size for physics collisions
//-----------------------------------------------------------------------------
void CFFInfoScript::PhysicsSimulate()
{
	if (!m_bUsePhysics && GetMoveType() == MOVETYPE_FLYGRAVITY)
	{
		UTIL_SetSize( this, m_vecPhysicsMins, m_vecPhysicsMaxs );

		if (VISUALIZE_INFOSCRIPT_SIZES)
			DrawBBoxOverlay();

		BaseClass::PhysicsSimulate();
		
		UTIL_SetSize( this, m_vecTouchMins, m_vecTouchMaxs );
	}
	else
	{
		BaseClass::PhysicsSimulate();
	}
	
	if (VISUALIZE_INFOSCRIPT_SIZES)
		DrawBBoxOverlay();
}

#endif // CLIENT_DLL