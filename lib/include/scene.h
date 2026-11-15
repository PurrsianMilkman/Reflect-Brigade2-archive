#pragma once
#include "log.h"
#include "timer.h"
#include "vector.h"
using namespace Math;
#include "bvh.h"
#include "text.h"
#include "syncdata.h"
#include "object.h"
#include "node.h"

class Scene;
class ObjectDynamic;
class ObjectStatic;
class ObjectAnimated;
class Material;
class ImportResult;

class TracerMaterial
{
	// data members
public:
	int2 diffusemapsize;
	int2 normalmapsize;
	int flags;
	uint firstdiffusepixel;			// index in diffuse array
	uint firstnormalpixel;			// index in normal array
	float specularity;
	float3 diffuseColor;
	float refractionIndex;
	float3 emissiveColor;
	float transparency;
	float absorbance;				// total size: 16 * 32bit; 64 bytes
	float detailmap;				// 0 = disabled >0 = tile factor
	int dummy[2];
	const static int EMITTER = 1;
};
/// Texture
class Texture
{
	friend Scene;
	friend Material;
	// methods
public:
	Texture() { Log::Error( "Please use Scene.CreateMaterial()" ); }
	/// returns the size of the texture ( in pixels )
	int2 GetSize() { return size; }
	/// gets the raw pixel data for this texture
	byte4*GetBuffer() { return (byte4*)element->GetPtr(); }
	/// gets the scene that owns this texture
	Scene*GetScene() { return scene; }
	/// synchronizes the material, call this when you change pixel data
	void Synchronize();
private:
	// data members
	~Texture();
private:
	Scene* scene;
	string filename;
	Collection::Element* element;
	int2 size;
};
/// Material
class Material
{
	friend ObjectDynamic;
	friend ObjectStatic;
	friend ObjectAnimated;
	friend Scene;
	friend Triangle;
	// methods
public:
	/// returns the intensity of the material, results might be wrong if material is not emissve
	float GetIntensity() { return (emissiveColor.X + emissiveColor.Y + emissiveColor.Z) * 0.333333f; }
	/// change the diffuse map ( texture ) of the material
	void SetDiffuseMap( char* _ImageFile );
	/// change the normal map of the material
	void SetNormalMap( char* _ImageFile );
	/// removed all textures on this material
	void RemoveTexture();
	/// gets the diffuse map
	Texture* GetDiffuse() { return diffuse; }
	/// gets the normal map
	Texture* GetNormal () { return normal ; }
	/// returns wether the material is emissive
	bool IsEmissive() { return emissiveColor.X > 0 || emissiveColor.Y > 0 || emissiveColor.Z > 0; }
	/// call this when you change a material property
	void Synchronize();
	// data members
public:
	/// emissive color ( light color ), if you change this value be sure to call SynchronizeLights for the objects that have this material
	float3 emissiveColor;
	/// color of the material
	float3 diffuseColor;
	/// index of refraction of the material
	float refractionIndex;
	/// materials transparency ( 0 = opague 1 = transparent )
	float transparency;
	/// if the material is transparent, this number indicates how many times per unit the material absorbs its diffuseColor
	float absorbance;
	float detailmap; // 0 = disabled >0 = tile factor
	/// name of the material
	string name;
	const static int IGNORESCENETRACE = 16;
	/// flags, can be IGNORESCENETRACE
	int flags;
	/// specularity of material ( 0 = diffuse 1 = specular )
	float specularity;
	/// a point that can be used by the user to store anything he wants
	void* tag;
	void Save(FILE*_File);
	void Load(FILE*_File);
	/// gets the scene that owns this material
	Scene* GetScene() { return scene; }
private:
	int CUDAmatidx;
	Scene* scene;
	Texture* diffuse;
	Texture* normal;
	// ~Material() { Log::Message( "%s", name ); }
};

class TriangleC;
class TracerTriangle
{
public:
	TracerTriangle(){}
	TracerTriangle(const TriangleC&_Triangle);
	// methods
	const bool IsLast()			const	{ return ((int*)woop)[0] & 1; }
	void SetLast( bool _Last )			{ if (_Last) ((int*)woop)[0] |= 1; else ((int*)woop)[0] &= ~1; }
	
	const bool IsDoubleSided()	const	{ return ((int*)woop)[1] & 1; }
	void SetDoubleSided( bool _Sided )	{ if (_Sided)((int*)woop)[1] |= 1; else ((int*)woop)[1] &= ~1; }

