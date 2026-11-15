#pragma once
#include "gameobject.h"
class btRigidBody;
class ObjectEmoCube :
	public GameObject
{
public:
	ObjectEmoCube(int _Type);
	bool enabled;
	void SetEnabled(bool _Enabled) { enabled = _Enabled; }
	btRigidBody*body;
	Node*node;
	void SetTransform(float4x4 _Transform);
	void Update();
	~ObjectEmoCube();
};
