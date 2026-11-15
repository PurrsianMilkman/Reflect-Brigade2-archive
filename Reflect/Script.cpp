#include "windows.h"
#include "brigade.h"
#include "script.h"
#include "world.h"
#include "ObjectAnimation.h"
#include "ObjectProximity.h"
#include "ObjectEmoCube.h"
#include "ObjectLamp.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

int LUAmessage(lua_State *L)
{
	int argc = lua_gettop(L);
	string message(0);
	for ( int n=1; n<=argc; ++n ) {
		// Log::Message("argument %i: %s",n,lua_tostring(L, n));
		message = message + string((char*)lua_tostring(L,n));
	}
	Log::Message("LUA: %s", message.data);
	return 0;
}

int LUAwarning(lua_State *L)
{
	int argc = lua_gettop(L);
	string message(0);
	for ( int n=1; n<=argc; ++n ) {
		// Log::Message("argument %i: %s",n,lua_tostring(L, n));
		message = message + string((char*)lua_tostring(L,n));
	}
	Log::Warning("LUA: %s", message.data);
	return 0;
}

int LUAerror(lua_State *L)
{
	int argc = lua_gettop(L);
	string message(0);
	for ( int n=1; n<=argc; ++n ) {
		// Log::Message("argument %i: %s",n,lua_tostring(L, n));
		message = message + string((char*)lua_tostring(L,n));
	}
	Log::Error("LUA: %s", message.data);
	return 0;
}

int LUAAnimationSetTarget(lua_State* L)
{
	string animname = string((char*)lua_tostring(L,1));
	float targetstate = lua_tonumber(L,2);

	ObjectAnimation*anim = World::GetAnimation(animname.data);
	if(anim)
	{
		anim->targetstate = targetstate;
	}else Log::Warning("Failed to find Animation \"%s\"", animname.data);
	return 0;
}

int LUAAnimationLoad(lua_State* L)
{
	string animname = string((char*)lua_tostring(L,1));
	string filename = string((char*)lua_tostring(L,2));

	ObjectAnimation*anim = World::GetAnimation(animname.data);
	if(anim)
	{
		anim->Load(filename.data);
	}else Log::Warning("Failed to find Animation \"%s\"", animname.data);
	return 0;
}

int LUAAnimationSetVisible(lua_State* L)
{
	string animname = string((char*)lua_tostring(L,1));
	bool visible = lua_toboolean(L,2);

	ObjectAnimation*anim = World::GetAnimation(animname.data);
	if(anim)
	{
		if(anim->node)anim->node->SetVisible(visible);
	}else Log::Warning("Failed to find Animation \"%s\"", animname.data);
	return 0;
}

int LUAEmoCubeSetVisible(lua_State* L)
{
	string animname = string((char*)lua_tostring(L,1));
	bool visible = lua_toboolean(L,2);

	ObjectEmoCube*anim = World::GetEmoCube(animname.data);
	if(anim)
	{
		if(anim->node)anim->SetEnabled(visible);
	}else Log::Warning("Failed to find Cube \"%s\"", animname.data);
	return 0;
}

int LUALampSetVisible(lua_State* L)
{
	string animname = string((char*)lua_tostring(L,1));
	bool visible = lua_toboolean(L,2);

	ObjectLamp*anim = World::GetLamp(animname.data);
	if(anim)
	{
		if(anim->node)anim->SetEnabled(visible);
	}else Log::Warning("Failed to find Lamp \"%s\"", animname.data);
	return 0;
}

int LUASetGravity(lua_State* L)
{
	float x = lua_tonumber(L,1);
	float y = lua_tonumber(L,2);
	float z = lua_tonumber(L,3);
	World::SetGravity(float3(x, y, z));

	return 0;
}

int LUAActivateArea(lua_State* L)
{
	int area = lua_tointeger(L,1);

	World::ActivateArea(area);

	return 0;
}

void Script::TriggerProximity(ObjectProximity*_Object)
{
	char name[1024];
	sprintf(name,"HandleProximity", _Object->name);

	lua_gettable(lua, LUA_RIDX_GLOBALS);
	lua_getglobal(lua, "HandleProximity");
	lua_pushstring(lua, _Object->name.data);
	lua_pushinteger(lua, _Object->playersnear);
	lua_pushinteger(lua, _Object->cubesnear);
	if (lua_pcall(lua, 3, 0, 0) != 0)
		Log::Error("error running function `f': %s",
		lua_tostring(lua, -1));
}

lua_State* Script::lua;
void Script::Initialize()
{
	lua = luaL_newstate();

	luaopen_io(lua); // provides io.*
	luaopen_base(lua);
	luaopen_table(lua);
	luaopen_string(lua);
	luaopen_math(lua);

	lua_register(lua, "message", LUAmessage);
	lua_register(lua, "warning", LUAwarning);
	lua_register(lua, "error", LUAerror);
	lua_register(lua, "AnimationSetTarget", LUAAnimationSetTarget);
	lua_register(lua, "AnimationLoad", LUAAnimationLoad);
	lua_register(lua, "ActivateArea", LUAActivateArea);
	lua_register(lua, "SetGravity", LUASetGravity);
	lua_register(lua, "AnimationSetVisible", LUAAnimationSetVisible);
	lua_register(lua, "EmoCubeSetVisible", LUAEmoCubeSetVisible);
	lua_register(lua, "LampSetVisible", LUALampSetVisible);

	int iErr = 0;
	if ((iErr = luaL_loadfile(lua, "world.lua")) == 0)
	{
		// Call main...
		// lua_pcall(lua, 0, LUA_MULTRET, 0);
		if ((iErr = lua_pcall (lua, 0, LUA_MULTRET, 0)) == 0)
		{ 
			// Push the function name onto the stack

			
			// Function is located in the Global Table

			lua_gettable (lua, LUA_RIDX_GLOBALS);  
			lua_pcall (lua, 0, 0, 0);
		}else{Log::Error("failed to find something");}
	}else{Log::Error("error in lua file");}
}

void Script::InvokeWorldInitialize()
{
	// Push the function name onto the stack

	lua_pushstring (lua, "Initialize");
	// Function is located in the Global Table

	lua_gettable (lua, LUA_RIDX_GLOBALS);  
	lua_pcall (lua, 0, 0, 0);
}
