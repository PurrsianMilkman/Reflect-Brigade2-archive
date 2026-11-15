#pragma once

#define BUFFERMAXDIRTY 64

#include "settings.h"

struct RemoteBufferUpdate
{
	int from,count;
};

template<typename T>class LocalBuffer
{
public:
	// ctor / dtor
	LocalBuffer() : ptr( 0 ), size( 0 ), allocatedsize(0), firstupdate( 0 ), lastupdate( 0 ) {}
	~LocalBuffer() { if (ptr) FREE( ptr ); }
	// access methods
	T* GetPtr() { return ptr; }
	int GetSize() { return size; }
	int GetFirstUpdate() { return firstupdate; }
	int GetLastUpdate() { return lastupdate; }
	// operators
	T& operator [] ( int a ) { return ptr[a]; }
	// other methods
	RemoteBufferUpdate& GetUpdate( int _UpdateID )
	{
		// bounds check ?
		return updates[_UpdateID - firstupdate];
	}
	
	void Alloc( int _ElementCount, bool _KeepData = true )
	{
		int oldsize = size;
		Resize(size + _ElementCount);
		allocatedsize = size;
		size = oldsize;
	}

	void Resize( int _NewElementCount, bool _KeepData = true )
	{
		if (allocatedsize < _NewElementCount)
		{
			T* newbuf = MALLOC( _NewElementCount, T );
			if(!newbuf) { Log::Error("Failed to allocated %iB", _NewElementCount); }
			if (ptr)
			{
				if ( _KeepData ) memcpy( newbuf, ptr, MIN( size, _NewElementCount ) * sizeof( T ) );
				FREE( ptr );
			}
			ptr = newbuf;
			size = _NewElementCount;
		}
	}
	void Update( int _From, int _Count )
	{
		//create update
#if 0
		for( int i = firstupdate; i < lastupdate;i++)
		{
			RemoteBufferUpdate& upd = GetUpdate( i );
			if (upd.from + upd.count > _From && upd.from < _From + _Count)
			{
				upd.from = MIN( upd.from, _From );
				upd.count = MAX( upd.count, _From + _Count - upd.from );
				return;
			}
		}
		GetUpdate( lastupdate ).from = _From;
		GetUpdate( lastupdate ).count = _Count;
		lastupdate++;
		if (lastupdate - firstupdate >= BUFFERMAXDIRTY) firstupdate = lastupdate; // intentionally invalidate
#else
		GetUpdate( lastupdate ).from = _From;
		GetUpdate( lastupdate ).count = _Count;
		lastupdate++;
		if (lastupdate - firstupdate >= BUFFERMAXDIRTY) firstupdate = lastupdate; //intentionally invalidate
#endif
	}

	void Optimize()
	{
		while(true)
		{
			bool changed = false;
			for( int i = firstupdate; i < lastupdate;i++)
			{
				for( int j = i + 1; j < lastupdate;j++)
				{
					RemoteBufferUpdate&a = GetUpdate(i);
					RemoteBufferUpdate&b = GetUpdate(j);
					if(a.count && b.count && a.from + a.count >= b.from && b.from >= a.from)
					{
						a.count = b.from + b.count - a.from;
						b.count = 0;
						b.from = 0;
						changed=true;
					}
				}
			}
			if(!changed) return;
		}
	}

	void Flush() { firstupdate = lastupdate; }
	// data members
public:
	T* ptr;
private:
	int firstupdate;
	int lastupdate;
	int size;
	int allocatedsize;
	RemoteBufferUpdate updates[BUFFERMAXDIRTY];
};

template<typename T>class RemoteBuffer
{
public:
	// ctor / dtor
	RemoteBuffer() : lastupdate( 0 ), localbuffer( 0 ) {}
	// access methods
	void SetLocalBuffer( LocalBuffer<T>* _LocalBuffer ) { localbuffer = _LocalBuffer; }
	// other methods
	virtual void Synchronize() = 0;
	void Invalidate() { lastupdate = 0; }
	virtual int GetSize() = 0;
	void ForceOutOfSync() { lastupdate =- 1; }
	// data members
protected:
	int lastupdate;
	LocalBuffer<T>* localbuffer;
};


//collection chunk size
//#define CSIZE 6 moved to settings.h
//chunk size calculator;
// n = 5
// 2^n = 32Chunks of 2^(32-n)/1024/1024 = 128MB
// n = 6
// 2^n = 64Chunks of 2^(32-n)/1024/1024 = 64 MB
class Collection
{
	uint AllocVAddr(int _Size);
public:
	struct CollectionUpdate
	{
		int chunk;
		int from, count;
	};
	class Element
	{
		friend Collection;
		uint vaddr;
	public:
		void*ptr;
		int size;//bytes
		
