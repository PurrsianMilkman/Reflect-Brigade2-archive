#include "windows.h"
#include "brigade.h"
#include "ObjectProximity.h"
#include "ObjectEmoCube.h"
#include "Game.h"
#include "World.h"
#include "Player.h"
#include "Script.h"

ObjectProximity::ObjectProximity() : playersnear(0), cubesnear(0)
{

}

void ObjectProximity::Update()
{
	float3 playerpos = Player::GetTransform().W.XYZ;
	float4x4 tolocal = transform.Inverse();
	float3 local = (tolocal*float4(playerpos, 1)).XYZ;
	int localplayersnear = (Length(local) < 1) ? 1 : 0;

	int localcubesnear = 0;
	for(LNode<ObjectEmoCube*>*n = World::EmoCubes.GetFirst(); n; n = n->GetNext())
	{
		float3 pos = n->data->node->GetTransform().W.XYZ;
		float3 local = (tolocal*float4(pos, 1)).XYZ;
		if(Length(local) < 1) localcubesnear++;
	}
	if(localcubesnear != cubesnear || localplayersnear != playersnear)
	{
		cubesnear = localcubesnear;
		playersnear = localplayersnear;
		// Log::Message("%i %i",playersnear, cubesnear );
		Script::TriggerProximity(this);
	}
}

ObjectProximity::~ObjectProximity()
{

}
