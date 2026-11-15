#include "windows.h"
#include "glee.h"
#include "vector.h"
#include "Graphics.h"
#include "Bitmap.h"
#include "stdio.h"
using namespace Math;

#include <gl\gl.h>
#include <gl\glext.h>

#include "settings.h"


GLByteTexture::GLByteTexture(int2 _Size)
{
	Size=_Size;
	glGenTextures(1,&texture);

	glBindTexture(GL_TEXTURE_2D,texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, Size.X, Size.Y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
	glBindTexture(GL_TEXTURE_2D,0);

	glGenBuffers(1,&buffer);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB,buffer);
	glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB,Size.X*Size.Y*sizeof(unsigned char)*4, NULL, GL_STREAM_DRAW_ARB );
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0 );

	glBindTexture(GL_TEXTURE_2D, texture );
	glBindBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, buffer );
	Data = (unsigned char*)glMapBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB );
	glBindTexture(GL_TEXTURE_2D,0);
	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB,0);
}

void GLByteTexture::Bind()
{
	//glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB,buffer);
	glBindTexture(GL_TEXTURE_2D,texture);
}

void GLByteTexture::UploadData()
{
	glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB );
	glBindTexture( GL_TEXTURE_2D, texture );
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, buffer );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, Size.X, Size.Y, GL_RGBA, GL_UNSIGNED_BYTE, 0 ); 
}

GLByteTexture::~GLByteTexture()
{
	glBindTexture(GL_TEXTURE_2D,0);
	glBindBuffer(GL_TEXTURE_2D,0);
	glDeleteTextures(1,&texture);
	glDeleteBuffers(1,&buffer);
}

void GLCheckForErrors(char* _FunctionName)
{
	int lasterror=glGetError();
	if(lasterror)
	{
		printf("OpenGL error in %s %i\n", _FunctionName, lasterror);
	}
}

GLTexture::GLTexture(Bitmap&_Bitmap)
{
	Size=_Bitmap.GetSize();
	texture=0;
	glGenTextures(1,&texture);
	glBindTexture(GL_TEXTURE_2D,texture);
		
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, Size.X, Size.Y, 0, GL_BGRA, GL_UNSIGNED_BYTE, _Bitmap.GetBuffer() );
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);	// Linear Filtering
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);	// Linear Filterin
	
	GLCheckForErrors("GLTexture()");
}

void GLTexture::Bind()
{
	if(texture)
	glBindTexture(GL_TEXTURE_2D,texture);
}


GLFont::GLFont(HWND _HWND,char*_Name,int _Size,bool _Bold)
{
	base = glGenLists(96);						// Storage For 96 Characters
	FontSize=_Size;
	font = CreateFont( -FontSize,					// Height Of Font
				0,								// Width Of Font
				0,								// Angle Of Escapement
				0,								// Orientation Angle
				_Bold?FW_BOLD:0,				// Font Weight
				FALSE,							// Italic
				FALSE,							// Underline
				FALSE,							// Strikeout
				ANSI_CHARSET,					// Character Set Identifier
				OUT_TT_PRECIS,					// Output Precision
				CLIP_DEFAULT_PRECIS,			// Clipping Precision
				ANTIALIASED_QUALITY,			// Output Quality
				FF_DONTCARE|DEFAULT_PITCH,		// Family And Pitch
				_Name);							// Font Name
	HDC hDC=GetDC(_HWND);

	oldfont = (HFONT)SelectObject(hDC, font);           // Selects The Font We Want
	wglUseFontBitmaps(hDC, 32, 96, base);				// Builds 96 Characters Starting At Character 32
	SelectObject(hDC, oldfont);							// Selects The Font We Want
	DeleteObject(font);

	GLCheckForErrors("GLFont()");
}

void GLFont::Print(float _X,float _Y,const char*fmt,...)
{
	//glRasterPos2f(_X/Settings::WindowWidth,1-((_Y+(float)(FontSize))/Settings::WindowHeight));
	glRasterPos2f(_X,_Y);
	
	char		text[256];								// Holds Our String
	va_list		ap;										// Pointer To List Of Arguments

	if (fmt == NULL)									// If There's No Text
		return;											// Do Nothing

	va_start(ap, fmt);									// Parses The String For Variables
	vsprintf_s(text, fmt, ap);						// And Converts Symbols To Actual Numbers
	va_end(ap);											// Results Are Stored In Text

	glPushAttrib(GL_LIST_BIT);							// Pushes The Display List Bits
	glListBase(base - 32);								// Sets The Base Character to 32
	glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);	// Draws The Display List Text
	glPopAttrib();										// Pops The Display List Bits
}

GLFont::~GLFont()
{
	
}