		Element*next;
		Element*prev;
		uint GetVAddr() { return vaddr; }
		int GetSize() { return size; }
		void* GetPtr() { return ptr; }
	};
	Collection():root( 0 ), tail( 0 ), firstupdate( 0 ), lastupdate( 0 ), ChunkCount( 0 ), highestaddr( 0 ){}
	~Collection();

	int GetFirstUpdate() { return firstupdate; }
	int GetLastUpdate() { return lastupdate; }
	int GetChunk (uint vaddr) { return vaddr >> (32 - CSIZE); }
	int GetOffset(uint vaddr) { return vaddr & ((1 << (32 - CSIZE)) - 1); }
public:
	void Defragment();
	Element* Alloc(int _Size);
	void Free(Element*_Element);

	void Update( const Element* _Element );
	void Flush() { firstupdate = lastupdate; }

	Element*& GetUpdate( int _UpdateID ) { return updates[_UpdateID - firstupdate]; }
	Element*GetFirst() { return root; }
public:
	Element*root;
	Element*tail;
	int ChunkCount;
private:
	int firstupdate;
	int lastupdate;
	Element* updates[BUFFERMAXDIRTY];
	int highestaddr;
};

#define COLLECTIONCHUNKSIZE 17 // chunksize = (1 << COLLECTIONCHUNKSIZE) * sizeof(element)
#define MAXCHUNKS ((1 << (32 - COLLECTIONCHUNKSIZE)) / sizeof(float4) / 4) // max 1GB
class Collection128
{
public:
	struct Element { uint vaddr; float4*ptr; };
	struct Update
	{
		int chunk;
		int from, count;
	};
	class Chunk
	{
		friend Collection128;
		Collection128*collection;
		float4*ptr;
		uint used; // right most element used		create new chunk if new element doesnt fit
		int elements; // amount of elements stored, delete chunk if zero
		bool dirty;
	public:
		bool IsDirty() { return dirty; }
		void Synchronize() { dirty = true; }
		bool Exists() { return ptr != 0; }
		Chunk():ptr(0), used(0), elements(0) {}
		~Chunk() { Destroy(); }
		void Create();
		float4*GetPtr() { return ptr; }
		Element Alloc( const int _Size );
		void Free(); // frees one element
		void Destroy();
	};
	Collection128() : updateindex( 0 )
	{
		for(int i=0;i<MAXCHUNKS;i++) chunks[i].collection = this;		
	}
	~Collection128(){}
	int GetUpdateIndex() { return updateindex; }
	int GetLastUpdateIndex() { return lastupdateindex; }
	Element Alloc( const int _Size );
	int GetChunkIdx (uint _vaddr) { return _vaddr >> COLLECTIONCHUNKSIZE; }
	Chunk*GetChunk(uint _vaddr) { return &chunks[GetChunkIdx(_vaddr)]; }
	Chunk*GetChunks() { return chunks; }
	int GetOffset(uint _vaddr) 
	{
		return _vaddr & ((1 << COLLECTIONCHUNKSIZE) - 1); 
	}
	// float4*GetPtr(uint vaddr) { &(chunks[GetChunk(vaddr)][GetOffset(vaddr)]); }
public:
	void Defragment();
	float4& operator [] ( int a ) { return GetChunk(a)->GetPtr()[GetOffset(a)]; }
	
	void Free(uint vaddr);
	void Synchronize(uint vaddr)
	{
		GetChunk(vaddr)->Synchronize();
		updateindex++;
	}
	void Synchronize(uint firstvaddr, int lastvaddr)
	{
		int chunk1 = GetChunkIdx(firstvaddr);
		int chunk2 = GetChunkIdx(lastvaddr );
		for(int i=chunk1;i<=chunk2;i++)chunks[i].Synchronize();
		updateindex++;
	}

	void Flush()
	{
		for(int i=0;i<MAXCHUNKS;i++) chunks[i].dirty = false;
		lastupdateindex = updateindex;
	}
private:
	Chunk chunks[MAXCHUNKS];
	int updateindex;
	int lastupdateindex;
};

class RemoteCollection128
{
public:
	// ctor / dtor
	RemoteCollection128() : updateindex( 0 ) {}
	// access methods
	void SetLocalBuffer( Collection128* _LocalBuffer ) { localbuffer = _LocalBuffer; }
	// other methods
	virtual void Synchronize() = 0;
	void Invalidate() { updateindex = -1; }
	// virtual int GetSize() = 0;
	// data members
protected:
	int updateindex;
	Collection128* localbuffer;
};

class RemoteCollection
{
public:
	// ctor / dtor
	RemoteCollection() : lastupdate( 0 ), localbuffer( 0 ) {}
	// access methods
	void SetLocalBuffer( Collection* _LocalBuffer ) { localbuffer = _LocalBuffer; }
	// other methods
	virtual void Synchronize() = 0;
	void Invalidate() { lastupdate = 0; }
	// virtual int GetSize() = 0;
	// data members
protected:
	int lastupdate;
	Collection* localbuffer;
};
