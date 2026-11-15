#pragma once
class Vertex;
class Material;
class Scene;
class Node;
class ObjectDynamic;
class ObjectStatic;
class ObjectSky;
class ObjectAnimated;
class ObjectMD5Animation;
class LightTriangle;
class TracerLight;
class Triangle;
class TriangleC;
class TracerBVHNode;
class Bone;
class BVH;
class Mesh;
/// Object
class Object
{
	friend Scene;
	friend BVH;
	friend Node;
	friend ObjectDynamic;
	friend ObjectStatic;
	friend ObjectSky;
	friend ObjectAnimated;
public:
	enum Type { Static, Dynamic, Animated } type;
	// ctor / dtor
	Object( Type _Type ) : type( _Type ), tag( 0 ), mesh( 0 ), flags( 0 ), node( 0 ), name( "" ), lights( 0 ), lightcount( 0 ), lightsdirty( true ), totalarea( 0 ) {}
	virtual ~Object();
	// methods
	virtual void Update() {}
	/// hide or reveal the object
	void SetVisible( bool _Visible );
	/// this the object visible ?
	bool GetVisible() { return (flags & 1) == 0; }
	/// is this object, and all its parents visible
	bool IsVisible();
	bool IsDirty() { return (flags & 2) != 0; }
	/// get a triangle given an index
	Triangle* GetTriangle( int _Idx );
	/// get the total amount of triangles in this object
	int GetTriangleCount();
	/// get a vertex given an index
	Vertex*GetVertex( int _Idx );
	/// get the total amount of vertices in this object
	int GetVertexCount();
	/// replace a material on this object
	void ReplaceMaterial( Material*_OriginalMaterial, Material* _NewMaterial );
	/// replace all materials on this object
	void ReplaceMaterial( Material* _NewMaterial );
	/// makes all transparent surfaces double sided
	void MakeTransparentSurfacesDoubleSided();
	/// gets the scene that owns this object
	Scene* GetScene();
	/// gets the node that owns this object
	Node*GetNode(){return node;}// returns the parent node
	// removes the object from the scenegraph
	void Remove();
	/// gets the mesh for this object
	Mesh* GetMesh(){return mesh;}
	/// Synchronizes the object, call this when you change something in the object ( triangles or the transform... )
	virtual void Synchronize() = 0;
	/// call this when you change lights on the object
	void SynchronizeLights() { lightsdirty = true; }
private:
	void FreeRecurse(int _Addr);
	void SetDirty( bool _Dirty ) { if (_Dirty) flags |= 2; else flags &= ~2; }
	virtual void SetMesh( Mesh* _Mesh );
	uint GetRootNodeAddr();
	// data members
private:
	Node* node;
	bool lightsdirty; // true when lights need to be rebuilt
	TracerLight* lights;
	int lightcount;
	float totalarea;
	Mesh* mesh;
	uint rootaddr;
	uint tailaddr;
	int flags;
public:
	/// name of the object
	string name;
	/// user data
	void* tag;
};

/// ObjectStatic
class ObjectStatic : public Object
{
public:
	ObjectStatic() : Object( Static ) {}
	virtual void Update();
	/// rebuilds the BVH for this object
	void ReBuild();
	void SaveObject( char* _File ); // WIP
	void Synchronize(){}
};

/// ObjectDynamic
class ObjectDynamic : public Object
{
	friend ObjectAnimated;
public:
	// ctor / dtor
	ObjectDynamic() : Object( Dynamic ) {}
	// methods
	void Update();
	void Refit();
	void RefitRecurse(int _NodeAddr);
	void Synchronize() { flags |= 2; }
	/// rebuilds the BVH for this object
	void ReBuild();
	/// sets the transform of the object note that this is relative to its parent
	void SetTransform( float4x4 _Transform )
	{
		transform = _Transform;
		Synchronize();
	}
	/// returns the transform of the object note that this is relative to its parent
	float4x4 GetTransform() { return transform; }
	// data members
private:
	float4x4 worldtransform;
	float4x4 transform;
};

/// ObjectAnimated
class ObjectAnimated : public Object
{
public:
	// methods
	ObjectAnimated( int _BoneCount );
	~ObjectAnimated();
	void SetMesh( Mesh* _Mesh );
	void Update();
	void Refit();
	void RefitRecurse(int _NodeAddr);
	void Synchronize() { flags |= 2; }
	/// rebuilds the BVH for this object
	void ReBuild();
	Vertex* TransformedVertices;
	/// get the amount of bones in this object
	int GetBoneCount() { return bonecount; }
	/// get a bone given its index
	Bone*&GetBone( int _Idx );
	/// get a bone given its index
	Bone* GetBone( char* _Name );
	void NormalizeWeights();
	// data members
private:
	int bonecount;
	Bone** bones;
	float4x4 worldtransform;
	float4x4 transform;
public:
};
