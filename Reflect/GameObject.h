#pragma once
#include "text.h"

class GameObject
{
public:
	string name;
	virtual void Update() = 0;
	float4x4 transform;
	virtual void SetTransform(float4x4 _Transform)
	{
		transform = _Transform;
	}
};
