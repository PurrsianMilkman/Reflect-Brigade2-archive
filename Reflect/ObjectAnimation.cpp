#include "windows.h"
#include "brigade.h"
#include "Game.h"
#include "ObjectAnimation.h"
#include "World.h"

ObjectAnimation::ObjectAnimation() : state(0), targetstate(0), animationspeed(1), node(0)
{
}

void ObjectAnimation::Load(char*_FileName)
{
	animation = 0;
	ImportResult result;
	node = World::scene->GetRoot()->Add(_FileName, result, false);
	if(result.animationcount)
		animation = result.animations[0];
	node->SetTransform(transform);
	animation->SetKeyFrame(0);
}

ObjectAnimation::~ObjectAnimation()
{
	if(node)node->Remove();
}

void ObjectAnimation::Update()
{
	if(!node) return;
	bool moved = false;
	float step = Game::DT / animation->GetFrameRate() * animationspeed;
	if(state < targetstate) {state += step; moved = true; if(state > targetstate) state = targetstate; }
	if(state > targetstate) {state -= step; moved = true; if(state < targetstate) state = targetstate; }
	if(moved && node->IsVisible())
	{
		animation->SetKeyFrame(state * (animation->GetFrameCount() - 1));
		node->Synchronize();
		// Log::Message("%f", state);
	}
}
