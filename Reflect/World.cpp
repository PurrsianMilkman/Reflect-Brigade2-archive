#include "windows.h"
#include "World.h"
#include "Brigade.h"
#include "ObjectAnimation.h"
#include "ObjectProximity.h"
#include "ObjectEmoCube.h"
#include "ObjectLamp.h"
#include "Player.h"
#include "Script.h"
#include "Game.h"

#include "btBulletDynamicsCommon.h"

extern "C"
{
	#include "lua.h"
	#include "lualib.h"
	#include "lauxlib.h"
}

Node*World::world;
int World::PreviousArea = -1;
int World::ActiveArea = -1;
int World::ActiveAreaTime = 0;
Scene* World::scene = 0;
List<StartLocation> World::PlayerStartLocations;
List<GameObject*> World::GameObjects;
List<ObjectAnimation*> World::Animations;
List<ObjectProximity*> World::Proximities;
List<ObjectEmoCube*> World::EmoCubes;
List<ObjectLamp*> World::Lamps;
float3 World::Gravity(0, -10, 0);
float3 World::GravityFrom(0, 0, 0);
float3 World::GravityTarget(0, 0, 0);
float World::GravityTime = 0;
float4x4 World::GravityBasis;

btDynamicsWorld* World::btdynamicsWorld = 0;
btCollisionDispatcher* World::btdispatcher = 0;
btDefaultCollisionConfiguration* World::btcollisionConfiguration = 0;
btDbvtBroadphase* World::btbroadphase = 0;
btConstraintSolver*	World::btsolver = 0;

btRigidBody* BodyForMesh(Object*obj, float mass)
{
	btTriangleMesh * myTriMesh = new btTriangleMesh();
	myTriMesh->preallocateVertices(obj->GetTriangleCount() * 3);
	for(int i = 0; i < obj->GetTriangleCount(); i++)
	{
		Triangle*tri = obj->GetTriangle(i);
		TriangleC tric(*tri, obj->GetNode()->GetTransform());
		// TriangleC tric(tri);
		myTriMesh->addTriangle(
			*(btVector3*)&tric.V[0].pos,
			*(btVector3*)&tric.V[1].pos,
			*(btVector3*)&tric.V[2].pos);
	}
	btVector3 localInertia(0,0,0);
	btCollisionShape* myShape = new btBvhTriangleMeshShape(myTriMesh, true);
	if(mass != 0){
		btBoxShape inertiashape(btVector3(4.5,0.4f,2.0f));// make me a convex hull
		inertiashape.calculateLocalInertia(mass, localInertia);
	}
	btDefaultMotionState* myMotionState = new btDefaultMotionState();
	btRigidBody* body = new btRigidBody(mass, myMotionState, myShape, localInertia);
	World::btdynamicsWorld->addRigidBody(body);
	return body;
}

void World::Initialize()
{
	scene = new Scene();
	scene->SetSkyColor(float3(0,0,0));
	// load level
	world = scene->GetRoot()->Add("data/world/world.dae");
	if(!world) Log::Error("Failed to load world, is maya still saving ?");

	Material*mattube = scene->GetMaterial("mat_tube");
	if(mattube)
	{
		mattube->transparency = 1;
		mattube->specularity = 1;
		mattube->diffuseColor = float3(0.8, 0.8f, 1);
		mattube->absorbance = 1;
		mattube->Synchronize();
	}else Log::Warning("failed to find tube-material");

	// init bullet
	btcollisionConfiguration = new btDefaultCollisionConfiguration();
	btdispatcher = new btCollisionDispatcher(btcollisionConfiguration);
	btbroadphase = new btDbvtBroadphase();
	btSequentialImpulseConstraintSolver* btsolver = new btSequentialImpulseConstraintSolver;
	btdynamicsWorld = new btDiscreteDynamicsWorld(btdispatcher, btbroadphase, btsolver, btcollisionConfiguration);
	SetGravity(float3(0, -10, 0));
	
	BodyForMesh(world->GetObjects().GetFirst()->data, 0);

	// add locators
	ImportResult locators;
	scene->GetRoot()->Add("data/world/locators.dae", locators, false);

	for(LNode<Node*>*n = locators.root->GetNodes().GetFirst(); n; n = n->GetNext())
	{
		Node*node = n->data;
		if(match(n->data->name.data, "animation"))
		{
			ObjectAnimation*obj = new ObjectAnimation();
			obj->SetTransform(node->GetWorldTransform());
			obj->name = string(n->data->name.data + strlen("animation") + 1);
			GameObjects.Add(obj);
			Animations.Add(obj);
		}
		if(match(n->data->name.data, "proximity"))
		{
			ObjectProximity*obj = new ObjectProximity();
			obj->SetTransform(node->GetWorldTransform());
			obj->name = string(n->data->name.data + strlen("proximity") + 1);
			GameObjects.Add(obj);
			Proximities.Add(obj);
		}
		if(match(n->data->name.data, "emocube"))
		{
			ObjectEmoCube*obj = new ObjectEmoCube(0);
			obj->SetTransform(node->GetWorldTransform() * float4x4::Translate(float3(0,1,0)));
			obj->name = string(n->data->name.data + strlen("emocube") + 1);
			GameObjects.Add(obj);
			EmoCubes.Add(obj);
		}
		if(match(n->data->name.data, "marble"))
		{
			ObjectEmoCube*obj = new ObjectEmoCube(1);
			obj->SetTransform(node->GetWorldTransform());
			obj->name = string(n->data->name.data + strlen("marble") + 1);
			GameObjects.Add(obj);
			EmoCubes.Add(obj);
		}
		if(match(n->data->name.data, "lamp"))
		{
			ObjectLamp*obj = new ObjectLamp();
			obj->SetTransform(node->GetWorldTransform());
			obj->name = string(n->data->name.data + strlen("lamp") + 1);
			GameObjects.Add(obj);
			Lamps.Add(obj);
		}
	}

	for(int a = 1; a < 10;a++)
	{
		for(int t = 0; ;t++)
		{
			char matname[1024];
			sprintf(matname, "LVL%i_%i", a, t);
			Material*mat = scene->GetMaterial(matname);
			if(!mat)break;
			mat->emissiveColor.X = -abs(mat->emissiveColor.X);
			mat->emissiveColor.Y = -abs(mat->emissiveColor.Y);
			mat->emissiveColor.Z = -abs(mat->emissiveColor.Z);
			mat->Synchronize();
		}
	}

	for(int i = 0;;i++)
	{
		char name[1024];
		sprintf(name,"start_location_%i", i);
		Node*locator = locators.root->GetNode(name);
		if(locator)
		{
			StartLocation loc;
			loc.Transform = locator->GetWorldTransform();
			loc.name = string(name);
			PlayerStartLocations.Add(loc);
		}else break;
	}

	locators.root->Remove();

	Player::Initialize();

	Script::InvokeWorldInitialize();
}