	Triangle*GetTriangle() { return triangle; }
	void SetTriangle(Triangle*_Triangle) { triangle = _Triangle; }

	// data members
	float4 woop[3];			//48
#ifdef COMPRESSNORMALS
	unsigned short N0[3];	// 6
	unsigned short N1[3];	// 6
	unsigned short N2[3];	// 6
#else
	float3 N0, N1, N2;		// 36
#endif
	float U0, U1, U2;		// 12
	float V0, V1, V2;		// 12
	unsigned int data;		// 4
	float3 T, B;			// 24
	float area;				// 4
	Triangle*triangle;		// 4 original
};

class TracerBVHNode
{
public:
	// methods
	float3 GetMin() const
	{
		return float3( MIN( Xmin.X, Xmin.Y ), MIN( Ymin.X, Ymin.Y ), MIN( Zmin.X, Zmin.Y ) );
	}
	float3 GetMax() const
	{
		return float3( MAX( Xmax.X, Xmax.Y ), MAX( Ymax.X, Ymax.Y ), MAX( Zmax.X, Zmax.Y ) );
	}
	AABB GetAABB() const
	{
		return AABB( GetMin(), GetMax() );
	}
	// data members
	float2 Xmin, Xmax;
	float2 Ymin, Ymax;
	float2 Zmin, Zmax;
	int GetChild(int _Idx){return child[_Idx];}
	int child[2];
	int dummy[2]; // 64
};

class TracerLight // 64
{
public:
	void Calculate() 
	{
		N = Cross( v1 - v0, v2 - v0 );
		float l = Length( N );
		area = l * 0.5f;
		N /= l;
	}
	// data members
	float3 v0, v1, v2, N;
	float3 emissiveColor;
	float area; // 64
};
class Vertex
{
public:
	Vertex(){}
	Vertex( const Vertex& _V, float4x4 _Transform )
	{
		pos = (_Transform * float4( _V.pos, 1 )).XYZ;
		N = Normalize((_Transform * float4( _V.N, 0 )).XYZ);
		UV = _V.UV;
	}
	float3 pos;
	float3 N;
	float2 UV;
};

class Triangle;
class Bone;
/// Mesh
class Mesh
{
	friend Triangle;
	friend Scene;
public:
	Mesh() : tag( 0 ), bvh( 0 ), triangles( 0 ), name(""){}
	~Mesh();
	BVH* GetBVH();
	/// Gets a triangle given its index
	Triangle* GetTriangle( int _Index );
	/// Gets a vertex given its index
	Vertex* GetVertex( int _Index ) const;
	/// Gets the total amount of triangles in this mesh
	int GetTriangleCount() const { return trianglecount; }
	/// Gets the total amount of vertices in this mesh
	int GetVertexCount() const { return vertexcount; }
	int GetMaxNodeCount() const;
	int GetMaxTriangleCount() const;
	void RemoveBVH() { if(bvh)delete bvh; bvh = 0;}
	// saves mesh/BVH to a file
	void Save( char* _FileName );
	void Save( FILE* _File, ImportResult* _Result );
	bool Load( char* _FileName);
	bool Load( FILE* _File, ImportResult* _Result );
private:
	// data members
	Scene* scene;
	Vertex* vertices;
	Triangle* triangles;
	int trianglecount;
	int vertexcount;
	bool isstatic;
	BVH* bvh;
	List<string> MTLFiles;//list of MTL files used for this mesh, only used for createstaticobject
public:
	/// name of this mesh
	string name;
	/// user data
	void* tag;
};
/// triangle
class Triangle
{
public:
	// methods
	void Initialize( Mesh* _Mesh ) { tag = 0; mesh = _Mesh; flags = 0; ID = (unsigned int)this; }
	int& GetOriginalIndex() { return originalidx; }
	/// gets the material of this triangle
	Material* GetMaterial() { return material; }
	AABB GetAABB()
	{
		return AABB( 
			Min( Min( GetVertex( 0 ).pos, GetVertex( 1 ).pos ), GetVertex( 2 ).pos ), 
			Max( Max( GetVertex( 0 ).pos, GetVertex( 1 ).pos ), GetVertex( 2 ).pos ) );
	}
	/// get the area of this triangle
	float GetArea()
	{
		const float3 e1 = GetVertex( 0 ).pos - GetVertex( 1 ).pos;
		const float3 e2 = GetVertex( 2 ).pos - GetVertex( 0 ).pos;
		return Length( Cross( e1, e2 ) ) * 0.5f;
	}
	// void Transform(float4x4 _Mat);
	void CalculateTangents();
	/// get a vertex
	Vertex& GetVertex( const int _Idx ) const { return *V[_Idx]; } 
	/// make the triangle double sided, this can be usefull for glass objects
	void SetDoubleSided( bool _Dsided ) { if (_Dsided) flags |= 1; else flags &= ~1; }
	/// is the triangle double side ?
	bool IsDoubleSided() { return flags & 1; }
	// data members
	Vertex* V[3];
	float3 T, B;
	int originalidx; // brigade 1
	Material* material;
	Mesh* mesh;
	int flags;
	void* tag;
	unsigned int ID;
};

