#pragma once
#include <math.h>
#include "emmintrin.h"


#ifndef MIN
#	define MIN(a,b)(((a)<(b))?(a):(b))
#endif
#ifndef MAX
#	define MAX(a,b)(((a)<(b))?(b):(a))
#endif
#ifndef MIN4
#	define MIN4(a,b,c,d)(MIN(MIN(a,b),MIN(c,d)))
#endif
#ifndef MAX4
#	define MAX4(a,b,c,d)(MAX(MAX(a,b),MAX(c,d)))
#endif

typedef unsigned int uint;

#define BADFLOAT(x) ((*(uint*)&x & 0x7f000000) == 0x7f000000)
void*mymalloc(int s);
//not the right place for this kind of stuff
#define MALLOC(x,t) ((t*)mymalloc((x)*sizeof(t)))
#define FREE(x)(_aligned_free((x)))
#define CAST(x,t)(reinterpret_cast<t>(x))
#ifdef INLINE
#undef INLINE
#endif
#define INLINE __forceinline
#define PI     3.14159265358979f


//TODO add float operators

namespace Math
{
	typedef unsigned char byte;
	class byte4;
	class int2;
	class int3;
	class float2;
	class float3;

	class byte4
	{
	public:
		union
		{
			struct { byte X, Y, Z, W; };
			byte cell[4];
		};
		byte4(){}
		byte4( const byte _X, const byte _Y, const byte _Z, const byte _W) : X( _X ), Y( _Y ), Z( _Z ), W( _W ) {}

		INLINE byte& operator[] ( unsigned int i ) { return cell[i]; }
		// vector operators
		INLINE void operator += ( byte4 a ){ X += a.X; Y += a.Y; Z += a.Z; W += a.W; }
		INLINE void operator -= ( byte4 a ){ X -= a.X; Y -= a.Y; Z -= a.Z; W -= a.W; }
		INLINE void operator *= ( byte4 a ){ X *= a.X; Y *= a.Y; Z *= a.Z; W *= a.W; }
		INLINE void operator /= ( byte4 a ){ X /= a.X; Y /= a.Y; Z /= a.Z; W /= a.W; }
		// int operators
		INLINE void operator *= ( byte a ) { X *= a; Y *= a; Z *= a; W *= a; }
		INLINE void operator /= ( byte a ) { X /= a; Y /= a; Z /= a; W /= a; }
		// vector operators
		INLINE byte4 operator + ( const byte4& a ) { return byte4( X + a.X, Y + a.Y, Z + a.Z, W + a.W ); }
		INLINE byte4 operator - ( const byte4& a ) { return byte4( X - a.X, Y - a.Y, Z - a.Z, W - a.W ); }
		INLINE byte4 operator * ( const byte4& a ) { return byte4( X * a.X, Y * a.Y, Z * a.Z, W * a.W ); }
		INLINE byte4 operator / ( const byte4& a ) { return byte4( X / a.X, Y / a.Y, Z / a.Z, W / a.W ); }
		// int operators
		INLINE byte4 operator * ( const int& a ) { return byte4( X * a, Y * a, Z / a, W / a ); }
		INLINE byte4 operator / ( const int& a ) { return byte4( X / a, Y / a, Z / a, W / a ); }

		bool operator == ( const byte4& a ) { return a.X == X && a.Y == Y && a.Z == Z && a.W == W; }
		bool operator != ( const byte4& a ) { return a.X != X || a.Y != Y || a.Z != Z || a.Z != Z; }
	};

	class int2
	{
	public:
		union
		{
			struct { int X, Y; };
			int cell[2];
		};
		int2() {}
		int2( const int _X, const int _Y) : X( _X ), Y( _Y ) {}

		INLINE int  operator[] ( int i ) const { return cell[i]; }
		INLINE int& operator[] ( int i ) 	   { return cell[i]; }
		INLINE int2 operator - (	   ) 	   { return int2( -X, -Y ); }

		// vector operators
		INLINE void operator += ( int2 a ) { X += a.X; Y += a.Y; }
		INLINE void operator -= ( int2 a ) { X -= a.X; Y -= a.Y; }
		INLINE void operator *= ( int2 a ) { X *= a.X; Y *= a.Y; }
		INLINE void operator /= ( int2 a ) { X /= a.X; Y /= a.Y; }
		// int operators
		INLINE void operator *= ( int a ) { X *= a; Y *= a; }
		INLINE void operator /= ( int a ) { X /= a; Y /= a; }
		// vector operators
		INLINE int2 operator + ( const int2& a ) { return int2( X + a.X, Y + a.Y ); }
		INLINE int2 operator - ( const int2& a ) { return int2( X - a.X, Y - a.Y ); }
		INLINE int2 operator * ( const int2& a ) { return int2( X * a.X, Y * a.Y ); }
		INLINE int2 operator / ( const int2& a ) { return int2( X / a.X, Y / a.Y ); }
		// int operators
		INLINE int2 operator * ( const int& a ) { return int2( X * a, Y * a ); }
		INLINE int2 operator / ( const int& a ) { return int2( X / a, Y / a ); }

