#pragma once
#include "vector.h"
using namespace Math;
#include "text.h"

#include "GameObject.h"

class Scene;
class ObjectAnimation;
class ObjectProximity;
class ObjectEmoCube;
class ObjectLamp;
class Node;

class btDynamicsWorld;
class btCollisionDispatcher;
class btDefaultCollisionConfiguration;
struct btDbvtBroadphase;
class btConstraintSolver;

class StartLocation
{
public:
	float4x4 Transform;
	string name;
};

class World
{
public:
	static void Initialize();
	static void Reset();
	static int PreviousArea;
	static int ActiveArea;
	static int ActiveAreaTime;
	static void ActivateArea(int _Area) 
	{
		if(ActiveArea == _Area) return;
		PreviousArea = ActiveArea;
		ActiveArea = _Area;
		ActiveAreaTime = 0;
	}
	static void SetGravity(float3 _Gravity);
	static float3 Gravity;
	static float3 GravityFrom;
	static float3 GravityTarget;
	static float GravityTime;
	static float4x4 GravityBasis;
	static Node* world;
	static Scene* scene;
	static List<StartLocation> PlayerStartLocations;
	static void Update();
	static void UpdateAsync();
	static List<GameObject*> GameObjects;
	static List<ObjectAnimation*> Animations;
	static List<ObjectProximity*> Proximities;
	static List<ObjectEmoCube*> EmoCubes;
	static List<ObjectLamp*> Lamps;
	static ObjectAnimation* GetAnimation(char* _Name);
	static ObjectEmoCube* GetEmoCube(char* _Name);
	static ObjectLamp* GetLamp(char* _Name);
	
	static btDynamicsWorld* btdynamicsWorld;
	static btCollisionDispatcher* btdispatcher;
	static btDefaultCollisionConfiguration* btcollisionConfiguration;
	static btDbvtBroadphase* btbroadphase;
	static btConstraintSolver*	btsolver;
};
