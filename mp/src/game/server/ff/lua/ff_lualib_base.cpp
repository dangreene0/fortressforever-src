
// ff_lualib_base.cpp

//---------------------------------------------------------------------------
// includes
//---------------------------------------------------------------------------
// includes
#include "cbase.h"
#include "ff_lualib.h"
#include "EntityList.h"
#include "ff_item_flag.h"
#include "ff_triggerclip.h"
#include "ff_projectile_base.h"
#include "ff_team.h"

#include "triggers.h"

// Lua includes
extern "C"
{
	#include "lua.h"
	#include "lualib.h"
	#include "lauxlib.h"
}

#include "LuaBridge/LuaBridge.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------------------------
using namespace luabridge;

//---------------------------------------------------------------------------
/// tostring implemenation for CBaseEntity
// not the cleanest name but yeah
std::string BaseEntityToString(const CBaseEntity& entity)
{
	std::stringstream stream;
	stream << const_cast<CBaseEntity&>(entity).GetClassname() << ":";
	const char* szEntityName = const_cast<CBaseEntity&>(entity).GetName();
	if (szEntityName[0])
	{
		stream << szEntityName << ":";
	}
	stream << entity.entindex();

	return stream.str();
}

//---------------------------------------------------------------------------
void CFFLuaLib::InitBase(lua_State* L)
{
	ASSERT(L);

	getGlobalNamespace(L)
		.beginClass<CGlobalEntityList>("EntityList")
			.addFunction("FirstEntity", &CGlobalEntityList::FirstEnt)
			.addFunction("NextEntity", &CGlobalEntityList::NextEnt)
			.addFunction("NumEntities", &CGlobalEntityList::NumberOfEntities)
		.endClass()

		// CBaseEntity
		.beginClass<CBaseEntity>("BaseEntity")
			.addFunction("__tostring",			&BaseEntityToString)

			.addFunction("EmitSound",			&CBaseEntity::PlaySound)

			.addFunction("StopSound",
				overload<const char*>(&CBaseEntity::StopSound)
			)

			.addFunction("GetClassName",		&CBaseEntity::GetClassname)
			.addFunction("GetName",				&CBaseEntity::GetName)
			.addFunction("SetName",				&CBaseEntity::SetName)
			.addFunction("GetTeam",				&CBaseEntity::GetTeam)
			.addFunction("GetTeamId",			&CBaseEntity::GetTeamNumber)
			.addFunction("GetId",				&CBaseEntity::entindex)
			.addFunction("GetVelocity",			&CBaseEntity::GetAbsVelocity)
			.addFunction("SetVelocity",			&CBaseEntity::SetAbsVelocity)
			.addFunction("GetOwner",			&CBaseEntity::GetOwnerEntity)

			.addFunction("SetModel",
				overload<const char*>(&CBaseEntity::SetModel),
				overload<const char*, int>(&CBaseEntity::SetModel)
			)

            .addFunction("StartTrail",
				overload<int>(&CBaseEntity::StartTrail),
				overload<int, float, float, float>(&CBaseEntity::StartTrail)
			)

            .addFunction("StopTrail",           &CBaseEntity::StopTrail)
			.addFunction("SetSkin",				&CBaseEntity::SetSkin)
			.addFunction("GetOrigin",			&CBaseEntity::GetAbsOrigin)
			.addFunction("SetOrigin",			&CBaseEntity::SetAbsOrigin)
			.addFunction("GetWorldMins",		&CBaseEntity::WorldAlignMins)
			.addFunction("GetWorldMaxs",		&CBaseEntity::WorldAlignMaxs)
			.addFunction("GetAngles",			&CBaseEntity::GetAbsAngles)
			.addFunction("SetAngles",			&CBaseEntity::SetAbsAngles)
			.addFunction("GetAbsFacing",		&CBaseEntity::GetAbsFacing)
			.addFunction("Teleport",			&CBaseEntity::Teleport)
			.addFunction("IsOnFire",			&CBaseEntity::IsOnFire)
			.addFunction("GetGravity",			&CBaseEntity::GetGravity)
			.addFunction("SetGravity",			&CBaseEntity::SetGravity)

			.addFunction("SetRenderColor",
				overload<byte, byte, byte>(&CBaseEntity::SetRenderColor)
			)

			.addFunction("SetRenderMode",		&CBaseEntity::SetRenderMode)

			.addFunction("SetRenderFx",			&CBaseEntity::SetRenderFx)
			.addFunction("GetRenderFx",			&CBaseEntity::GetRenderFx)
			.addFunction("ClearRenderFx",		&CBaseEntity::ClearRenderFx)

			.addFunction("GetFriction",			&CBaseEntity::GetFriction)
			.addFunction("SetFriction",			&CBaseEntity::SetFriction)
		.endClass()

		// CFFProjectileBase
		.deriveClass<CFFProjectileBase, CBaseEntity>("Projectile")
		.endClass()

		// CFFInfoScript
		.deriveClass<CFFInfoScript, CBaseEntity>("InfoScript")

			.addFunction("Drop",
				overload<float, float>(&CFFInfoScript::Drop),
				overload<float, Vector, Vector>(&CFFInfoScript::Drop)
			)

			.addFunction("Pickup",				&CFFInfoScript::Pickup)
			.addFunction("Respawn",				&CFFInfoScript::Respawn)
			.addFunction("Return",				&CFFInfoScript::Return)
			.addFunction("IsCarried",			&CFFInfoScript::IsCarried)
			.addFunction("IsReturned",			&CFFInfoScript::IsReturned)
			.addFunction("IsDropped",			&CFFInfoScript::IsDropped)
			.addFunction("IsActive",			&CFFInfoScript::IsActive)
			.addFunction("IsInactive",			&CFFInfoScript::IsInactive)
			.addFunction("IsRemoved",			&CFFInfoScript::IsRemoved)
			.addFunction("GetCarrier",			&CFFInfoScript::GetCarrier)
			.addFunction("GetDropper",			&CFFInfoScript::GetDropper)
			.addFunction("Remove",				&CFFInfoScript::LUA_Remove)
			.addFunction("Restore",				&CFFInfoScript::LUA_Restore)
			.addFunction("GetOrigin",			&CFFInfoScript::LUA_GetOrigin)
			.addFunction("SetOrigin",			&CFFInfoScript::LUA_SetOrigin)
			.addFunction("GetAngles",			&CFFInfoScript::LUA_GetAngles)
			.addFunction("SetAngles",			&CFFInfoScript::LUA_SetAngles)
			.addFunction("SetBotGoalInfo",		&CFFInfoScript::SetBotGoalInfo)
			.addFunction("SpawnBot",			&CFFInfoScript::SpawnBot)
			.addFunction("SetModel",			&CFFInfoScript::LUA_SetModel) // Leave this!
			.addFunction("GetModel",			&CFFInfoScript::LUA_GetModel)
			.addFunction("SetStartOrigin",		&CFFInfoScript::LUA_SetStartOrigin)
			.addFunction("GetStartOrigin",		&CFFInfoScript::LUA_GetStartOrigin)
			.addFunction("SetStartAngles",		&CFFInfoScript::LUA_SetStartAngles)
			.addFunction("GetStartAngles",		&CFFInfoScript::LUA_GetStartAngles)
			.addFunction("SetTouchFlags",		&CFFInfoScript::SetTouchFlags)
			.addFunction("SetDisallowTouchFlags",&CFFInfoScript::SetDisallowTouchFlags)
			.addFunction("GetAngularVelocity",	&CFFInfoScript::GetLocalAngularVelocity)
			.addFunction("SetAngularVelocity",	&CFFInfoScript::SetLocalAngularVelocity)
		.endClass()

		// CFuncFFScript - trigger_ff_script
		.beginClass<CFuncFFScript>("TriggerScript")
			.addFunction("IsActive", &CFuncFFScript::IsActive)
			.addFunction("IsInactive", &CFuncFFScript::IsInactive)
			.addFunction("IsRemoved", &CFuncFFScript::IsRemoved)
			.addFunction("Remove", &CFuncFFScript::LuaRemove)
			.addFunction("Restore", &CFuncFFScript::LuaRestore)
			.addFunction("IsTouching", &CFuncFFScript::IsTouching)
			//.addFunction("SetLocation",		&CFuncFFScript::LuaSetLocation)
			.addFunction("SetBotGoalInfo", &CFuncFFScript::SetBotGoalInfo)
		.endClass()

		.deriveClass<CFFTriggerClip, CBaseEntity>("TriggerClip")
			.addFunction("SetClipFlags",		&CFFTriggerClip::LUA_SetClipFlags)
		.endClass();

	setGlobal(L, &gEntList, "GlobalEntityList");
};