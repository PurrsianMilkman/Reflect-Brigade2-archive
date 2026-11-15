#pragma once

#ifndef FLT_MAX
#define FLT_MAX 10000000
#endif

// __declspec(align(16))
class AABB
{
public:
	// ctor / dtor
	AABB()
	{
		min = float3( FLT_MAX, FLT_MAX, FLT_MAX );
		max = float3(-FLT_MAX,-FLT_MAX,-FLT_MAX );
	}
	AABB( const float3& bmin, const float3& bmax ) { min = bmin; max = bmax; }
	// methods
	void Extend( const float3& _p ) { min = Min( min, _p ); max = Max( max, _p ); }
	void Extend( const AABB& bb ) { min = Min( min, bb.min ); max = Max( max, bb.max ); }
	void Union( const AABB& bb ) { min = Max(min, bb.min ); max = Min(max, bb.max ); }
	float Volume() const { const float3 diff = (max - min); return diff.X * diff.Y * diff.Z; }
	float Area() const
	{
		const float3 diff = (max - min);
		return (diff.X * diff.Y + diff.X * diff.Z + diff.Y * diff.Z) * 2.0f;
	}
	float3 Center() const
	{
		float3 c = (min + max) * 0.5f;
		return float3(c.X,c.Y,c.Z);
	}
	const int LongestSide() const
	{
		float s[3] = { max.X - min.X, max.Y - min.Y, max.Z - min.Z };
		int retval = 0;
		if (s[1] > s[0]) retval = 1;
		if (s[2] > s[retval]) retval = 2;
		return retval;
	}
	// data members
	float3 min, max;
};

__declspec(align(16))
class AABBa
{
public:
	// ctor / dtor
	AABBa()
	{
		min = float3a( FLT_MAX, FLT_MAX, FLT_MAX );
		max = float3a(-FLT_MAX,-FLT_MAX,-FLT_MAX );
	}
	AABBa( const float3& bmin, const float3& bmax ) { min = bmin; max = bmax; }
	// methods
	void Extend( const float3& _p ) { min = Min( min, _p ); max = Max( max, _p ); }
	void Extend( const AABBa& bb ) { min = Min( min, bb.min ); max = Max( max, bb.max ); }
	void Union( const AABBa& bb ) { min = Max(min, bb.min ); max = Min(max, bb.max ); }
	float Volume() const { const float3a diff = (max - min); return diff.X * diff.Y * diff.Z; }
	float Area() const
	{
		const float3a diff = (max - min);
		return (diff.X * diff.Y + diff.X * diff.Z + diff.Y * diff.Z) * 2.0f;
	}
	float3 Center() const
	{
		float3a c = (min + max) * 0.5f;
		return float3(c.X,c.Y,c.Z);
	}
	const int LongestSide() const
	{
		float s[3] = { max.X - min.X, max.Y - min.Y, max.Z - min.Z };
		int retval = 0;
		if (s[1] > s[0]) retval = 1;
		if (s[2] > s[retval]) retval = 2;
		return retval;
	}
	// data members
	float3a min, max;
};


#ifdef FLT_MAX
#undef FLT_MAX
#endif