// compact Triangle
class TriangleC
{
public:
	TriangleC( const Triangle* _Tri )
	{
		for( int i = 0; i < 3; i++ ) V[i] = *_Tri->V[i];
		T = _Tri->T;
		B = _Tri->B;
	}
	TriangleC( const Triangle& _Tri, float4x4 _Transform )
	{
		for( int i = 0; i < 3; i++ ) V[i] = Vertex( *_Tri.V[i], _Transform );
		T = Normalize((_Transform * float4(_Tri.T,0)).XYZ);
		B = Normalize((_Transform * float4(_Tri.B,0)).XYZ);
	}
	// data members
	Vertex V[3];
	float3 T, B;
};

class Node;

class AnimationChannel;
/**
	animation class description
*/
class Animation
{
	friend Scene;
	friend Node;
	AnimationChannel** channel;
	int channelcount;
	Node*node;
	float fps;
public:
	/// returns the frame rate in frames per second
	float GetFrameRate(){return fps;}
	Animation( Node*_Parent, int _ChannelCount ) : channelcount(_ChannelCount) , node(_Parent) { channel = new AnimationChannel*[channelcount]; }
	~Animation()
	{
		delete[]channel; channel = 0;
		channelcount = 0;
	}
	/// jump to a keyframe in the animation, by gradualy taking a different keyframe every frame you can play an animation
	void SetKeyFrame( float _Frame, float _Alpha = 1 );
	// data members
	AnimationChannel*&GetChannel(int _Idx){return channel[_Idx];}
	/// returns the total amount of animation channels
	int GetChannelCount(){return channelcount;}
	/// gets the total amount of frames in this animation
	int GetFrameCount();
	/// gets the name of this animation
	string name;
	/// remove this animation
	void Remove();
};

/**
	the camera object is responsible for providing brigade with a view
	note that a camera can refer to a node if animated using collada	
 */
class Camera
{
	friend Node;
private:
	Node*node;
	float4x4 view;
	float fov;
	float lenssize;
	float focaldist;
	float exposure;
public:
	Camera():node(0),view(float4x4::Identity()){}
	// name of the camera
	string name;
	Node*parent;
	/// removes the camera
	void Remove();
	/// gets the node that owns this camera
	Node*GetNode();
	/// gets the view of the camera relative to its node
	float4x4 GetView() { return view; }
	/// set a view matrix for the camera
	void SetView(float4x4 _View) { view = _View; }
	/// sets the field of view in radians
	void SetFOV(float _FOV) { fov = _FOV; }
	/// gets the field of view in radians
	float GetFOV() { return fov; }
	/// set the exposure of the camera, this basically makes the image brighter, the default value is 1
	void SetExposure( float _Exposure ) { exposure = _Exposure; }
	/// get the exposure of the camera
	float GetExposure() const { return exposure; }
	/// set the lens size of the camera
	void SetLensSize(float _Size) { lenssize = _Size; }
	/// get the lens size of the camera
	float GetLensSize() { return lenssize; }
	/// set the focal distance to a given distance
	void SetFocalDistance(float _Dist) { focaldist = _Dist; }
	/// get the focal distance of the camera, this is the point the image is 'sharp'
	float GetFocalDistance() { return focaldist; }
	/// gets the view of the camera in world, this is what the pathtracer will be rendering from
	float4x4 GetWorldView();
};

