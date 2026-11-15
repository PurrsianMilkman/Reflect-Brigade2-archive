#pragma once
#include "vector.h"
using namespace Math;

class Node;
class ObjectEmoCube;

class Player
{
	static Node*node;
	
	static float3 velocity;
	static float4x4 transform;
	static float tilt;
	static float acceleration;
	static float runacceleration;
	static float jumpacceleration;
public:
	static ObjectEmoCube*pickup;
	static void Initialize();
	static void Reset();
	static void Update();

	static float4x4 GetTransform();
	static float4x4 GetView();
};
