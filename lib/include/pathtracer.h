#pragma once
#include "vector.h"
#include "timer.h"
#include "log.h"
#include "settings.h"

#include "bitmap.h"
#include "syncdata.h"
using namespace Math;

#include "tracer.h"

class PathTracer;


class Scene;
class CUDATracer;
/// PathTracer
class PathTracer
{
	friend class CUDATracer;
	friend class CUDATracerThread;
	friend class OpenCLTracer;
	friend class OpenCLTracerThread;
	///
	enum LoadBalancer
	{
		None,			/// dont belance the workload
		RobinHood,		/// moves one item from the slowest to the fastest
		Perfect,		/// recalculate load balancing everyframe, requires identical tracers
		PerfectSmooth	/// same as perfect loadbalancer, but smoothed
	};

public:
	// ctor / dtor
	/// creates a pathtracer
	PathTracer( int2 _Resolution, int _RaysPerPixel = 16, bool _DetectTracers = true );
	/// destroys pathtracer
	~PathTracer();
	// data access methods
	/// set the amount of rays that the tracer may spend on each pixel
	void SetRaysPerPixel(int _RaysPerPixel) { raysperpixel = _RaysPerPixel; }
	/// get the amount of rays per pixel
	int GetRaysPerPixel() const { return raysperpixel; }
	/// returns the total amount of samples taken ( when converging )
	int GetSamplesDone() const { return samplesdone; }
	/// set the scene you want to render
	void SetScene( Scene* _Scene );
	/// get the scene the pathtracer is currently rendering
	Scene* GetScene() const { return currentscene; }
	/// get the time the pathtracer neede to render last frame in micro seconds
	int GetFrameTimeUs() const { return frametime; }
	/// the amount of rays per second the tracers were able to shoot
	int GetRaysPerSecond() const { return rayspersecond; }
	/// the maximumum amount of rays per second the tracers would be able to shoot if there was no overhead
	int GetMaxRaysPerSecond() const { return maxrayspersecond; }
	/// get the resolution the pathtracer is rendering at
	int2 GetResolution() const { return resolution; }
	/// get the amount of tracers registered to this pathtracer
	int GetTracerCount() const { return tracercount; }
	/// get a list of tracers
	Tracer** GetTracers() { return tracers; }
	/// get a ray direction for a given pixel coordinate
	float3 GetRayDir( int2 _PixelCoord );
	/// get a pixel coordinate given a position in the world
	float3 GetPixelCoord( float3 _WorldPos );
	/// sets the load balancer defining how the work is spread over active tracers
	void SetLoadBalancer(LoadBalancer _Balancer) { loadbalancer = _Balancer; }
	/// gets the current load balancer
	LoadBalancer GetLoadBalancer() { return loadbalancer; }
	/// get the image rendered
	Bitmap GetFinalImage()
	{
		return Bitmap(FinalImage->GetSize(), FinalImage->GetBuffer());
	}
	void RegisterTracer( char*_DLLName );
	void RegisterTracer( Tracer* _Tracer )
	{
		_Tracer->pathtracer = this;
		tracers[tracercount] = _Tracer;
		// Log::Message( "-   %s", _Tracer->name );
		tracercount++;
	}
	/// change the resolution of the pathtracer
	void SetResolution( int2 _Resolution )
	{
		int ox = _Resolution.X % 8, oy = _Resolution.Y % 4;
		if (ox||oy) Log::Warning( "Resolution must be multiple of 8x4" );
		_Resolution.X -= ox, _Resolution.Y -= oy;
		if (resolution != _Resolution)
		{
			resolution = _Resolution;
			FinalResult.Resize( resolution.X * resolution.Y );
			if(FinalImage)delete FinalImage; FinalImage = 0;
			FinalImage = new Bitmap( resolution );
#ifdef BLOOM
			BloomImage.Resize( resolution.X * resolution.Y );
#endif
		}
	}
	/// start the rendering of a frame ( use RenderEnd() to end the rendering )
	void RenderBegin() { RenderBegin( 0, resolution.Y ); }
	//// get the amount of enabled tracers registered to this pathtracer
	int GetEnabledtracercount()
	{
		int count = 0;
		for( int i = 0; i < tracercount; i++ )
		{
			for(LNode<Tracer::Worker*>*e = tracers[i]->workers.GetFirst();e;e=e->GetNext())
			{
				if (e->data->IsEnabled()) count++;
			}
		}
		return count;
	}
	/// start the rendering of a frame ( use RenderEnd() to end the rendering )
	void RenderBegin( int _FirstLine, int _LineCount );
	/// stop the rendering, if the rendering is still in progress this is a blocking call
	void RenderEnd();
	/// converging means that the pathtracer will save up all samples to make a nicer image, note that objects/the camera shouldnt move if this is enabled
	void SetConverging(bool _On) { converge = _On; }
	/// 
	bool GetConverging() { return converge; }
private:
	LoadBalancer loadbalancer;
	bool converge;
	int samplesdone, rayspersecond, maxrayspersecond, tracercount, raysperpixel, frametime;
	int2 resolution;
	Tracer* tracers[MAXTRACERS];
	Scene* currentscene;
	Timer frametimer, rendertimer;
	Bitmap* FinalImage;
	LocalBuffer<float4> FinalResult;
#ifdef BLOOM
	LocalBuffer<float4> BloomImage;
#endif
};
