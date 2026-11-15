#pragma once
#include "windows.h"
#include "brigade.h"
#include "ObjectEmoCube.h"
#include "World.h"
#include "Game.h"

#include "Player.h"

class ObjectLamp : public GameObject
{
public:
	ObjectDynamic*object;
	float4x4 transform;
	Node*node;
	ObjectLamp();
	bool enabled;
	void SetEnabled(bool _Enabled) { enabled = _Enabled; node->SetVisible(enabled); }
	float angle, speed, friction;
	void SetTransform(float4x4 _Transform);
	void Update();
	~ObjectLamp();
};
