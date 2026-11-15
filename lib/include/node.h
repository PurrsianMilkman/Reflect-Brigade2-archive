#pragma once

class Scene;
class AnimationChannel;
class Object;
class Mesh;
class Animation;
class Camera;
class ImportResult;
class Bone;
class ObjectDynamic;
class ObjectStatic;
class ObjectAnimated;

#include "text.h"
#include "stdio.h"

class Node
{
	friend AnimationChannel;
	friend Scene;
public:
	// ctor / dtor
	Node() : name( "" ), parent( 0 ), visible( true ) {}
	~Node()
	{
	}
	// methods
	void Import( char* _File, bool _Optimize, ImportResult* _Result );
	Node* GetParent() { return parent; }
	Node* GetNode( char* _Node );
	Bone* GetBone( char* _Bone );
	Node* AddNode( char* _Name = "" )
	{
		Node* node = new Node();
		node->name = string( _Name );
		node->scene = scene;
		node->parent = this;
		nodes.Add( node );
		return node;
	}
	
	Node*			Add( char* _FileName, bool _Optimize = true );
	Node*			Add( char* _FileName, ImportResult& _Result, bool _Optimize = true );
	ObjectDynamic*	AddObjectDynamic( char* _OBJFile );
	ObjectDynamic*	AddObjectDynamic( Mesh* _Mesh );
	ObjectStatic*	AddObjectStatic( char* _OBJFile );
	ObjectStatic*	AddObjectStatic( Mesh* _Mesh );
	ObjectAnimated* AddObjectAnimated( Mesh* _Mesh, int _BoneCount );
	// adds a camera to this node
	Camera*			AddCamera(string name = "");
	// set the visiblility of this object
	void SetVisible( bool _Visible ) { visible = _Visible; }
	// is this obejct invisible?
	bool GetVisible() { return visible; }
	// is none of the parents invisible?
	bool IsVisible() { return GetVisible() && (parent ? parent->IsVisible() : true); }
	// remove this object
	void Remove();
	List<Object*>& GetObjects() { return objects; }
	List<Node*>& GetNodes() { return nodes; }
	List<Animation*>& GetAnimations() { return animations; }
	List<Camera*>& GetCameras() { return cameras; }
	void SetTransform( float4x4 _Transform ) { transform = _Transform; Synchronize(); }
	float4x4 GetTransform() { return transform; }
	float4x4 GetWorldTransform()
	{
		if (parent)
			return transform * parent->GetWorldTransform();
		else
			return transform;
	}
	Scene*& GetScene();
	void Synchronize();
	void SynchronizeLights();
	void Save(char*_FileName, ImportResult* _Result);
private:
	void Save(FILE*_File, ImportResult* _Result);
	void Load(FILE*_File, ImportResult* _Result);
	void Update();
	// data members
private:
	Scene* scene;
	List<Object*> objects;
	List<Node*> nodes;
	List<Animation*> animations;
	List<Camera*> cameras;
	bool visible;
	bool parentinvisible; // indicates if one of the parents is invisible
	Node* parent;
	float4x4 transform;
public:
	string name;
};