		bool operator == ( const int2& a ) { return a.X == X && a.Y == Y; }
		bool operator != ( const int2& a ) { return a.X != X || a.Y != Y; }
	};

	class int3
	{
	public:
		union
		{
			struct { int X, Y, Z; };
			int cell[3];
		};
		int3() {}
		int3( const int _X, const int _Y, const int _Z) : X( _X ), Y( _Y ), Z( _Z ) {}

		INLINE int  operator[] ( int i ) const { return cell[i]; }
		INLINE int& operator[] ( int i ) 	   { return cell[i]; }

		// vector operators
		INLINE void operator += ( int3 a ) { X += a.X; Y += a.Y; Z += a.Z; }
		INLINE void operator -= ( int3 a ) { X -= a.X; Y -= a.Y; Z -= a.Z; }
		INLINE void operator *= ( int3 a ) { X *= a.X; Y *= a.Y; Z *= a.Z; }
		INLINE void operator /= ( int3 a ) { X /= a.X; Y /= a.Y; Z /= a.Z; }
		// int operators
		INLINE void operator *= ( int a ) { X *= a; Y *= a; Z *= a; }
		INLINE void operator /= ( int a ) { X /= a; Y /= a; Z /= a; }
		// vector operators
		INLINE int3 operator + ( const int3& a ) { return int3( X + a.X, Y + a.Y, Z + a.Z ); }
		INLINE int3 operator - ( const int3& a ) { return int3( X - a.X, Y - a.Y, Z - a.Z ); }
		INLINE int3 operator * ( const int3& a ) { return int3( X * a.X, Y * a.Y, Z * a.Y ); }
		INLINE int3 operator / ( const int3& a ) { return int3( X / a.X, Y / a.Y, Z / a.Z ); }
		// int operators
		INLINE int3 operator * ( const int& a ) { return int3( X * a, Y * a, Z * a ); }
		INLINE int3 operator / ( const int& a ) { return int3( X / a, Y / a, Z / a ); }

		bool operator == ( const int3& a ) { return a.X == X && a.Y == Y && a.Z == Z; }
		bool operator != ( const int3& a ) { return a.X != X || a.Y != Y || a.Z != Z; }
	};

	class float2
	{
	public:
		union
		{
			struct { float X, Y; };
			float cell[2];
		};
		float2() {}
		float2( const float _X, const float _Y ) : X( _X ), Y( _Y ) {}
		float2(int2 a) : X((float)a.X), Y((float)a.Y){}

		INLINE float  operator[] ( int i ) const { return cell[i]; }
		INLINE float& operator[] ( int i ) 		 { return cell[i]; }
		INLINE float2 operator - (		 ) 		 { return float2( -X, -Y ); }

		friend INLINE float2 operator + ( const float2&a, const float2&b ){ return float2(a.X + b.X, a.Y + b.Y); }
		friend INLINE float2 operator - ( const float2&a, const float2&b ){ return float2(a.X - b.X, a.Y - b.Y); }
		friend INLINE float2 operator * ( const float2&a, const float2&b ){ return float2(a.X * b.X, a.Y * b.Y); }
		friend INLINE float2 operator / ( const float2&a, const float2&b ){ return float2(a.X / b.X, a.Y / b.Y); }
		friend INLINE float2 operator + ( const float2&a, const float &b ){ return float2(a.X + b  , a.Y + b  ); }
		friend INLINE float2 operator - ( const float2&a, const float &b ){ return float2(a.X - b  , a.Y - b  ); }
		friend INLINE float2 operator * ( const float2&a, const float &b ){ return float2(a.X * b  , a.Y * b  ); }
		friend INLINE float2 operator / ( const float2&a, const float &b ){ return float2(a.X / b  , a.Y / b  ); }
		friend INLINE float2 operator * ( const float &a, const float2&b ){ return float2(a   * b.X, a   * b.Y); }
		friend INLINE float2 operator / ( const float &a, const float2&b ){ return float2(a   / b.X, a   / b.Y); }

		// vector operators
		INLINE void operator += ( const float2 a ) { *this = *this + a; }
		INLINE void operator -= ( const float2 a ) { *this = *this - a; }
		INLINE void operator *= ( const float2 a ) { *this = *this * a; }
		INLINE void operator /= ( const float2 a ) { *this = *this / a; }
		INLINE void operator *= ( const float  a ) { *this = *this * a; }
		INLINE void operator /= ( const float  a ) { *this = *this / a; }

		bool operator == ( const float2& a ) { return a.X == X && a.Y == Y; }
		bool operator != ( const float2& a ) { return a.X != X || a.Y != Y; }
	};

