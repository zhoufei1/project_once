#include "Malloc.h"

int main()
{
	Malloc<int> t;
	int *p=t.MallocAllocate(12);
	return 0;
}