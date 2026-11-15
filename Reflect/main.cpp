#include "windows.h"
#include "brigade.h"
#include "splash.h"
#include "window.h"
#include "Graphics.h"
#include "bitmap.h"
#include "system.h"

#include "Game.h"
#include "world.h"
#include "Player.h"

#include "Script.h"

#define CONTAINERCOUNT 4

// #define ONEFRAMEBEHIND

int2 ScreenResolution( 1024, 600 );
int2 RenderResolution( ScreenResolution / 2 );
int samplecount = 4;

PathTracer* pathtracer;
Scene* scene;
float4x4 view;

bool Loading = true;
bool ShowHUD = false;

void SaveCam( int _Idx );
void LoadCam( int _Idx );
void LoadCam( char* _File );

// test objects
float3 dragpos;

Timer Clock;
bool dragging = false;

class LoaderThread : public Thread
{
	void run()
	{
		Script::Initialize();
		// init path tracer
		pathtracer = new PathTracer( RenderResolution, samplecount );
		// init scene
		World::Initialize();
		scene = World::scene;
		pathtracer->SetScene( scene );
		view = World::PlayerStartLocations.GetFirst()->GetNext()->data.Transform;
		
		// create material to stick on the sky
	#ifdef SKY_PROBEMAP
		Texture* sky = scene->CreateTexture( "data/probemap.png" );
	#endif
	#ifdef SKY_UTMAP
		Texture* sky = scene->CreateTexture( "data/sky.png" );
	#endif
		sky->Synchronize();	// Synchronize() informs brigade the object is 'dirty'
		
		scene->SetSkyTexture( sky );
		
		scene->ForceUpdate(); // forcefully update the scenegraph, so the first game-frame doesnt take longer than usual ( it will still be a bit longer due to GPU transfers etc. )
		
		scene->Optimize();
		Loading = false;
	}
}
loader;

void SaveCam( int _Idx )
{
	char fname[128];
	sprintf( fname, "%i.cam", _Idx );
	FILE* file = fopen( fname, "wb" );
	if (!file) return;
	fwrite( &view, sizeof( float4x4 ), 1, file );
	fclose( file );
}

void LoadCam( int _Idx )
{
	char fname[128];
	sprintf( fname, "%i.cam", _Idx );
	FILE* file = fopen( fname, "rb" );
	if (!file) return;
	fread( &view, sizeof( float4x4 ), 1, file );
	fclose( file );
}

void LoadCam( char* _File )
{
	FILE* f = fopen( _File, "r" );
	if (!f) return;
	float cx, cy, cz, tx, ty, tz;
	char t[1024];
	fgets( t, 1020, f );
	sscanf( t + 7, "(%f %f %f)", &cx, &cy, &cz );
	fgets( t, 1020, f );
	sscanf( t + 10, "(%f %f %f)", &tx, &ty, &tz );
	fclose( f );
	view = float4x4::LookAt( float3( cx, cy, cz ), float3( tx, ty, tz ), float3( 0, 1, 0 ) );
}

void OutlinerNodeRecurse( Bitmap& _dst, int indent, int& line,Node* _Node, float3 _center )
{
	float3 center = _Node->GetWorldTransform().W.XYZ;
	int2 pos( 4 + indent * 8, 4 + line * 8);
	_dst.Print( pos, byte4( 255, 255, 255, 255 ), "%s", _Node->name.data, center.X, center.Y, center.Z );
	float3 from = pathtracer->GetPixelCoord( _center );
	float3 to   = pathtracer->GetPixelCoord( center );
	if (from.Z > 0 && to.Z > 0)
	{
		int2 f( (int)from.X, (int)from.Y ); f *= ScreenResolution; f /= RenderResolution;
		int2 t( (int)to  .X, (int)to  .Y ); t *= ScreenResolution; t /= RenderResolution;
		_dst.Line( f, t, byte4( 255, 255, 255, 255 ) );
	}
	int hl = MAX( 0, pos.X - 4 );
	line++;
	int vf = pos.Y;
	for( LNode<Object*>* n = _Node->GetObjects().GetFirst(); n; n = n->GetNext() )
	{
		Object* obj = n->data;
		int c = obj->type + 1;
		byte4 color(((c >> 0) & 1) * 255, ((c >> 1) & 1) * 255, ((c >> 2) & 1) * 255, 255);
		//if(obj->name.length==0)
		//	_dst.Print( int2( 4 + indent * 8, 4 + line * 8), color, "%s(%i)", obj->GetMesh()->name.data, obj->GetTriangleCount() );
		//else
			_dst.Print( int2( 4 + indent * 8, 4 + line * 8), color, "%s(%i)", obj->name.data, obj->GetTriangleCount() );
		line++;
	}
	for( LNode<Node*>* n = _Node->GetNodes().GetFirst(); n; n = n->GetNext() )
	{
		OutlinerNodeRecurse( _dst, indent + 1, line, n->data, center );
	}
	int vt = line * 8;
}

