#pragma once
/// Timer
class Timer
{
public:
	// ctor / dtor
	Timer();
	// other methods
	/// start timing
	void Start();
	/// stop timing
	void Stop();
	/// get the time elapsed in microseconds
	int ElapsedUs();
	/// get the time elapsed in milliseconds
	int ElapsedMs();
	// data members
private:
	static __int64 frequency;
	__int64 begin,end;
};