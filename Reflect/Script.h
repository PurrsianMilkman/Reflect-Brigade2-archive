class lua_State;
class ObjectProximity;
class Script
{
public:
	static lua_State *lua;
	static void Initialize();
	static void InvokeWorldInitialize();
	static void TriggerProximity(ObjectProximity*_Object);
};
