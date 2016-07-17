#include "allocator.h"

typedef malloc_alloc<0> _Malloc_alloc;
typedef DefaultAllocTemplate<1,0> DefaultAlloc;

//template<class T,class Malloc_alloc=_Malloc_alloc>
template<class T,class Malloc_alloc=DefaultAlloc>
class Malloc
{
public:
	Malloc()
	{}
	~Malloc()
	{}
public:

	static T* MallocAllocate(size_t n)
	{
		return 0==n ? 0 :(T*)DefaultAlloc::Allocate(n);
		//return 0==n ? 0 :(T*)Malloc_alloc::allocate(n);
	}
	static T* MallocAllocate()
	{
		return (T*)DefaultAlloc::Allocate(sizeof(T));
		//return (T*)Malloc_alloc::allocate(sizeof(T));
	}
	static void MallocDeAllocate(T *p)
	{
		DefaultAlloc::DeAllocate(p,sizeof(T));
		//Malloc_alloc::dellocate(p,sizeof(T));
	}
	static void MallocDeAllocate(T *p,size_t n)
	{
		DefaultAlloc::DeAllocate(p,n);
		//Malloc_alloc::dellocate(p,n*sizeof(T));
	}
};