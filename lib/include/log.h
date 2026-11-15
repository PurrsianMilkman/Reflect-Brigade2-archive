#pragma once

#pragma warning(disable:4996)
// #define LOG( logger, format, __VA_ARGS__ ) logger( __LINE__, __FILE__, format, __VA_ARGS__ );
/// Log
namespace Log
{
	namespace OutputModes
	{
		const int Console = 0x1;
		const int File = 0x2;
	};
	/// Console
	namespace Console
	{
		extern const int ForegroundBlue;
		extern const int ForegroundGreen;
		extern const int ForegroundRed;
		extern const int ForegroundIntensity;

		extern const int BackgroundBlue;
		extern const int BackgroundGreen;
		extern const int BackgroundRed;
		extern const int BackgroundIntensity;

		extern int DefaultColor;
		/// change the console foreground color
		void SetColor( int _Color );
		/// change the console windows title
		void SetTitle( char* _Title );
		/// clear the console window
		void Clear();
	};
	extern void* OutputFile;
	/// Logger
	class Logger
	{
	public:
		char* header;
		int consolecolor;
		bool showfilename;
		int outputmode;
		Logger( char* _Header, int _OutputMode, int _ConsoleColor, bool _ShowFilename )
		{
			header = _Header;
			outputmode = _OutputMode;
			consolecolor = _ConsoleColor;
			showfilename = _ShowFilename;
		}
		/// Log a message
		void operator()( char* _Format, ... );
		void operator()( int _Line, char* _Filename, char* _Format, ... );
		void SetOutputMode( int _OutputMode ) { outputmode = _OutputMode; }
		void SetConsoleColor( int _ConsoleColor ) { consolecolor = _ConsoleColor; }
	};
	// some common loggers
	/**
	used to log error messages
	*/
	extern Logger Error;
	/**
	used to log warning messages
	*/
	extern Logger Warning;
	/**
	used to log normal messages
	*/
	extern Logger Message;
};