class ImportResult
{
public:
	ImportResult(){ materialcount = 0; meshcount = 0; animationcount = 0; scene = 0; materials = 0; meshes = 0; animations = 0; root = 0; }
	~ImportResult()	{Clear(); }
	void Clear()
	{
		if(materials)delete materials;
		if(meshes)delete meshes;
		if(animations)delete animations;
					materialcount = 0; meshcount = 0; animationcount = 0; scene = 0; materials = 0; meshes = 0; animations = 0; root = 0;}
	// data members
	Scene* scene;
	Node* root;
	int materialcount;
	Material** materials;
	int meshcount;
	Mesh** meshes;
	int animationcount;
	Animation** animations;
};

/// Structure describing the relative transform of a node in an animation
class AnimationKeyFrame
{
public:
	Quaternion rotate;
	float3 scale;
	float3 translation;
};

/**
	bone class description
*/
class Bone
{
public:
	class Weight
	{
	public:
		Vertex* vertex;
		float weight;
	};
	// ctor / dtor
	Bone() : node(0) {}
	Bone( int _WeightCount) : name( "" ), node(0)
	{
		weightcount = _WeightCount;
		weights = new Weight[weightcount];
	}
	~Bone()
	{
		if(weights) delete[] weights; weights = 0;
	}
	Node*GetNode();
	// data members
	int weightcount;
	Weight* weights;
	ObjectAnimated* object;
	string name;
	Node*node;
	float4x4 bindTransform;
};

/**
	object holding all keyframes for a given node in an animation
*/
class AnimationChannel
{
	friend Scene;
	friend Node;
	// data members
	AnimationKeyFrame* frames;
	int framecount;
	Node* node;
public:
	// methods
	AnimationChannel(int _FrameCount) : framecount(_FrameCount)
	{
		node = 0;
		frames = new AnimationKeyFrame[framecount];
	}
	~AnimationChannel()
	{
		delete frames; frames = 0;
		framecount = 0;
	}

	AnimationKeyFrame* GetFrame( int _FrameIdx )
	{
		return &frames[_FrameIdx];
	}
	int GetFrameCount() { return framecount; }
	Node*GetNode() { return node; }
	void SetKeyFrame( float _Frame, float _Alpha );
};

/**
	scenegraph, object that holds all resources
*/
class Scene
{
	friend Material;
	friend Object;
	friend ObjectDynamic;
	friend ObjectStatic;
	friend ObjectAnimated;
	friend Texture;
	friend Node;
	// ctor / dtor
public:
	/// creates an instnace of a scene
	Scene();
	/// destroys a scene, and removes everything in it
	~Scene();
	// data access
#ifdef BRIGADEPRIVATE
	Collection& GetTracerHeap() { return tracerheap; }
	LocalBuffer<TracerMaterial>& GetTracerMaterials() { return tracermaterials; }
	Collection128& GetTracerSceneData() { return tracerscenedata; }
	LocalBuffer<TracerLight>& GetTracerLights() { return tracerlights; }
	void Update();
	void FlushChanges()
	{
		tracerheap.Flush();
		tracermaterials.Flush();
		tracerscenedata.Flush();
		tracerlights.Flush();
	}
	bool LoadOBJ( Mesh* _Mesh, char* _OBJFile, float4x4 _Transform = float4x4::Identity() );
	void RegisterObject( Object* _Obj );
	int FindBestMatch( int _a, int* list, int lsize );
	int AddRecurse( BVH::Node& _Node, Object* _Obj, BVH& _BVH, uint& _rootaddr, uint& _tailaddr );
	void BuildTopBVHScanNode( Node* _Node, Object** list, int* node, int* temp, int& pcount, int& todo );
	void BuildTopBVH();
	TracerTriangle& GetPrimForAddr( int _Addr ) { return *(TracerTriangle*)&tracerscenedata[_Addr]; }
	TracerBVHNode&  GetNodeForAddr( int _Addr ) { return *(TracerBVHNode *)&tracerscenedata[_Addr]; }
	void Init();
#endif
#ifdef SUNRAY
	void SetSunDir( float3 _SunDir ) { sundir = _SunDir; }
	float3 GetSunDir() { return sundir; }
	void SetSunColor( float3 _SunColor ) { suncolor = _SunColor; }
	float3 GetSunColor() { return suncolor; }
#endif
	/// returns the amount of triangles in the scene
	int GetTriangleCount() { return tricount; }
	/// loads a MTL file and adds all materials found to the scene
	void LoadMTLFile( char* _MTLFile );
	/// changes the material of the sky
	void SetSkyTexture( Texture* _Texture )
	{
		if (_Texture->GetScene() != this) Log::Error( "Material does not belong to this scene" );
#ifdef SKY_OPTIMIZE
		if (_Texture->GetSize() != int2( 2048, 2048 ))
		{
			Log::Warning( "Sky is not 2048*2048 pixels, disable SKY_OPTIMIZE?" );
		}
#endif
		skytexture = _Texture;
	}
	/// returns the current material of the sky
	Texture*		GetSkyTexture() { return skytexture; }
	// returns the virtual address of the sky texture
	uint			GetSkyTextureVAddr() { return skytexture ? skytexture->element->GetVAddr() : -1; }
	/// sets the color of the sky
	void SetSkyColor( float3 _HorizonColor, float3 _ZenithColor )
	{
		skyzenithcolor  = _ZenithColor;
		skyhorizoncolor = _HorizonColor;
	}
	/// sets the color of the sky
	void SetSkyColor( float3 _Color )
	{
		skyzenithcolor = _Color;
		skyhorizoncolor = _Color;
	}
	/// returns the color of the sky at the horizon
	float3 GetSkyHorizonColor() { return skyhorizoncolor; }
	/// returns the color of the sky at the zenith
	float3 GetSkyZenithColor() { return skyzenithcolor; }
	/// loads a image from a file and creates a texture out of it
	Texture*		CreateTexture( char* _FileName );
	/// creates a material with a given name
	Material*		CreateMaterial( char* _Name );
	/// creates a material with a texture and a name
	Material*		CreateMaterial( char*_DiffuseMapFile, char*_Name );
	