	class float3
	{
	public:
		union
		{
			struct { float2 XY; };
			struct { float X, Y, Z; };
			float cell[3];
		};
		float3() {}
		float3( const float2 _V, const float _Z ) : XY( _V ), Z( _Z ) {}
		float3( const float _X, const float _Y, const float _Z ) : X( _X ), Y( _Y ), Z( _Z ) {}

		INLINE float  operator[] ( int i ) const { return cell[i]; }
		INLINE float& operator[] ( int i ) 		 { return cell[i]; }
		INLINE float3 operator - (		 ) 		 { return float3( -X, -Y, -Z ); }

		friend INLINE float3 operator + ( const float3&a, const float3&b ){ return float3(a.X + b.X, a.Y + b.Y, a.Z + b.Z); }
		friend INLINE float3 operator - ( const float3&a, const float3&b ){ return float3(a.X - b.X, a.Y - b.Y, a.Z - b.Z); }
		friend INLINE float3 operator * ( const float3&a, const float3&b ){ return float3(a.X * b.X, a.Y * b.Y, a.Z * b.Z); }
		friend INLINE float3 operator / ( const float3&a, const float3&b ){ return float3(a.X / b.X, a.Y / b.Y, a.Z / b.Z); }
		friend INLINE float3 operator * ( const float3&a, const float &b ){ return float3(a.X * b  , a.Y * b  , a.Z * b  ); }
		friend INLINE float3 operator / ( const float3&a, const float &b ){ return float3(a.X / b  , a.Y / b  , a.Z / b  ); }
		friend INLINE float3 operator * ( const float &a, const float3&b ){ return float3(a   * b.X, a   * b.Y, a   * b.Z); }
		friend INLINE float3 operator / ( const float &a, const float3&b ){ return float3(a   / b.X, a   / b.Y, a   / b.Z); }

		// vector operators
		INLINE void operator += ( const float3 a ) { *this = *this + a; }
		INLINE void operator -= ( const float3 a ) { *this = *this - a; }
		INLINE void operator *= ( const float3 a ) { *this = *this * a; }
		INLINE void operator /= ( const float3 a ) { *this = *this / a; }
		INLINE void operator *= ( const float  a ) { *this = *this * a; }
		INLINE void operator /= ( const float  a ) { *this = *this / a; }

		bool operator == ( const float3& a ) { return a.X == X && a.Y == Y && a.Z == Z; }
		bool operator != ( const float3& a ) { return a.X != X || a.Y != Y || a.Z != Z; }

		
		static float3 Lerp(const float3&a, const float3&b, float t) { return a * (1.0f - t) + b * t; }

	};

	//aligned float3 ( for SIMD )
	__declspec(align(16))
	class float3a
	{
	public:
		union
		{
			__m128 quad;
			struct { float2 XY; };
			struct { float X, Y, Z, W; };
			float cell[4];
		};
		float3a() {}
		float3a( const float2 _V, const float _Z ) : XY( _V ), Z( _Z ) {}
		float3a( const float _X, const float _Y, const float _Z ) : X( _X ), Y( _Y ), Z( _Z ) {}
		float3a( const float3 _V ) : X( _V.X ), Y( _V.Y ), Z( _V.Z ) {}

		INLINE float   operator[] ( int i ) const { return cell[i]; }
		INLINE float&  operator[] ( int i ) 	  { return cell[i]; }
		INLINE float3a operator - (		 ) 		  { return float3a( -X, -Y, -Z ); }

		friend INLINE float3a operator + ( const float3a&a, const float3a&b ){ return float3a(a.X + b.X, a.Y + b.Y, a.Z + b.Z); }
		friend INLINE float3a operator - ( const float3a&a, const float3a&b ){ return float3a(a.X - b.X, a.Y - b.Y, a.Z - b.Z); }
		friend INLINE float3a operator * ( const float3a&a, const float3a&b ){ return float3a(a.X * b.X, a.Y * b.Y, a.Z * b.Z); }
		friend INLINE float3a operator / ( const float3a&a, const float3a&b ){ return float3a(a.X / b.X, a.Y / b.Y, a.Z / b.Z); }
		friend INLINE float3a operator * ( const float3a&a, const float  &b ){ return float3a(a.X * b  , a.Y * b  , a.Z * b  ); }
		friend INLINE float3a operator / ( const float3a&a, const float  &b ){ return float3a(a.X / b  , a.Y / b  , a.Z / b  ); }
		friend INLINE float3a operator * ( const float  &a, const float3a&b ){ return float3a(a   * b.X, a   * b.Y, a   * b.Z); }
		friend INLINE float3a operator / ( const float  &a, const float3a&b ){ return float3a(a   / b.X, a   / b.Y, a   / b.Z); }

		// vector operators
		INLINE void operator += ( const float3a&a ) { *this = *this + a; }
		INLINE void operator -= ( const float3a&a ) { *this = *this - a; }
		INLINE void operator *= ( const float3a&a ) { *this = *this * a; }
		INLINE void operator /= ( const float3a&a ) { *this = *this / a; }
		INLINE void operator *= ( const float   a ) { *this = *this * a; }
		INLINE void operator /= ( const float   a ) { *this = *this / a; }
	};


