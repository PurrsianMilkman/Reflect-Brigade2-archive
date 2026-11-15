#include "windows.h"
#include "brigade.h"
#include "Player.h"
#include "Game.h"
#include "World.h"
#include "ObjectEmoCube.h"

Node* Player::node = 0;
// float4x4 Player::transform(float4x4::Translate(float3(24.5,2,14.5)));
// float4x4 Player::transform(float4x4::Translate(float3(29,-1.5,22.5))); // hallway 2
// float4x4 Player::transform(float4x4::Translate(float3(1 ,12 ,8.5f))); // marble 1
float4x4 Player::transform(float4x4::Translate(float3(0,2,0)));
float3 Player::velocity(0, 0, 0);
float Player::tilt = 0;
ObjectEmoCube*Player::pickup = 0;

float Player::acceleration;
float Player::runacceleration;
float Player::jumpacceleration;

void Player::Initialize()
{
	acceleration = 20;
	runacceleration = 42.5f;
	jumpacceleration = 5.0f;
/*
	node = World::scene->GetRoot()->Add("data/character/character.dae", false);
	Material*
	mat = World::scene->GetMaterial("mat_character");
	mat->flags |= Material::IGNORESCENETRACE;
	mat->Synchronize();
	mat = World::scene->GetMaterial("mat_mirrorgun");
	mat->flags |= Material::IGNORESCENETRACE;
	mat->Synchronize();
	mat = World::scene->GetMaterial("mat_gravitygun");
	mat->flags |= Material::IGNORESCENETRACE;
	mat->Synchronize();
//*/
}

void Player::Reset()
{
	
}

void Player::Update()
{
	// mouselook
	transform = float4x4::RotateY(-(float)Game::MouseRelative.X / 1000.0f) * transform;
	tilt		+= (float)Game::MouseRelative.Y / 1000.0f;

	if(GetAsyncKeyState('0')) transform.W.XYZ = float3(0,2,0);
	if(GetAsyncKeyState('1')) transform.W.XYZ = float3(29,-1.5,22.5);

	// clamp
	if (tilt <-PI / 2) tilt =-PI / 2;
	if (tilt > PI / 2) tilt = PI / 2;

	transform.W.XYZ += velocity * Game::DT;

	// REFLECT CODE
	float lookahead = 2;
	bool touchesfloor = false;
	for(float y=0;y<3.1415f;y+=0.2f)
	{
		for(float x=0;x<3.1415f*2;x+=0.2f)
		{
			float3 normal;
			float3 d = float3(cosf(x)*sinf(y),-cosf(y),sinf(x)*sinf(y));
			if(d.Y<0)
				d *= float3(0.4f,1.2f , 0.4f);
			else
				d *= float3(0.4f,0.15f, 0.4f);
			d = (transform * float4(d,0)).XYZ;
			// d += velocity * 2;
			float probedist = Length(d);
			float dist = probedist;
			float3 o = transform.W.XYZ;
			Triangle*tri;
			d /= probedist;
			World::scene->Trace(o, d, dist, normal, tri);
			if(tri)
			{
				bool dontcollide = false;
				float i = (probedist - dist);
#if 0
				//this allows the player to go on sharp corners but not on walls
				if(normal.Y < 0.1 && y < 0.2f) dontcollide = true;
				//printf("%f\n",p);
#endif
				if(!dontcollide)
				{
					transform.W.XYZ += normal * (Dot(normal, -d) * i);
					float velinnormaldir = Dot(normal, velocity);
					velocity -= velinnormaldir * normal;
					if(Dot(d, transform.Y.XYZ) < -0.8f) touchesfloor = true;//only indicated touchesfloor when one of the bottom rays hits the floor
				}
				//printf("%s\n",Scene::GetMaterial(tri->GetMaterial())->name);
			}
		}
	}

	GetAsyncKeyState(VK_SPACE);
	if(touchesfloor)
	{
		//only allow input if we're not going up
		if(velocity.Y<1)// only fails first frame of jumping
		{
			float walkspeed = runacceleration; // FIXME
			if (GetAsyncKeyState(VK_SHIFT))walkspeed = acceleration;
			float2 dir(0,0);
			if (GetAsyncKeyState('W')) dir += float2( 0,-1);
			if (GetAsyncKeyState('A')) dir += float2(-1, 0);
			if (GetAsyncKeyState('S')) dir += float2( 0, 1);
			if (GetAsyncKeyState('D')) dir += float2( 1, 0);
			float ld = Length(dir);
			if(ld>0.1f)//if any button pressed
			{
				dir /= ld;//normalize direction so you dont go faster diagonally
				velocity -= (transform * float4(dir.X * walkspeed, 0, dir.Y * walkspeed,0)).XYZ * Game::DT;
			}
			velocity.X *= powf(0.00005f, Game::DT);
			velocity.Y *= powf(0.00005f, Game::DT);
			velocity.Z *= powf(0.00005f, Game::DT);
			if (GetAsyncKeyState(VK_SPACE)) velocity += Normalize(-World::Gravity) * jumpacceleration;
			goto NOAIRFRICTION;
		}
	}
	else velocity += World::Gravity * Game::DT;
	velocity.X *= powf(0.99f, Game::DT);
	velocity.Y *= powf(0.99f, Game::DT);
	velocity.Z *= powf(0.99f, Game::DT);
NOAIRFRICTION:;

	if(pickup)
	{
		if(!(GetAsyncKeyState('E') || Game::LMB)) pickup = 0;
	}else{
		if(GetAsyncKeyState('E') || Game::LMB)
		{
			float bestdot = 0;
			for(LNode<ObjectEmoCube*>*n = World::EmoCubes.GetFirst(); n; n = n->GetNext())
			{
				float3 pos = n->data->node->GetTransform().W.XYZ;
				float4x4 view = GetView();
				float3 dir = pos - view.W.XYZ;
				float dist = Length(dir); dir /= dist;
				float dot = Dot(view.Z.XYZ, dir);
				if(dot>0.95f && dist < 2.2f && dot > bestdot) pickup = n->data, bestdot = dot;
			}
		}
	}

	if(transform.W.Y < -6)transform.W.XYZ = float3(24.5f, 0.5f, 14.5f); // respawn

	// make matrix look up again
	float3 up = -Normalize(World::Gravity);
	float3 left = Normalize(transform.X.XYZ);
	float3 forward = Cross(left,up);
	left = Cross(up,forward);
	transform = float4x4(float4(left,0), float4(up,0), float4(forward,0), transform.W);
	if(node && node->GetTransform() != transform) node->SetTransform(transform);
}

float4x4 Player::GetTransform()
{
	float4x4 t = transform;
	t.W.XYZ -= t.Z.XYZ * 1.2f; // move pivot to ground
	return transform;
}

float4x4 Player::GetView()
{
	return float4x4::RotateX(tilt) * transform;
}
