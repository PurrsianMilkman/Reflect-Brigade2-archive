#include "windows.h"
#include "brigade.h"
#include "ObjectLamp.h"
#include "World.h"
#include "Game.h"

#include "Player.h"

ObjectLamp::ObjectLamp(void)
{
	enabled = false;
	node = World::scene->GetRoot()->Add("data/lamp/lamp.dae", false);
	Material*oldmat = World::scene->GetMaterial("mat_lamp_emissive");
	object = (ObjectDynamic*)node->GetNodes().GetFirst()->data->GetObjects().GetFirst()->data;
	Material*newmat = World::scene->GetMaterial("LVL4_0");
	if(!newmat)
	{
		newmat = World::scene->CreateMaterial("LVL4_0");
		newmat->emissiveColor = float3(200, 200, 200);
		newmat->Synchronize();
	}
	object->ReplaceMaterial(oldmat, newmat);
	object->Synchronize();
	object->SynchronizeLights();
	angle = speed = 0;
	friction = ((float)(rand() % 1000)/1000.0f) * 0.5f + 0.5f;
	// Log::Message("%f", friction);
}

ObjectLamp::~ObjectLamp(void)
{
	node->Remove();
	delete node;
}

void ObjectLamp::SetTransform(float4x4 _Transform)
{
	transform = _Transform;
	node->SetTransform(transform);
	node->SynchronizeLights();
}

void ObjectLamp::Update()
{
	float3 g = Normalize(World::Gravity);
	float ta = atan2(g.Z, -g.Y);
	float delta = angle - ta;
	// Log::Message("%f %f",ta,delta);
	speed += delta * Game::DT * 15 * friction;
	angle -= speed * Game::DT;
	float limit = 1.3f;
	if(angle<-limit) speed = -speed * 0.75f, angle =-limit;
	if(angle> limit) speed = -speed * 0.75f, angle = limit;
	// speed *= friction;
	speed *= powf(0.90f, Game::DT);
	if(enabled)
	{
		node->SetTransform(float4x4::RotateX(-angle) * transform);
		node->SynchronizeLights();
	}
}