	class float4
	{
	public:
		union
		{
			struct{ float2 XY; float2 ZW; };
			struct{ float3 XYZ; };
			struct{ float X, Y, Z, W; };
			float cell[4];
		};
		float4() {}
		float4( const float2 _XY, const float2 _ZW ) : XY( _XY ), ZW( _ZW ) {}
		float4( const float3 _XYZ, const float _W ) : XYZ( _XYZ ), W( _W ) {}
		float4( const float _X, const float _Y, const float _Z, const float _W) : X( _X ), Y( _Y ), Z( _Z ), W( _W ) {}

		INLINE float  operator[] ( int i ) const { return cell[i]; }
		INLINE float& operator[] ( int i ) 		 { return cell[i]; }
		INLINE float4 operator - (		 ) 		  { return float4( -X, -Y, -Z, -W ); }

		friend INLINE float4 operator + ( const float4&a, const float4&b ){ return float4(a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W); }
		friend INLINE float4 operator - ( const float4&a, const float4&b ){ return float4(a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.W - b.W); }
		friend INLINE float4 operator * ( const float4&a, const float4&b ){ return float4(a.X * b.X, a.Y * b.Y, a.Z * b.Z, a.W * b.W); }
		friend INLINE float4 operator / ( const float4&a, const float4&b ){ return float4(a.X / b.X, a.Y / b.Y, a.Z / b.Z, a.W / b.W); }
		friend INLINE float4 operator * ( const float4&a, const float &b ){ return float4(a.X * b  , a.Y * b  , a.Z * b  , a.W * b  ); }
		friend INLINE float4 operator / ( const float4&a, const float &b ){ return float4(a.X / b  , a.Y / b  , a.Z / b  , a.W / b  ); }
		friend INLINE float4 operator * ( const float &a, const float4&b ){ return float4(a   * b.X, a   * b.Y, a   * b.Z, a   * b.W); }
		friend INLINE float4 operator / ( const float &a, const float4&b ){ return float4(a   / b.X, a   / b.Y, a   / b.Z, a   / b.W); }

		// vector operators
		INLINE void operator += ( const float4 a ) { *this = *this + a; }
		INLINE void operator -= ( const float4 a ) { *this = *this - a; }
		INLINE void operator *= ( const float4 a ) { *this = *this * a; }
		INLINE void operator /= ( const float4 a ) { *this = *this / a; }
		INLINE void operator *= ( const float  a ) { *this = *this * a; }
		INLINE void operator /= ( const float  a ) { *this = *this / a; }
		
		bool operator == ( const float4& a ) const { return a.X == X && a.Y == Y && a.Z == Z && a.W == W; }
		bool operator != ( const float4& a ) const { return a.X != X || a.Y != Y || a.Z != Z || a.Z != Z; }
	};

	static INLINE float3 Min( const float3& a, const float3& b ) { return float3( MIN( a.X, b.X ), MIN( a.Y, b.Y ), MIN( a.Z, b.Z ) ); }
	static INLINE float3a Min( const float3a& a, const float3a& b )
	{
		float3a r;
		r.quad = _mm_min_ps(a.quad,b.quad);
		return r;
	}
	static INLINE int2 Min( const int2& a, const int2& b) { return int2( MIN( a.X, b.X ), MIN( a.Y, b.Y ) ); }

	static INLINE float3 Max( const float3& a, const float3& b) { return float3( MAX( a.X, b.X ), MAX( a.Y, b.Y ), MAX( a.Z, b.Z ) ); }
	static INLINE float3a Max( const float3a& a, const float3a& b )
	{
		float3a r;
		r.quad = _mm_max_ps(a.quad,b.quad);
		return r;
	}
	static INLINE int2 Max( const int2& a, const int2& b) { return int2( MAX( a.X, b.X ), MAX( a.Y, b.Y ) ); }

