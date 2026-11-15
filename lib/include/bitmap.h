#pragma once
#include "vector.h"

// ZE LAYOUT! It is ze horribolz!

using namespace Math;

/// Bitmap
class Bitmap
{
public:
	// ctor / dtor
	/// Load a bitmap from a file
	Bitmap( char* _Filename );
	/// create a bitmap given its size
	Bitmap( int2 _Size );
	/// create a bitmap given its size and an extenal buffer, note that it does NOT create a copy and instead uses the provided buffer
	Bitmap( int2 _Size, byte4* _Buffer );
	~Bitmap();
	// data access
	/// retuns the size of the bitmap
	int2 GetSize() const { return Size; }
	/// get the raw pixel data
	byte4* GetBuffer() const { return Buffer; }
	// data members
	/// clear the bitmap to a certain color
	void Clear(byte4 _Color);
	//void DrawRectangle(int2 _From,int2 _To);
	//void Draw(Bitmap _Bitmap, int2 _Position);
	/// print text to the bitmap
	void Print( int2 _Pos, byte4 _Color, char* _Format, ... );
	/// set the color of a pixel at a given location
	void Set(int2 _Position, byte4 _Color) { Buffer[_Position.X + _Position.Y * Size.X] = _Color; }
	/// save the bitmap to a file
	void Save(char* _FileName);
	/// draw another bitmap in this one
	void Draw(const Bitmap&_Bitmap,int2 _Position,bool _AlphaBlend = false)
	{
		int x1 = MAX(0, _Position.X);
		int y1 = MAX(0, _Position.Y);
		int x2 = MIN(Size.X, _Position.X + _Bitmap.GetSize().X);
		int y2 = MIN(Size.Y, _Position.Y + _Bitmap.GetSize().Y);

		if(_AlphaBlend)
		{
			for(int y=y1;y<y2;y++)
			{
				for(int x=x1;x<x2;x++)
				{
					const byte4&s = _Bitmap.GetBuffer()[x-_Position.X+(y-_Position.Y)*_Bitmap.GetSize().X];
					byte4&d = GetBuffer()[x+y*Size.X];
					int w1 = 255 - s.W;
					d.X = (d.X*w1 + s.X*s.W)>>8;
					d.Y = (d.Y*w1 + s.Y*s.W)>>8;
					d.Z = (d.Z*w1 + s.Z*s.W)>>8;
					d.W = s.W;
				}
			}
		}else{
			for(int y=y1;y<y2;y++)
			{
				// memcpy ?
				for(int x=x1;x<x2;x++)
				{
					GetBuffer()[x+y*Size.X] = _Bitmap.GetBuffer()[x-_Position.X+(y-_Position.Y)*_Bitmap.GetSize().X];
				}
			}
		}
	}
	/// draw a line
	void Line( float2 _From, float2 _To, byte4 _Color)
	{
#if 1 // not checked, from old school project
		float2 ang=_From-_To;
		if (abs(ang.X)>abs(ang.Y)){
			if(_From.X>_To.X){
				int t=(int)_To.Y;
				_To.Y=_From.Y;
				_From.Y=(float)t;
				t=(int)_To.X;
				_To.X=_From.X;
				_From.X=(float)t;
			}
			int mx=(int)MIN(_To.X,Size.X);
			for (int x=(int)MAX(_From.X,0);x<mx;x++)
			{
				int y = int(_From.Y+float(_To.Y-_From.Y)*((x-_From.X)/float(_To.X-_From.X)));
				if (y >= 0 && y < Size.Y){Set(int2(x,y),_Color);}
			}
		}else{
			if(_From.Y>_To.Y){
				int t=(int)_To.Y;
				_To.Y=_From.Y;
				_From.Y=(float)t;
				t=(int)_To.X;
				_To.X=_From.X;
				_From.X=(float)t;
			}
			int my=(int)MIN(_To.Y,Size.Y);
			for (int y=(int)MAX(_From.Y,0);y<my;y++)
			{
				int x = int(_From.X+float(_To.X-_From.X)*((y-_From.Y)/float(_To.Y-_From.Y)));
				if (x >= 0 && x < Size.X){Set(int2(x,y),_Color);}
			}
		}
#else
		if ((_From.X < 0) || (_From.Y < 0) || (_From.X >= Size.X) || (_From.Y >= Size.Y) ||
			(_To.X < 0) || (_To.Y < 0) || (_To.X >= Size.X) || (_To.Y >= Size.Y))
		{
			return;
		}
		float b = _To.X - _From.X;
		float h = _To.Y - _From.Y;
		float l = fabsf( b );
		if (fabsf ( h ) > l) l = fabsf( h );
		int il = (int)l;
		float dx = b / (float)l;
		float dy = h / (float)l;
		for ( int i = 0; i <= il; i++ )
		{
			*(Buffer + (int)_From.X + (int)_From.Y * Size.X) = _Color;
			_From.X += dx, _From.Y += dy;
		}
#endif
	}
	/// draw a line
	void Line( int2 _From, int2 _To, byte4 _Color)
	{
		Line(float2((float)_From.X, (float)_From.Y),float2((float)_To.X, (float)_To.Y),_Color);
	}
private:
	void SetChar( int c, char* c1, char* c2, char* c3, char* c4, char* c5 );
	void InitCharset();

	bool HasOwnership;
	int2 Size;
	byte4* Buffer;

	static char s_Font[51][5][5];
	static int s_Transl[256];
	static bool charsetinitialized;
};
