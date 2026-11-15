#pragma once

//make a nice string class out of me

class string
{
public:
	// ctor / dtor
	string() { data = 0; }
	string( int _Length );
	string( char* _Source );
	string( const string& str);
	string& string::operator= ( string const& f );
	string operator + (string const&s);
	~string()
	{
		delete data;
		data = 0;
		//Log::Message("whut?");	// TODO FIXME
	}
	int Length();
	// data members
	char* data;
	int length;
};

bool match( char*_str1, char*_str2 );
