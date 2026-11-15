#include "windows.h"
#include "brigade.h"
#include "ObjectEmoCube.h"
#include "World.h"
#include "Game.h"
#include "btBulletDynamicsCommon.h"

#include "Player.h"

ObjectEmoCube::ObjectEmoCube(int _Type)
{
	enabled = true;
	btCollisionShape* chassisShape;
	switch (_Type)
	{
	case 0:
		node = World::scene->GetRoot()->Add("data/emocube/emocube.dae", false);
		chassisShape= new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));// make me a convex hull
		break;
	case 1:
		node = World::scene->GetRoot()->Add("data/marble/marble.dae", false);
		chassisShape= new btSphereShape(0.25f);// make me a convex hull
		break;
	}
	
	float mass = 10;
	
	btVector3 localInertia;
	chassisShape->calculateLocalInertia(mass, localInertia);
	btTransform cartransform;cartransform.setIdentity();
	btDefaultMotionState* motionstate = new btDefaultMotionState(cartransform);// transform
	body = new btRigidBody(mass, motionstate, chassisShape, localInertia);
	// body->setRestitution(1);
	// body->setFriction(0);
	World::btdynamicsWorld->addRigidBody(body);
}

void ObjectEmoCube::SetTransform(float4x4 _Transform)
{
	node->SetTransform(_Transform);
	btTransform t;t.setFromOpenGLMatrix(&_Transform.X.X);
	body->setWorldTransform(t);
}

void ObjectEmoCube::Update()
{
	if(enabled) body->activate(); else body->wantsSleeping();
	node->SetVisible(enabled);
	if(!enabled) return;
	float4x4 n = float4x4::Identity();
	float4x4 m;
	body->getWorldTransform().getOpenGLMatrix(&m.X.X);
	node->SetTransform(*(float4x4*)&m);
	if(Player::pickup == this)
	{
		btVector3 target = *(btVector3*)&(Player::GetView() * float4(0,0,2,1)).XYZ;
		btVector3 pos = body->getWorldTransform().getOrigin();
		btVector3 force = (target - pos);
		// force /= force.length();
		force *= 1000;
		force -= body->getLinearVelocity() * 100;
		body->applyImpulse(force * Game::DT, btVector3(0,0,0));
		body->setDamping(0.99f, 0.15f);
	}else body->setDamping(0, 0);
}

ObjectEmoCube::~ObjectEmoCube()
{
	if(node) node->Remove(), node = 0;
	if(body) World::btdynamicsWorld->removeRigidBody(body), body = 0;
}
