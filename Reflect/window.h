#pragma once

#include <map>
#include "windows.h"
class Window;
static std::map<HWND,Window*> windows;
#include "vector.h"
using namespace Math;

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

#include <gl\gl.h>
#include <gl\glu.h>

class Window
{	
public:
	HWND HWnd;
	static LRESULT WindowProcCallback(HWND _Hwnd, UINT _Message, WPARAM _Wparam, LPARAM _Lparam);
	virtual void Mouse(int _X,int _Y,bool _LMB,bool _MMB,bool _RMB)=0;
	virtual void SetKey(int _key,bool _State)=0;
	virtual int2 GetWindowSize()=0;
	virtual int2 GetClientSize()=0;
	virtual void Resized(int2 _Size)=0;
	virtual void Begin()=0;
	virtual void End()=0;
	virtual bool UpdateWindow()=0;//returns false if the window is closed
	virtual HWND GetHWND(){return HWnd;}
};

class WindowedWindow : public Window
{
private:
	WNDCLASS wc;
	DWORD dwExStyle;

	HGLRC m_HRC;
	HDC hdc;
public:
	int2 WindowSize;
	int2 ClientSize;
	int2 MousePos;
	bool LMB,MMB,RMB;
	bool Key[256];
	WindowedWindow(const int2&_Size,char* _ClassName,bool _ShowBorder=true);
	~WindowedWindow();

	void SetKey(int _key,bool _State)
	{
		Key[_key] = _State;
	}

	void Mouse(int _X,int _Y,bool _LMB,bool _MMB,bool _RMB)
	{
		MousePos=int2(_X,_Y);
		LMB=_LMB;
		MMB=_MMB;
		RMB=_RMB;
	}

	void Resized(int2 _WindowSize)
	{
		WindowSize=_WindowSize;
		RECT rcClient;
		GetClientRect(HWnd, &rcClient);
		ClientSize.X = rcClient.right - rcClient.left;
		ClientSize.Y = rcClient.bottom- rcClient.top;
	}

	void ResizeClient(int2 _Size)
	{
		RECT rcClient, rcWind;
		POINT ptDiff;
		GetClientRect(HWnd, &rcClient);
		GetWindowRect(HWnd, &rcWind);
		ptDiff.x = (rcWind.right - rcWind.left) - rcClient.right;
		ptDiff.y = (rcWind.bottom - rcWind.top) - rcClient.bottom;
		MoveWindow(HWnd,rcWind.left, rcWind.top, _Size.X + ptDiff.x, _Size.Y + ptDiff.y, TRUE);
	}

	int2 GetPosition()
	{	
		RECT rcWind;
		GetWindowRect(HWnd, &rcWind);
		return int2( rcWind.left,rcWind.top);
	}

	void ResizeWindow(int2 _Size)
	{
		RECT rcWind;
		GetWindowRect(HWnd, &rcWind);
		MoveWindow(HWnd,rcWind.left, rcWind.top, _Size.X, _Size.Y, TRUE);
	}

	void Begin()
	{
		//wglMakeCurrent(hdc,m_HRC);
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);//ahm yea ?
		glViewport(0,0,ClientSize.X,ClientSize.Y);
	}

	void End()
	{
		glFlush();
	}

	bool UpdateWindow()//returns false if the window is closed
	{
		//handle windows messages
		SwapBuffers(hdc);
		glFinish();
		MSG	msg;
		while (PeekMessage(&msg,HWnd,0,0,PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		return true;
	}

	int2 GetWindowSize()
	{
		return WindowSize;
	}

	void SetPosition(int2 _Position, bool _Center = false)
	{
		RECT rcWind;
		GetWindowRect(HWnd, &rcWind);
		int w = rcWind.right-rcWind.left;
		int h = rcWind.bottom-rcWind.top;
		if(_Center)
		{
			_Position.X-=w/2;
			_Position.Y-=h/2;
		}
		MoveWindow(HWnd,_Position.X, _Position.Y, w, h, TRUE);
	}

	int2 GetClientSize(){return ClientSize;}
};
