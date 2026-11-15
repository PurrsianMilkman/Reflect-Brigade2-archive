#include"Window.h"

#include "windows.h"
#include "windowsx.h"
//#include "Graphics.h"
#include "bitmap.h"

// #include "resource.h"

LRESULT Window::WindowProcCallback( HWND _Hwnd, UINT _Message, WPARAM _Wparam, LPARAM _Lparam )
{
	Window* win = windows[_Hwnd];
	LRESULT result = 0;
	switch (_Message)
	{
	case WM_KEYDOWN:
		if (win != 0)
		{
			win->SetKey( (int)_Wparam, true );
		}
		break;
	case WM_KEYUP:
		if (win != 0)
		{
			win->SetKey( (int)_Wparam, false );
		}
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
		if (win!=0)
		{
			win->Mouse( GET_X_LPARAM( _Lparam ), GET_Y_LPARAM( _Lparam ), (_Wparam & MK_LBUTTON) != 0, (_Wparam & MK_MBUTTON) != 0, (_Wparam & MK_RBUTTON) != 0);
		}
		break;
	case WM_SIZE:
		if (win!=0)
		{
			win->Resized( int2( LOWORD( _Lparam ), HIWORD( _Lparam ) ) );
		}
		break;
	default:
		result=DefWindowProc( _Hwnd, _Message, _Wparam, _Lparam );
		break;
	}
	return result;
}

WindowedWindow::WindowedWindow( const int2& _Size, char* _ClassName, bool _ShowBorder )
{
	MousePos = int2(0,0);
	for( int i = 0; i < 255; i++ ) Key[i] = false;
	LMB = MMB = RMB = 0;
	WindowSize = _Size;
	RECT WindowRect;
	unsigned int PixelFormat;

	WindowRect.left = (long)0;
	WindowRect.right = (long)_Size.X;
	WindowRect.top = (long)0;
	WindowRect.bottom = (long)_Size.Y;
	HINSTANCE instance = GetModuleHandle( NULL );

	dwExStyle = WS_EX_APPWINDOW;
	DWORD style=_ShowBorder ?
		WS_OVERLAPPEDWINDOW|WS_VISIBLE|WS_POPUP
		: WS_VISIBLE|WS_POPUP;

	wc.style = CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC)Window::WindowProcCallback;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = instance;
	// wc.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hIcon = 0;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = TEXT( _ClassName );
	
	if (!RegisterClass(& wc))
	{
		MessageBox( NULL, "Failed To Register The Window Class.", "ERROR", MB_OK | MB_ICONEXCLAMATION );
		return;
	}
	AdjustWindowRectEx( &WindowRect, WS_POPUP, false, dwExStyle );

	if (!(HWnd = CreateWindow( _ClassName, _ClassName, style, 0, 0, _Size.X, _Size.Y, NULL, NULL, instance, this ) ) )
	{
		MessageBox( NULL, "Window Creation Error.", "ERROR", MB_OK | MB_ICONEXCLAMATION );
		return;
	}
	//add the window to the registered windows list
	windows[HWnd] = this;
	wc.lpfnWndProc = 0;
	int bitdepth = 32;
	PIXELFORMATDESCRIPTOR pfd = { sizeof( PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL, PFD_TYPE_RGBA, bitdepth,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0 };
	if (!(hdc = GetDC(HWnd)))
	{
		MessageBox( NULL, "Can't Create A GL Device Context.", "ERROR", MB_OK | MB_ICONEXCLAMATION );
		return;
	}
	if (!(PixelFormat = ChoosePixelFormat( hdc, & pfd ))) 
	{
		//creates a bunch of threads for some reason
		MessageBox( NULL, "Can't Find A Suitable PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION );
		return;
	}
	if (!SetPixelFormat( hdc, PixelFormat, & pfd))
	{
		MessageBox( NULL, "Can't Set The PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION );
		return;
	}
	if (!(m_HRC = wglCreateContext(hdc)))
	{
		MessageBox( NULL, "Can't Create A GL Rendering Context.", "ERROR", MB_OK | MB_ICONEXCLAMATION );
		return;
	}
	if (!wglMakeCurrent( hdc, m_HRC ))
	{
		MessageBox( NULL, "Can't Activate The GL Rendering Context.", "ERROR", MB_OK | MB_ICONEXCLAMATION );
		return;
	}
	ResizeClient( _Size );
	UpdateWindow();
}

WindowedWindow::~WindowedWindow()
{
	DestroyWindow( HWnd );
}