#include "windows.h"
#include "brigade.h"
#include "Game.h"
#include "Door.h"
#include "World.h"

Door::Door() : state(0), targetstate(0)
{
	ImportResult result;
	node = World::scene->GetRoot()->Add("data/door/door.dae", result, false);
	animation = result.animations[0];
	targetstate = 1;
	animationspeed = 1;
}

Door::~Door()
{
	node->Remove();
}

void Door::SetTransform(float4x4 _Transform)
{
	node->SetTransform(_Transform);
}

void Door::Update()
{
	bool moved = false;
	float step = Game::DT / animation->GetFrameRate()*animationspeed;
	if(state < targetstate) {state += step; moved = true; if(state > targetstate) state = targetstate; }
	if(state > targetstate) {state -= step; moved = true; if(state < targetstate) state = targetstate; }
	if(moved)
	{
		animation->SetKeyFrame(state * (animation->GetFrameCount() - 1));
		node->Synchronize();
		// Log::Message("%f", state);
	}
}
