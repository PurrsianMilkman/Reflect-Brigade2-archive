#pragma once
#include "GameObject.h"
#include "text.h"

#include "vector.h"
using namespace Math;
class Node;
class Animation;
class ObjectAnimation : public GameObject
{
public:
	float state;
	float targetstate;
	float animationspeed;
	string name;
	Node*node;
	Animation*animation;
	ObjectAnimation();
	~ObjectAnimation();
	void Load(char*_FileName);
	void Update();
};
