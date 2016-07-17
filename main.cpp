#include "Malloc.h"

//#define n 1000000000000 不能分配成功   1T
//#define n 100000000000   这是能分配成功的极限，即100G
#define n 10
int main()
{
	Malloc<int> t;
	//int *p=t.MallocAllocate(12);
	//t.MallocDeAllocate(p);
	int *q=t.MallocAllocate(n);
	t.MallocDeAllocate(q,n);
	return 0;
}