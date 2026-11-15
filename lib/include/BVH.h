#pragma once
#include "aabb.h"
#include "emmintrin.h"

class Scene;
class Mesh;
class Object;
class Triangle;
class BVH
{
public:
	BVH(void);
	~BVH(void);
public:
	// construction helper class
	class Helper
	{
	public:
		void ClipTo( AABB& box, int axis );
		Triangle* prim;
		AABB box;
		float3 centre;
		int dummy[2];
	};
	class BuildNode
	{
	public:
		bool leaf;
		AABB box;
		BuildNode* left, *right;
		int size;
		int* idx;
		int axis;
		int first;
	};
	// main tree node class
	class Node
	{
	public:
		unsigned int GetAxis() const { return (data >> 29) & 3; }			// 2 bits
		bool IsLeaf() const { return (data & 0x00FFFFFF) != 0; }
		unsigned int GetFirst() const { return (data >> 27) & 1; }			// 1 bit
		unsigned int GetTCount() const { return (data & 0x00FFFFFF); }		// 24 bits
		void SetAxis( unsigned int axis ) { data = (data & (0xFFFFFFFF - (3 << 29))) | (axis << 29); }
		void SetFirst( unsigned int first ) { data = (data & (0xFFFFFFFF - (1 << 27))) | (first << 27); }
		void SetTCount( unsigned int count ) { data = (data & (0xFFFFFFFF - 0x00FFFFFF)) | count; }
		unsigned int GetLeft() const { return child & 0x00FFFFFF; }
		void SetLeft( unsigned int idx ) { child = (child & 0xFF000000)|idx; }
		void SetDirMask( unsigned int mask ) { child = (child & 0x00FFFFFF) + (mask << 24); }
		unsigned int GetDirMask() const { return child >> 24; }	
		void SetStart( unsigned int s ) { start = s; }
		unsigned int GetStart() const { return start; }
		union
		{
			struct
			{
				float3 min;
				union { unsigned int child; unsigned int start; };
				float3 max;
				unsigned int data;
			};
			struct { __m128 min4, max4; };
		};
		float Area() { float3 s = max - min; return (s.X * s.Y + s.Y * s.Z + s.Z * s.X) * 2.0f; }
		void Subdivide( int first, int last, Helper* tri, int offset );
	};
	void DryRun(BVH::Node& _Node, int& _NodeIdx, int& _PrimIdx );
public:
	void Subdivide2( BVH::BuildNode* node, Helper* tri );
	void Convert( BVH::Node* node, BVH::BuildNode* bnode, Helper* tri, int* idx, int& offset );
	void Build( bool presplit, Mesh*_Obj );
	Node* GetRoot() const { return root; }
	void SetRoot( Node* root ) { root = root; }
	int GetPrimCount() const { return pcount; }
	void SetPrimCount( int pcount ) { pcount = pcount; }
	void SAHCostRecurse( BVH::Node* _Node, float& _Area );
	float SAHCost();
	void Refit( Object* _Obj );
	int GetFirstNodeIdx() { return firstnode; }
	int GetNodeCount() { return nodecount; }
	int GetTracerNodeCount() { return tracernodes; }
	int GetTracerPrimCount() { return tracerprims; }
	Scene* scene;
	Node* node;
	int* primidxarray;
	Node* root;
	int rootidx;
	int totalnodes, totaltris;
	int pcount, firstnode, nodecount, helpers;
	float saroot;
	int tracernodes, tracerprims;
	int bnptr, spbudget;
	BuildNode* bnpool;
	bool spwarned;
};