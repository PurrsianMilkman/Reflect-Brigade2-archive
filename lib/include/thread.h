#pragma once
#define MAXJOBTHREADS	32
#define MAXJOBS			512

class Thread 
{
	// ctor / dtor
public:
	Thread() { m_hThread = 0; }
	// data access
	unsigned long* handle() { return m_hThread; }
	// other methods
	void start();
	virtual void run() {};
	void sleep(long ms);
	void suspend();
	void resume();
	void kill();
	void stop();
	void setPriority(int p);
	void SetName( char* _Name );
	
	// data members
private:
	unsigned long* m_hThread;
	static const int P_ABOVE_NORMAL;
	static const int P_BELOW_NORMAL;
	static const int P_HIGHEST;
	static const int P_IDLE;
	static const int P_LOWEST;
	static const int P_NORMAL;
	static const int P_CRITICAL;
};
extern "C" { unsigned int sthread_proc( void* param ); }