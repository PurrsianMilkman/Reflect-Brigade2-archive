#pragma once
#include "GameObject.h"
#include "text.h"

#include "vector.h"
using namespace Math;

class ObjectProximity : public GameObject
{
public:
	int playersnear;
	int cubesnear;
	float4x4 transform;
	void Update();
	void SetTransform(float4x4 _Transform) { transform = _Transform; }
	ObjectProximity();
	~ObjectProximity();
};