	Mesh*			CreateMesh( FILE* _file, ImportResult* _Result );//FIXME i should not be here
	/// loads a *.mesh file
	Mesh*			CreateMesh( char* _FileName, bool _IsStatic = false );
	/// creates a mesh with a given primitive count
	Mesh*			CreateMesh( int _PrimCount, int _VertexCount = -1, bool _IsStatic = false, char*_Name = 0 );
	/// finds a texture given its name
	Texture*		GetTexture	( char* _Name );
	/// finds a material given its name, materials without a name will not be found
	Material*		GetMaterial	( char* _Name );
	/// finds a mesh give its name
	Mesh*			GetMesh		( char* _Name );
	/// removes a material from the scene ( beware, some mesh might still be using it )
	void RemoveMaterial( Material* _Material );
	/// removes a texture from the scene ( beware, some material might still be using it )
	void RemoveTexture( Texture* _Texture );
	/// removes a mesh from the scene ( beware, some object might still be using it )
	void RemoveMesh( Mesh* _Mesh );
	/// traces a single ray, this can be used for collision
	void Trace( const float3& _RO, const float3& _RD, float& _Dist, float3& _N, Triangle*& _Triangle, bool _BackFaceCulling = true );
	/// removes any unused things from the scenegraph and collapses ( defragments ) the GPU data structures
	void Optimize();
	/// returns the update time for the last frame
	int GetUpdateTimeUs() { return updatetimer.ElapsedUs(); }
	/// forces a update of the scenegraph
	void ForceUpdate();
	/// returns the total amount of area ( light ) in the scene
	float GetTotalarea() { return totalarea; }
	/// returns the total number of light emitting polygons
	int GetLightCount() { return lightcount; }
	// data members
private:
#ifdef SUNRAY
	float3 sundir;
	float3 suncolor;
#endif
	float totalarea;
	int lightcount;
	Timer updatetimer;
	Collection tracerheap;
	LocalBuffer<TracerMaterial>	tracermaterials;
	Collection128				tracerscenedata;
	LocalBuffer<TracerLight>	tracerlights;
	List<Mesh*>		meshes;
	List<Object*>	objects;
	List<Material*>	materials;
	List<Texture*>	textures;
	Node			root;
	Texture*		skytexture;
	float3			skyhorizoncolor;
	float3			skyzenithcolor;
	bool topbvhdirty;
	bool lightsdirty;
	int tricount;
	// the current camera
	Camera*camera;
public:
	List<Material*>& GetMaterials() { return materials; }
	/// returns the root of the scenegraph
	Node* GetRoot() { return &root; }
	/// sets the camera currently used
	void SetCamera(Camera*_Camera) { camera = _Camera; }
	/// returns the camera currently in use
	Camera* GetCamera() { return camera; }
};