void World::Reset()
{

}

float fov = 120.0f / 57.0f;
float tfov = 120.0f / 57.0f;
int activeareatimeout = 0;

void World::Update()
{
	Material*mattube = scene->GetMaterial("mat_tube");
	if(mattube)
	{
		if(GetAsyncKeyState('J'))
		{
			mattube->transparency = 0;
			mattube->specularity = 0;
		}
		if(GetAsyncKeyState('K'))
		{
			mattube->transparency = 0;
			mattube->specularity = 1;
		}
		if(GetAsyncKeyState('L'))
		{
			mattube->transparency = 1;
			mattube->specularity = 1;
		}
		mattube->diffuseColor = float3(0.8, 0.8f, 1);
		mattube->absorbance = 1;
		mattube->Synchronize();
	}else Log::Warning("failed to find tube-material");

#if 0
	if(GetAsyncKeyState('F')) gdir -= 1 * Game::DT;
	if(GetAsyncKeyState('G')) gdir += 1 * Game::DT;
	SetGravity(float3(0,-cos(gdir),sin(gdir)) * 10);
#endif
	Player::Update();
	for(LNode<GameObject*>*n = GameObjects.GetFirst(); n; n = n->GetNext()) n->data->Update();
	
	Camera*camera = scene->GetCamera();
	camera->SetView(Player::GetView());
	if(GetAsyncKeyState(VK_RBUTTON))
		tfov = (12 / 57.0f);
	else
		tfov = (120 / 57.0f);
	
	fov -= (fov - tfov) / 2.0f;
	camera->SetFOV(fov);
	// camera->SetLensSize(0.1f);
	// camera->SetFocalDistance(2);

	if(activeareatimeout<GetTickCount())
	{
		char matname[1024];
		sprintf(matname, "LVL%i_%i", ActiveArea, ActiveAreaTime);
		Material*mat = scene->GetMaterial(matname);
		if(mat)
		{
			mat->emissiveColor.X = abs(mat->emissiveColor.X);
			mat->emissiveColor.Y = abs(mat->emissiveColor.Y);
			mat->emissiveColor.Z = abs(mat->emissiveColor.Z);
			mat->Synchronize();
			world->SynchronizeLights();
		}
		sprintf(matname, "LVL%i_%i", PreviousArea, ActiveAreaTime);
		mat = scene->GetMaterial(matname);
		if(mat)
		{
			mat->emissiveColor.X = -abs(mat->emissiveColor.X);
			mat->emissiveColor.Y = -abs(mat->emissiveColor.Y);
			mat->emissiveColor.Z = -abs(mat->emissiveColor.Z);
			mat->Synchronize();
			world->SynchronizeLights();
			
		}
		ActiveAreaTime++;
		activeareatimeout = GetTickCount() + 1000;
	}
	// interpolate gravity
	GravityTime += Game::DT;
	if(GravityTime>1)GravityTime=1;
	Gravity = float3::Lerp(GravityFrom,GravityTarget,GravityTime);
	btdynamicsWorld->setGravity(*(btVector3*)&Gravity);
}

void World::UpdateAsync()
{
	btdynamicsWorld->stepSimulation(Game::DT, 250);
}

ObjectAnimation* World::GetAnimation(char* _Name)
{
	for(LNode<ObjectAnimation*>*n = Animations.GetFirst(); n; n = n->GetNext())
	{
		if(match(n->data->name.data,_Name))
		{
			return n->data;
		}
	}
	return 0;
}

ObjectEmoCube* World::GetEmoCube(char* _Name)
{
	for(LNode<ObjectEmoCube*>*n = EmoCubes.GetFirst(); n; n = n->GetNext())
	{
		if(match(n->data->name.data, _Name))
		{
			return n->data;
		}
	}
	return 0;
}

ObjectLamp* World::GetLamp(char* _Name)
{
	for(LNode<ObjectLamp*>*n = Lamps.GetFirst(); n; n = n->GetNext())
	{
		if(match(n->data->name.data, _Name))
		{
			return n->data;
		}
	}
	return 0;
}

void World::SetGravity(float3 _Gravity)
{
	GravityFrom = Gravity;
	GravityTarget = _Gravity;
	GravityTime = 0;
}
