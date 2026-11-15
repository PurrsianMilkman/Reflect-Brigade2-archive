#pragma once
using namespace Math;
class GLByteTexture
{
	GLuint texture;
	GLuint buffer;
	int2 Size;
public:
	unsigned char*Data;

	GLByteTexture(int2 _Size);

	void Bind();

	void UploadData();

	~GLByteTexture();
};

class GLFont
{
public:
	int FontSize;
	HFONT font;
	HFONT oldfont;
	GLuint	base; //opengl font thingy
	GLFont(HWND _HWND,char*_Name,int _Size,bool _Bold);
	void Print(float _X,float _Y,const char*fmt,...);
	~GLFont();	
};
class Bitmap;
class GLTexture
{
	GLuint texture;
	int2 Size;
	
	GLuint buffer;
public:
	
	GLTexture(Bitmap&_Bitmap);
	void Bind();

	int2 GetSize()
	{
		return Size;
	}
};

class GLRenderTarget
{
private:
	int m_Width;
	int m_Height;

	int m_RenderTargets;

	bool m_UseMipMaps;
	
	GLuint m_DepthId; 

	GLuint m_fboId;			// ID of FBO
	GLuint m_TextureId[4];	// ID of textures
	int	m_NumRenderTargets;

	void GetFrameBufferErrorString( GLenum status, char* tmps, unsigned int maxString );

public:
	void Init( int _Width, int _Height, int _RenderTargets, GLuint _PixelFormat=GL_RGBA8, bool _UseMipMaps=false, bool _Filter=true );

	void Start();
	void Stop();

	void Bind(int _Index);

	int GetWidth (){return m_Width ;}
	int GetHeight(){return m_Height;}

};