int main( int argc, char* argv[] )
{
	OutputDebugString("bladibla");
	//set working directory
	SetCurrentDirectory( System::GetDirectory(argv[0]).data );
	SplashScreen* splashscreen = new SplashScreen();
	Log::Console::SetTitle( "Brigade 2 - Console" );
	LoadCam( 0 );	//load last used cam
	loader.start();	//start loader thread ( this demo application loads its scene async )
	while (Loading)
	{
		splashscreen->Update();
		Sleep( 20 );
	}
	delete splashscreen;
	FreeConsole();
	WindowedWindow* window = new WindowedWindow( ScreenResolution, "Brigade 2", true );
	int2 screensize = int2( GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ) );
	window->SetPosition(screensize / 2, true);
	GLByteTexture* ScreenBuffer = new GLByteTexture( ScreenResolution );
	GLByteTexture* RenderBuffer = new GLByteTexture( RenderResolution );
	// done loading, give window the 'ingame' size
	window->SetPosition( screensize / 2, true );
	int2 LastMousePos;
	float dtime = 0.00001f;
	Clock.Start();
	ShowCursor(false);
	int totalframetime = 1;
	int brigadetime = 1;
#ifdef ONEFRAMEBEHIND
	pathtracer->RenderBegin();
#endif
	while (true)
	{
		Clock.Stop();
		float time = (float)Clock.ElapsedMs() / 1000.0f;
		Timer timer;
		timer.Start();

		//if we can find the lamps from the nutcracker scene, move them around a bit
		bool moved = false;
		float movespeed = 6;
		float rotspeed = 0.8f;
		if (window->Key[ VK_SHIFT ]) movespeed /= 4, rotspeed /= 4;
		if (window->Key[ VK_CONTROL ]) movespeed *= 4;
		// keyboard input
		if (window->Key['D'])		view = float4x4::Translate( float3(-movespeed, 0, 0 ) * dtime ) * view, moved = true;
		if (window->Key['W'])		view = float4x4::Translate( float3( 0, 0, movespeed ) * dtime ) * view, moved = true;
		if (window->Key['A'])		view = float4x4::Translate( float3( movespeed, 0, 0 ) * dtime ) * view, moved = true;
		if (window->Key['R'])		view = float4x4::Translate( float3( 0, movespeed, 0 ) * dtime ) * view, moved = true;
		if (window->Key['F'])		view = float4x4::Translate( float3( 0,-movespeed, 0 ) * dtime ) * view, moved = true;
		if (window->Key['S'])		view = float4x4::Translate( float3( 0, 0,-movespeed ) * dtime ) * view, moved = true;

		if (window->Key['Q'])		view = float4x4::RotateZ(-rotspeed * dtime ) * view, moved = true;
		if (window->Key['E'])		view = float4x4::RotateZ( rotspeed * dtime ) * view, moved = true;
		if (window->Key[VK_RIGHT])	view = float4x4::RotateY(-rotspeed * dtime ) * view, moved = true;
		if (window->Key[VK_LEFT])	view = float4x4::RotateY( rotspeed * dtime ) * view, moved = true;
		if (window->Key[VK_DOWN])	view = float4x4::RotateX(-rotspeed * dtime ) * view, moved = true;
		if (window->Key[VK_UP])		view = float4x4::RotateX( rotspeed * dtime ) * view, moved = true;
		// moved = true;
		Game::LMB = window->LMB;
		Game::MouseGlobal = window->MousePos;
		if (Game::MouseisRelative && GetFocus() == window->HWnd)
		{
			int2 center=window->GetPosition();
			center+=window->GetWindowSize()/2;
			POINT mousepos;
			GetCursorPos(&mousepos);
			int2 bla(mousepos.x,mousepos.y);
			Game::MouseRelative = bla - center;
			SetCursorPos(center.X,center.Y);
		}


		// save/load cam position
		for( int i = 1; i <= 3; i++ ) if (window->Key[ '0' + i ])
		{
			if (window->Key[ VK_CONTROL ]) SaveCam( i ); else LoadCam( i );
			moved = true;
		}
	#if 1 // disable rolling
		view = float4x4::LookAt( view.W.XYZ, (view * float4( 0, 0, 1, 1 )).XYZ, float3( 0, 1, 0 ) );
	#endif
		view.OrthoNormalize();
		view = scene->GetCamera()->GetWorldView();
		// assign view to scene
		// scene->GetCamera()->SetView( view );
		// scene->GetCamera()->SetFOV( 120.0f / 57.28f );
		Timer t; t.Start();
		World::Update();
		t.Stop(); // Log::Message("%i",t.ElapsedUs());
		// pathtracer->SetConverging(!moved);
#ifdef ONEFRAMEBEHIND
		
		pathtracer->RenderEnd();
		pathtracer->RenderBegin();
		t.Stop();
		World::UpdateAsync();
#else
		t.Start();
		pathtracer->RenderBegin();
		World::UpdateAsync();
		pathtracer->RenderEnd();
		t.Stop(); brigadetime = t.ElapsedUs();
#endif

		// present image
		window->Begin();
		Bitmap screen( ScreenResolution, (byte4*)ScreenBuffer->Data );	//wrap a Bitmap around the ScreenBuffer ( so we can print text etc.. )
		memset( screen.GetBuffer(), 0, ScreenResolution.X * ScreenResolution.Y * sizeof( byte4 ) );
		
		Bitmap finalimage = pathtracer->GetFinalImage();
		if (window->Key['C']) finalimage.Save("screenshot.png");
		memcpy(RenderBuffer->Data,finalimage.GetBuffer(),RenderResolution.X*RenderResolution.Y*sizeof(byte4));
		// brightness *= scale * 0.3333f;
		// brightness /= RenderResolution.X * RenderResolution.Y;
		// pathtracer->SetExposure(1.0f/brightness);
		RenderBuffer->UploadData();
		float rayssec = (float)pathtracer->GetRaysPerSecond() / 1000000.0f;
		float maxrayssec = (float)pathtracer->GetMaxRaysPerSecond() / 1000000.0f;
		// HUD
		if (ShowHUD)
		{
			int lines = 5;
			//draw dark bar
		#if 1
			for( int Y = 0; Y < MIN( ScreenResolution.Y, lines * 8 + 8 ); Y++ )
				for( int X = 0; X < ScreenResolution.X; X++ )
					screen.GetBuffer()[X + Y * ScreenResolution.X] = byte4(0,0,0,128);
		#endif
			// print performance stats
			int line = 0;

			double frametime = (double)pathtracer->GetFrameTimeUs() / 1000000.0;
			screen.Print( int2( 4, 4 + line * 8 ), byte4( 255, 255, 255, 255 ), "%.3f ( %.2f ) m/s (%.2f%%)", maxrayssec, rayssec, (rayssec/maxrayssec)*100.0f ) ; line++;
			screen.Print( int2( 4, 4 + line * 8 ), byte4( 255, 255, 255, 255 ), "%.1f FPS", 1.0f / (float)frametime ); line++;
			screen.Print( int2( 4, 4 + line * 8 ), byte4( 255, 255, 255, 255 ), "Scene %ius", scene->GetUpdateTimeUs() ); line++;
			screen.Print( int2( 4, 4 + line * 8 ), byte4( 255, 255, 255, 255 ), "App %ius brigade %ius", totalframetime - brigadetime, brigadetime ); line++;
			screen.Print( int2( 4, 4 + line * 8 ), byte4( 255, 255, 255, 255 ), "Prims %i area %.0f", scene->GetTriangleCount(), scene->GetTotalarea() ); line++;
			// more text here...
			line = 0;
			int index = 0;
			int right = ScreenResolution.X - 250;
			for( int i = 0; i < pathtracer->GetTracerCount(); i++ )
			{
				Tracer* tracer = pathtracer->GetTracers()[i];

				screen.Print( int2(right, 4 + line * 8), byte4(255, 255, 255, 255), "%s(%ius)", tracer->GetName().data, tracer->GetSynchronizeTimeUs()); line++;
				for(LNode<Tracer::Worker*>*e = tracer->workers.GetFirst(); e; e=e->GetNext())
				{
					Tracer::Worker*worker = e->data;
					int c = index + 1; index++;
					// create color for tracer
					byte4 color(((c >> 0) & 1) * 255, ((c >> 1) & 1) * 255, ((c >> 2) & 1) * 255, 255);
					screen.Line( int2( right + 2, 6 + line * 8), int2( right + 6, 6 + line * 8), byte4(255, 255, 255, 255));
					if(e==tracer->workers.GetFirst())
						screen.Line( int2( right + 2, 2 + line * 8), int2( right + 2, 6 + line * 8), byte4(255, 255, 255, 255));
					else
						screen.Line( int2( right + 2, -2 + line * 8), int2( right + 2, 6 + line * 8), byte4(255, 255, 255, 255));
					screen.Print( int2(right + 8, 4 + line * 8), color, "%s(%iMB)", worker->GetName().data, worker->GetRemoteMemoryUsage()); line++;
					// print tracer stats
					if (!tracer->IsEnabled()) color = byte4( 128, 128, 128, 255 );
					
					if (!worker->IsEnabled()) continue;
					// draw tracer indicator lines
					int wfirstline = worker->GetFirstLine();
					int wlastline = wfirstline + worker->GetLineCount();
					for( int y = (wfirstline * ScreenResolution.Y) / RenderResolution.Y; y < (wlastline * ScreenResolution.Y) / RenderResolution.Y; y++)
					{
						screen.Set( int2( ScreenResolution.X - 2, y ), color );
						screen.Set( int2( ScreenResolution.X - 3, y ), color );
					}
				}
			}
			// scene Outline
			int l = lines + 1;
			OutlinerNodeRecurse( screen, 0, l, scene->GetRoot(), float3( 0, 0, 0 ) );
		}
		if (window->Key[VK_F1]) ShowHUD = window->Key[VK_CONTROL] ? false : true;
		if (window->Key['P']) //Probe
		{
			float3 RO = view.W.XYZ;
			float3 RD = view.Z.XYZ;
			float dist = RAYLENGTH;
			Triangle* tri = 0;
			float3 N;
			scene->Trace( RO, RD, dist, N, tri );
			screen.Print( ScreenResolution / 2 + int2( 2, 2 ), byte4( 255, 255, 255,255 ), "dist:%f", dist );
			if (tri)
			{
				screen.Print( ScreenResolution / 2 + int2( 2, 10 ), byte4( 255, 255, 255, 255 ), "mat:%s", tri->material->name );
			}
			for( int i = -2; i <= 2; i++ )
			{
				if (!i) continue;
				screen.Set( ScreenResolution / 2 + int2( i, 0 ), byte4( 255, 255, 255, 255 ) );
				screen.Set( ScreenResolution / 2 + int2( 0, i ), byte4( 255, 255, 255, 255 ) );
			}
			scene->GetCamera()->SetFocalDistance(dist);
			moved=true;
		}
	
		// upload ScreenBuffer to texture
		ScreenBuffer->UploadData();
		// present rendered image
		glViewport( 0, 0, ScreenResolution.X, ScreenResolution.Y );
		glEnable( GL_TEXTURE_2D );
		RenderBuffer->Bind();
		glBegin( GL_QUADS );
		glTexCoord2f( 0, 1 ); glVertex2f( -1, -1 );
		glTexCoord2f( 1, 1 ); glVertex2f(  1, -1 );
		glTexCoord2f( 1, 0 ); glVertex2f(  1,  1 );
		glTexCoord2f( 0, 0 ); glVertex2f( -1,  1 );
		glEnd();
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		ScreenBuffer->Bind();
		glColor3f( 1, 1, 1 );
		glBegin( GL_QUADS );
		glTexCoord2f( 0, 1 ); glVertex2f( -1, -1 );
		glTexCoord2f( 1, 1 ); glVertex2f(  1, -1 );
		glTexCoord2f( 1, 0 ); glVertex2f(  1,  1 );
		glTexCoord2f( 0, 0 ); glVertex2f( -1,  1 );
		glEnd();
		glDisable( GL_BLEND );
		glDisable( GL_TEXTURE_2D );

		window->End();
		window->UpdateWindow();
		// resize buffers if needed
		if (window->GetClientSize() != ScreenResolution)
		{
			ScreenResolution = window->GetClientSize();
			window->ResizeClient(ScreenResolution);
			if (ScreenBuffer) delete ScreenBuffer;
			ScreenBuffer = new GLByteTexture( ScreenResolution );
		#if 0
			RenderResolution = ScreenResolution;
		#endif

		}
		if (RenderResolution.X > ScreenResolution.X) RenderResolution.X = ScreenResolution.X;
		if (RenderResolution.Y > ScreenResolution.Y) RenderResolution.Y = ScreenResolution.Y;
		if (pathtracer->GetResolution() != RenderResolution)
		{
			pathtracer->SetResolution( RenderResolution );
			RenderResolution = pathtracer->GetResolution();
			if (RenderBuffer) delete RenderBuffer;
			RenderBuffer = new GLByteTexture( RenderResolution );
		}
		if (window->Key[ 27 ]) break;
		timer.Stop();
		dtime = timer.ElapsedMs() / 1000.0f;
		if(dtime > 0.1f) dtime = 0.1f;
		Game::DT = dtime;
		float fps = 1.0f / dtime;
		if(fps < 16 && samplecount > 4) samplecount--, pathtracer->SetRaysPerPixel(samplecount);
		if(fps > 20)					samplecount++, pathtracer->SetRaysPerPixel(samplecount);
		Game::DT = 1.0f / (float)MAX(10, fps);
		if(GetAsyncKeyState(VK_ADD))samplecount++;
		if(GetAsyncKeyState(VK_SUBTRACT))samplecount--;
		pathtracer->SetRaysPerPixel(samplecount);
		totalframetime = timer.ElapsedUs();
	}
	// end of application -  save last camera position
	SaveCam( 0 );
	//throw away some things
	delete ScreenBuffer;
	delete window;
	delete scene;
	delete pathtracer;
}
