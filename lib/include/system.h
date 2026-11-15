#pragma once
#include "text.h"

namespace System
{
	int GetCPUCount();
	string GetCPUName();
	void CopyFileDate( char* _SrcFile, char* _DstFile );
	bool CompareFileDate( char* _FileA, char* _FileB );
	bool FileExists( char* _File );
	string GetDirectory( char* _FileName );
	bool FileDelete( char* _File );
	string GetFileName( char* _FileName );
	string GetFileNameWithExtension( char* _FileName );
	string RemoveExtension(string _Name);
};
