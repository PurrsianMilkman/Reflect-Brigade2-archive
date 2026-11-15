#pragma once
#include "text.h"

class PathTracer;
class Tracer /// 
{
	friend PathTracer;
protected:
	string name;
	PathTracer* pathtracer;
	virtual void SyncBegin() = 0;
	virtual void SyncEnd() = 0;
	virtual void RenderBegin() = 0;
	virtual void RenderEnd() = 0;
public:
	class Worker;
	List<Worker*>workers;
	PathTracer*GetPathTracer() { return pathtracer; } /// gets the pathtracer this Tracer is working for
	
	// ctor / dtor
	Tracer() : tracing( false ), name("tracer") {}
	virtual ~Tracer();
	
	// access methods
	
	// other methods
	virtual void ForceOutOfSync();
	float GetScreenPercentage();

	string GetName() { return name; }
	bool IsEnabled() { return workers.GetSize() > 0; }



	int GetRenderTimeUs();
	int GetSynchronizeTimeUs();
	// int GetRemoteMemoryUsage() { return remotememoryused; }

	bool tracing;
	class Worker ///
	{
		friend PathTracer;
		friend Tracer;
	protected:
		Tracer*tracer;
		bool enabled;
		string name;
		int remotememoryused;
		int rendertime;
		int synctime;
		int firstline;
		int linecount;
		float4* FinalImage;
	public:
		Worker(Tracer*_Tracer) : name("worker"), tracer(_Tracer), enabled(false), FinalImage( 0 ), linecount( 0 ), firstline( 0 )
		{}
		virtual ~Worker() {  }
		Tracer*GetTracer() { return tracer; } /// gets the tracer that owns this worker
		string GetName() { return name; } /// name of the worker, usualy the name of the device the worker is running on
		bool IsEnabled() { return enabled; } /// is the tracer enabled, tracers can be disabled if they malfunction
		int GetFirstLine() { return firstline; } /// the first column this workers has to render
		int GetLineCount() { return linecount; } /// the amount of columns this workers has to render
		int GetRenderTimeUs() { return rendertime; } /// time the worker has spent rendering in Us
		int GetSynchronizeTimeUs() { return synctime; } /// time the worker has spent synchronizing in Us
		int GetRemoteMemoryUsage() { return remotememoryused; } /// amount of memory tracer used
		virtual void ForceOutOfSync() = 0;
	};
};