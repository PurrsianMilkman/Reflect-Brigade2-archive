#pragma once
#include "bitmap.h"
#include "window.h"
#include "Graphics.h"
class SplashScreen
{
public:
	Bitmap image;
	WindowedWindow*window;
	GLByteTexture*screenbuffer;
	SplashScreen():image( "data/logo.png" )
	{
		window = new WindowedWindow(image.GetSize(),"Splash",false);
		screenbuffer = new GLByteTexture( image.GetSize() );
		int2 screensize = int2(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

		window->SetPosition(screensize / 2, true);
	}
	void Update()
	{
		// copy loading image to exture
		Bitmap screen(image.GetSize(),(byte4*)screenbuffer->Data);
		memcpy( screen.GetBuffer(), image.GetBuffer(), image.GetSize().X * image.GetSize().Y * sizeof( byte4 ) );
		
		// add some text
		// screen.Print(int2(4, LoadingScreen.GetSize().Y-8),byte4(0xff,0xff,0xff,0xff), LoadingString.data);
		// upload texture to GPU
		screenbuffer->UploadData();
		window->Begin();
		glViewport( 0, 0, image.GetSize().X, image.GetSize().Y );
		glEnable( GL_TEXTURE_2D );
		screenbuffer->Bind();
		glColor3f( 1, 1, 1 );
		// draw the texture in a quad
		glBegin( GL_QUADS );
		glTexCoord2f( 0, 1 ); glVertex2f(-1,-1 );
		glTexCoord2f( 1, 1 ); glVertex2f( 1,-1 );
		glTexCoord2f( 1, 0 ); glVertex2f( 1, 1 );
		glTexCoord2f( 0, 0 ); glVertex2f(-1, 1 );
		glEnd();
		glDisable( GL_TEXTURE_2D );
		window->End();
		window->UpdateWindow();
	}
	~SplashScreen()
	{
		delete screenbuffer;
		delete window;
	}
};