	// reciprocal
	static INLINE float2 Rcp( const float2& a ) { return float2( 1 / a.X, 1 / a.Y ); }
	static INLINE float3 Rcp( const float3& a ) { return float3( 1 / a.X, 1 / a.Y, 1 / a.Z ); }
	static INLINE float4 Rcp( const float4& a ) { return float4( 1 / a.X, 1 / a.Y, 1 / a.Z, 1 / a.W ); }
	// dot product
	static INLINE float Dot( const float2& a, const float2& b ) { return a.X * b.X + a.Y * b.Y; }
	static INLINE float Dot( const float3& a, const float3& b ) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }
	static INLINE float Dot( const float4& a, const float4& b ) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W; }
	// cross product
	static INLINE float  Cross( const float2& a, const float2& b ) { return a.X * b.Y - a.Y * b.X; }
	static INLINE float3 Cross( const float3& a, const float3& b ) { return float3( a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X ); }
	// vector length
	static INLINE float Length( const float2& _Vec ) { return sqrtf( Dot( _Vec, _Vec ) ); }
	static INLINE float Length( const float3& _Vec ) { return sqrtf( Dot( _Vec, _Vec ) ); }
	static INLINE float Length( const float4& _Vec ) { return sqrtf( Dot( _Vec, _Vec ) ); }
	static INLINE float LengthSquare( const float2& _Vec ) { return Dot( _Vec, _Vec ); }
	static INLINE float LengthSquare( const float3& _Vec ) { return Dot( _Vec, _Vec ); }
	static INLINE float LengthSquare( const float4& _Vec ) { return Dot( _Vec, _Vec ); }
	// vector normalize
	static INLINE float2 Normalize( float2& _Vec ) { return _Vec / Length( _Vec ); }
	static INLINE float3 Normalize( float3& _Vec ) { return _Vec / Length( _Vec ); }
	static INLINE float4 Normalize( float4& _Vec ) { return _Vec / Length( _Vec ); }

	class Quaternion // not checked!
	{
	public:
		float x,y,z,w;
		Quaternion(){}
		Quaternion(float _x,float _y,float _z,float _w):x(_x),y(_y),z(_z),w(_w)
		{
			
		}
		static Quaternion AxisAngle(float3 _Axis, float _Angle)
		{
			Quaternion r;
			float result = sin(_Angle/2.0f);
			r.w = cosf(_Angle/2.0f);
			r.x = _Axis.X * result;
			r.y = _Axis.Y * result;
			r.z = _Axis.Z * result;
			return r;
		}
		Quaternion Quaternion::operator *(const Quaternion &q)
		{
			Quaternion r;
			r.w = w * q.w - x * q.x - y * q.y - z * q.z;
			r.x = w * q.x + x * q.w + y * q.z - z * q.y;
			r.y = w * q.y + y * q.w + z * q.x - x * q.z;
			r.z = w * q.z + z * q.w + x * q.y - y * q.x;
			return r;
		}

		void operator *= ( const Quaternion &q ) { *this = *this * q; }
	};

	/*
	class float3x3
	{
	public:
	union
	{
	struct { float3 Row[3]; };
	struct { float3 X, Y, Z; };
	};
	float3x3() { X = float3( 1, 0, 0 ); Y = float3( 0, 1, 0); Z = float3( 0, 0, 1 ); }
	float3x3( float3 x, float3 y, float3 z ) { X = x; Y = y; Z = z; }
	float3x3( float xx, float xy , float xz, float yx, float yy, float yz, float zx, float zy, float zz )
	{
	X = float3( xx, xy, xz );
	Y = float3( yx, yy, yz );
	Z = float3( zx, zy, zz );
	}

	void operator *= ( const float3x3 a )
	{
	float3x3 b = *this;
	*this = b * a;
	}
	float3x3 operator * ( const float3x3& a )
	{
	return float3x3 (
	Dot( X, float3( a.Row[0].cell[0], a.Row[1].cell[0], a.Row[2].cell[0] ) ),
	Dot( X, float3( a.Row[0].cell[1], a.Row[1].cell[1], a.Row[2].cell[1] ) ),
	Dot( X, float3( a.Row[0].cell[2], a.Row[1].cell[2], a.Row[2].cell[2] ) ),
	Dot( Y, float3( a.Row[0].cell[0], a.Row[1].cell[0], a.Row[2].cell[0] ) ),
	Dot( Y, float3( a.Row[0].cell[1], a.Row[1].cell[1], a.Row[2].cell[1] ) ),
	Dot( Y, float3( a.Row[0].cell[2], a.Row[1].cell[2], a.Row[2].cell[2] ) ),
	Dot( Z, float3( a.Row[0].cell[0], a.Row[1].cell[0], a.Row[2].cell[0] ) ),
	Dot( Z, float3( a.Row[0].cell[1], a.Row[1].cell[1], a.Row[2].cell[1] ) ),
	Dot( Z, float3( a.Row[0].cell[2], a.Row[1].cell[2], a.Row[2].cell[2] ) )
	);
	}
	float3 operator * (const float3& a)
	{
	return float3(
	Dot( a, float3( Row[0].cell[0], Row[1].cell[0], Row[2].cell[0] ) ),
	Dot( a, float3( Row[0].cell[1], Row[1].cell[1], Row[2].cell[1] ) ),
	Dot( a, float3( Row[0].cell[2], Row[1].cell[2], Row[2].cell[2] ) )
	);
	}
	float3x3 operator * ( const float a )
	{
	float3x3 b;
	b = *this;
	b.X *= a;
	b.Y *= a;
	b.Z *= a;
	return b;
	}
	static float3x3 Rotate( float a )
	{
	float x = cosf( a );
	float y = sinf( a );
	return float3x3( x, y, 0, -y, x, 0, 0, 0, 1 );
	}
	float3x3 Transpose()
	{
	return float3x3( X.X, Y.X, Z.X, X.Y, Y.Y, Z.Y, X.Z, Y.Z, Z.Z );
	}
	static float3x3 Translate( float2 t )
	{
	return float3x3( 1, 0, 0, 0, 1, 0, t.X, t.Y, 1);
	}
	static float3x3 Scale( float2 s )
	{
	return float3x3( s.X, 0, 0, 0, s.Y, 0, 0, 0, 1 );
	}
	static float3x3 Scale( float s )
	{
	return float3x3( s, 0, 0, 0, s, 0, 0, 0, 1 );
	}
	static float3x3 Invert( float3x3 b )
	{
	float3x3 adjoint;
	adjoint.Row[0][0] = b.Row[1][1] * b.Row[2][2] - b.Row[1][2] * b.Row[2][1];
	adjoint.Row[0][1] = b.Row[0][2] * b.Row[2][1] - b.Row[0][1] * b.Row[2][2];
	adjoint.Row[0][2] = b.Row[0][1] * b.Row[1][2] - b.Row[0][2] * b.Row[1][1];
	adjoint.Row[1][0] = b.Row[1][2] * b.Row[2][0] - b.Row[1][0] * b.Row[2][2];
	adjoint.Row[1][1] = b.Row[0][0] * b.Row[2][2] - b.Row[0][2] * b.Row[2][0];
	adjoint.Row[1][2] = b.Row[0][2] * b.Row[1][0] - b.Row[0][0] * b.Row[1][2];
	adjoint.Row[2][0] = b.Row[1][0] * b.Row[2][1] - b.Row[1][1] * b.Row[2][0];
	adjoint.Row[2][1] = b.Row[0][1] * b.Row[2][0] - b.Row[0][0] * b.Row[2][1];
	adjoint.Row[2][2] = b.Row[0][0] * b.Row[1][1] - b.Row[0][1] * b.Row[1][0];
	float fDet = b.Row[0][0] * adjoint.Row[0][0] + b.Row[0][1] * adjoint.Row[1][0] + b.Row[0][2] * adjoint.Row[2][0];
	adjoint = adjoint * (1.0f / fDet);
	return adjoint;
	}
	};
	*/
	class float4x4
	{
	public:
		union
		{
			struct { float4 Row[4]; };
			struct { float4 X, Y, Z, W; };
		};
		float4x4()
		{
			X = float4( 1, 0, 0, 0 );
			Y = float4( 0, 1, 0, 0 );
			Z = float4( 0, 0, 1, 0 );
			W = float4( 0, 0, 0, 1 );
		}
		float4x4( float4 x, float4 y, float4 z, float4 w ) { X = x; Y = y; Z = z; W = w; }
		float4x4( float xx, float xy, float xz, float xw,
			float yx, float yy, float yz, float yw,
			float zx, float zy, float zz, float zw,
			float wx, float wy, float wz, float ww )
		{
			X = float4( xx, xy, xz, xw );
			Y = float4( yx, yy, yz, yw );
			Z = float4( zx, zy, zz, zw );
			W = float4( wx, wy, wz, ww );
		}
		float4x4( const Quaternion&_Q)
		{
			X = float4(1.0f - 2.0f * ( _Q.y * _Q.y + _Q.z * _Q.z ), 2.0f * ( _Q.x * _Q.y - _Q.w * _Q.z ), 2.0f * ( _Q.x * _Q.z + _Q.w * _Q.y ), 0.0f);
			Y = float4(2.0f * ( _Q.x * _Q.y + _Q.w * _Q.z ), 1.0f - 2.0f * ( _Q.x * _Q.x + _Q.z * _Q.z ), 2.0f * ( _Q.y * _Q.z - _Q.w * _Q.x ), 0.0f);
			Z = float4(2.0f * ( _Q.x * _Q.z - _Q.w * _Q.y ), 2.0f * ( _Q.y * _Q.z + _Q.w * _Q.x ), 1.0f - 2.0f * ( _Q.x * _Q.x + _Q.y * _Q.y ), 0.0f);
			W = float4(0, 0, 0, 1);
		}

		bool operator == ( const float4x4& a ) { return a.X == X && a.Y == Y && a.Z == Z && a.W == W; }
		bool operator != ( const float4x4& a ) { return a.X != X || a.Y != Y || a.Z != Z || a.W != W; }

		INLINE float4  operator[] ( int i ) const	{ return Row[i]; }
		INLINE float4& operator[] ( int i ) 		{ return Row[i]; }

		void operator *= ( const float4x4 b )
		{
			float4x4 a = *this;
			*this = a * b;
		}

		INLINE float4x4 operator * ( const float4x4& a ) const
		{
			float4x4 b = *this;
			return float4x4(
				Dot( b.X, float4( a.Row[0][0], a.Row[1][0], a.Row[2][0], a.Row[3][0] ) ),
				Dot( b.X, float4( a.Row[0][1], a.Row[1][1], a.Row[2][1], a.Row[3][1] ) ),
				Dot( b.X, float4( a.Row[0][2], a.Row[1][2], a.Row[2][2], a.Row[3][2] ) ),
				Dot( b.X, float4( a.Row[0][3], a.Row[1][3], a.Row[2][3], a.Row[3][3] ) ),

				Dot( b.Y, float4( a.Row[0][0], a.Row[1][0], a.Row[2][0], a.Row[3][0] ) ),
				Dot( b.Y, float4( a.Row[0][1], a.Row[1][1], a.Row[2][1], a.Row[3][1] ) ),
				Dot( b.Y, float4( a.Row[0][2], a.Row[1][2], a.Row[2][2], a.Row[3][2] ) ),
				Dot( b.Y, float4( a.Row[0][3], a.Row[1][3], a.Row[2][3], a.Row[3][3] ) ),

				Dot( b.Z, float4( a.Row[0][0], a.Row[1][0], a.Row[2][0], a.Row[3][0] ) ),
				Dot( b.Z, float4( a.Row[0][1], a.Row[1][1], a.Row[2][1], a.Row[3][1] ) ),
				Dot( b.Z, float4( a.Row[0][2], a.Row[1][2], a.Row[2][2], a.Row[3][2] ) ),
				Dot( b.Z, float4( a.Row[0][3], a.Row[1][3], a.Row[2][3], a.Row[3][3] ) ),

				Dot( b.W, float4( a.Row[0][0], a.Row[1][0], a.Row[2][0], a.Row[3][0] ) ),
				Dot( b.W, float4( a.Row[0][1], a.Row[1][1], a.Row[2][1], a.Row[3][1] ) ),
				Dot( b.W, float4( a.Row[0][2], a.Row[1][2], a.Row[2][2], a.Row[3][2] ) ),
				Dot( b.W, float4( a.Row[0][3], a.Row[1][3], a.Row[2][3], a.Row[3][3] ) )
				);
		}
		INLINE float4 operator * ( const float4& a ) const
		{
			return float4(
				Dot( a, float4( Row[0][0], Row[1][0], Row[2][0], Row[3][0] ) ),
				Dot( a, float4( Row[0][1], Row[1][1], Row[2][1], Row[3][1] ) ),
				Dot( a, float4( Row[0][2], Row[1][2], Row[2][2], Row[3][2] ) ),
				Dot( a, float4( Row[0][3], Row[1][3], Row[2][3], Row[3][3] ) )
				);
		}
		INLINE float4x4 operator * ( const float a ) const
		{
			float4x4 b;
			b = *this;
			b.X *= a;
			b.Y *= a;
			b.Z *= a;
			b.W *= a;
			return b;
		}

		static float4x4 LookAt(const float3 _From, const float3 _To, const float3 _Up)
		{
			float3 forward = Normalize(_To-_From);
			float3 side = Normalize(Cross(forward,_Up));
			float3 up = Normalize(Cross(side,forward));
			float4x4 mat=float4x4::Identity();
			mat.X.XYZ = -side;		//not sure why i need a negation
			mat.Y.XYZ = up;
			mat.Z.XYZ = forward;
			mat.W.XYZ = _From;
			return mat;
		}

		static float4x4 Lerp(const float4x4&a, const float4x4&b, float t)
		{
			float t2 = 1.0f - t;
			float4x4 r;
			for(int i = 0; i < 4; i++)
				r.Row[i] = a.Row[i] * t2 + b.Row[i] * t;
			return r;
		}

		static float4x4 RotateX( float a )
		{
			float x = cosf( a );
			float y = sinf( a );
			return float4x4(1, 0, 0, 0,	0, x, y, 0,	0, -y, x, 0, 0, 0, 0, 1 );
		}
		static float4x4 RotateY( float a )
		{
			float x = cosf( a );
			float y = sinf( a );
			return float4x4( x, 0, -y, 0, 0, 1, 0, 0, y, 0, x, 0, 0, 0, 0, 1 );
		}
		static float4x4 RotateZ( float a )
		{
			float x = cosf( a );
			float y = sinf( a );
			return float4x4( x, y, 0, 0, -y, x, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 );
		}
		static float4x4 Rotate( float3 _XYZ )
		{
			return RotateX(_XYZ.X)*RotateY(_XYZ.Y)*RotateZ(_XYZ.Z);
		}

		float4x4 Transpose()
		{
			return float4x4( X.X, Y.X, Z.X, W.X, X.Y, Y.Y, Z.Y, W.Y, X.Z, Y.Z, Z.Z, W.Z, X.W, Y.W, Z.W, W.W );
		}

		static const float4x4 Identity()
		{
			return float4x4( 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
		}

		static float4x4 Translate( float3 t )
		{
			return float4x4( 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, t.X, t.Y, t.Z, 1 );
		}

		void OrthoNormalize()
		{
			float4x4 n;
			n.X.XYZ = Normalize( Cross( Y.XYZ, Z.XYZ ) );
			n.Y.XYZ = Normalize( Cross( Z.XYZ, X.XYZ ) );
			n.Z.XYZ = Normalize( Cross( X.XYZ, Y.XYZ ) );
			n.W = W;
			*this = n;
		}

		static float4x4 Scale( float3 s )
		{
			return float4x4( s.X, 0, 0, 0, 0, s.Y, 0, 0, 0, 0, s.Z, 0, 0, 0, 0, 1 );
		}
		static float4x4 Scale( float s )
		{
			return float4x4( s, 0, 0, 0, 0, s, 0, 0, 0, 0, s, 0, 0, 0, 0, 1 );
		}

		float4x4 Inverse()
		{
			float m00 = Row[0][0], m01 = Row[0][1], m02 = Row[0][2], m03 = Row[0][3];
			float m10 = Row[1][0], m11 = Row[1][1], m12 = Row[1][2], m13 = Row[1][3];
			float m20 = Row[2][0], m21 = Row[2][1], m22 = Row[2][2], m23 = Row[2][3];
			float m30 = Row[3][0], m31 = Row[3][1], m32 = Row[3][2], m33 = Row[3][3];

			float v0 = m20 * m31 - m21 * m30;
			float v1 = m20 * m32 - m22 * m30;
			float v2 = m20 * m33 - m23 * m30;
			float v3 = m21 * m32 - m22 * m31;
			float v4 = m21 * m33 - m23 * m31;
			float v5 = m22 * m33 - m23 * m32;

			float t00 = + (v5 * m11 - v4 * m12 + v3 * m13);
			float t10 = - (v5 * m10 - v2 * m12 + v1 * m13);
			float t20 = + (v4 * m10 - v2 * m11 + v0 * m13);
			float t30 = - (v3 * m10 - v1 * m11 + v0 * m12);

			float invDet = 1 / (t00 * m00 + t10 * m01 + t20 * m02 + t30 * m03);

			float d00 = t00 * invDet;
			float d10 = t10 * invDet;
			float d20 = t20 * invDet;
			float d30 = t30 * invDet;

			float d01 = - (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
			float d11 = + (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
			float d21 = - (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
			float d31 = + (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

			v0 = m10 * m31 - m11 * m30;
			v1 = m10 * m32 - m12 * m30;
			v2 = m10 * m33 - m13 * m30;
			v3 = m11 * m32 - m12 * m31;
			v4 = m11 * m33 - m13 * m31;
			v5 = m12 * m33 - m13 * m32;

			float d02 = + (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
			float d12 = - (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
			float d22 = + (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
			float d32 = - (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

			v0 = m21 * m10 - m20 * m11;
			v1 = m22 * m10 - m20 * m12;
			v2 = m23 * m10 - m20 * m13;
			v3 = m22 * m11 - m21 * m12;
			v4 = m23 * m11 - m21 * m13;
			v5 = m23 * m12 - m22 * m13;

			float d03 = - (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
			float d13 = + (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
			float d23 = - (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
			float d33 = + (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

			return float4x4(
				d00, d01, d02, d03,
				d10, d11, d12, d13,
				d20, d21, d22, d23,
				d30, d31, d32, d33);
		}
	};
};

template<class T>class List;
template<class T>class LNode
{
	friend List<T>;
	LNode<T>*prev,*next;
public:
	T data;
	LNode<T>*GetNext(){return next;}
	LNode<T>*GetPrev(){return prev;}
};

template<class T>class List
{
	LNode<T>*head, *tail;
	int size;
public:
	List(): size( 0 ), head( 0 ), tail( 0 ) {}

	LNode<T>* Add(T _Object)
	{
		LNode<T>* node = new LNode<T>();
		node->data = _Object;
		if(head && tail)
			tail->next = node, node->prev = tail, tail = node;
		else
			head = tail = node,	node->prev = node->next = 0;
		size++;
		return node;
	}

	LNode<T>*Get(int _Idx)
	{
		int idx = 0;
		LNode<T>*n = head;
		while(n)
		{
			if(idx == _Idx) return n;
			n=n->next;
			idx++;
		}
		return 0;
	}

	void Remove(LNode<T>*_Object)
	{
		if(_Object->prev) _Object->prev->next = _Object->next; else head = _Object->next;
		if(_Object->next) _Object->next->prev = _Object->prev; else tail = _Object->prev;
		size--;
		delete _Object;
	}

	void Remove(T _Object)
	{
		Remove(Find(_Object));
	}

	LNode<T>* Find(T _Object)
	{
		LNode<T>*n = head;
		while(n)
		{
			if(n->data == _Object) return n;
			n=n->next;
		}
		return 0;
	}

	int GetSize() { return size; }

	LNode<T>*GetFirst() { return head; }
	LNode<T>*GetLast() { return tail; }

	void operator += ( T a ) { Add(a); }

	void operator -= ( LNode<T>*a ) { Remove(a); }
	void operator -= ( T a ) { Remove(a); }	
};
