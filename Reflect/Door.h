#pragma once
#include "GameObject.h"
#include "text.h"

#include "vector.h"
using namespace Math;
class Node;
class Animation;
class Door : public GameObject
{
public:
	float state;
	float targetstate;
	float animationspeed;
	string name;
	Node*node;
	Animation*animation;
	Door();
	~Door();
	void SetTransform(float4x4 _Transform);
	void Update();
};
