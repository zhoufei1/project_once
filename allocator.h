#pragma once

#include <iostream>
#include <string>
using namespace std;

#include <stdarg.h>//打trace log函数


/////////////////////////////////////////////////////////////////////
//打印trace，跟踪内存分配情况
#define _DEBUG_
static string GetFileName(const string &path)
{
	char ch='/';
#ifdef _WIN32
	ch='\\';
#endif
	size_t pos=path.rfind(ch);
	if(pos==string::npos)
		return path;
	else
		return path.substr(pos+1);
}
inline void __trace_bug(const char *Function,const char *FileName,int line,char *format,...)
{
#ifdef _DEBUG_
	fprintf(stdout," 【 %s  %d】: %s",GetFileName(FileName).c_str(),line,Function);
	va_list args;
	va_start(args,format);
	vfprintf(stdout,format,args);
	va_end(args);
#endif
}
#define __TRACE_DEBUG(...)\
	__trace_bug(__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__);
//__FUNCTION__：当前函数
//__FILE__    ：所在文件
//__LINE__    ：所在文件行号
//__VA_ARGS__ :是一个可变参数的宏
/////////////////////////////////////////////////////////////////////

//打印出没有内存的情况
static inline void print_log()
{
	cerr<<"out of memory"<<endl;
	exit(1);
}

//一级空间配置器
template<int inst>
class malloc_alloc
{
private:
	//主要是为了分配内存失败后不断的进行内存分配。直到物理内存没有为止
	//因为要涉及到对内存中一些暂时不用的内存进行释放，这里面有会涉及到操作系统的调度算法
	//所以这个函数没有被具体实现，如果申请内存失败就直接退出程序
	static void* oom_malloc(size_t n); 
	//主要是对内存空间大小重新分配，同理也没有实现
	static void* oom_realloc(void* ,size_t n);
	//主要是设置句柄，作用是当内存中没有足够分配的内存时调用，同理也没有被实现
	static void* (*__malloc_alloc_oom_handler)();
public:
	//分配内存
	static void* allocate(size_t n)
	{
		__TRACE_DEBUG("分配内存块对象：%d bit\n", n);
		void *result=(void*)malloc(n);
		if(0 == result)
			print_log();
		return result;
	}
	//释放内存空间
	static void dellocate(void *p,size_t)
	{
		free(p);
		//防止野指针
		p=NULL;
	}
	//重新分配内存
	static void* reallocate(void *p,size_t n)
	{
		__TRACE_DEBUG("重新分配内存块对象：%d bit\n", n);
		void *result=realloc(p,n);
		if(0 == result)
			print_log();
		return result;
	}
};


//二级空间配置器
template<bool Threads,int inst>
class DefaultAllocTemplate
{
private:
	enum {ALIGN=8};      //小型区块的上调边界
	enum {MAX_BYTES=128};//小型区块的上限
	enum {NFREELISTS=MAX_BYTES/ALIGN};//自由链表的个数,这块只需要16个数组大小就满足
private:
	//上调分配的字节数到8的倍数
	static size_t ROUND_UP(size_t bytes)
	{
		//假如分配5个字节--》(5+8-1) & ~(8-1)
		return ( ((bytes)+ALIGN-1) & ~(ALIGN-1) );
	}
	//寻找到n个自由链表的下表，从0~n开始
	static size_t FREELIST_INDEX(size_t bytes)
	{
		//假如寻找的字节数大小是35bytes,即 (35+8-1)/8 -1=4
		return ((bytes + ALIGN-1)/ALIGN -1);
	}
private:
	//自由链表
	union obj
	{
		union obj *FreeListLink;
		char ClientData[1];
	};
private:
	//内存池的结构
	static char *StartFree;
	static char *EndFree;
	static size_t HeapSize;
private:
	//自由链表数组，大小为16
	static obj* volatile FreeList[NFREELISTS];
	//返回一个大小为n的对象，并可能加入大小为n的其他区块到自由链表中
	static void* ReFill(size_t n);
	//配置一大块空间，可容纳大小为size的区块
	static char* ChunkAlloc(size_t size,size_t &nobjs);
public:
	//分配内存
	static void* Allocate(size_t n);
	//释放内存
	static void DeAllocate(void *p,size_t);
	//重新分配内存
	static void* ReAllocate(void *p,size_t old_size,size_t new_size);
};

template<bool Threads,int inst>
char* DefaultAllocTemplate<Threads,inst>::StartFree=0;

template<bool Threads,int inst>
char* DefaultAllocTemplate<Threads,inst>::EndFree=0;

template<bool Threads,int inst>
size_t DefaultAllocTemplate<Threads,inst>::HeapSize=0;

template<bool Threads,int inst>
typename DefaultAllocTemplate<Threads,inst>::obj* volatile
DefaultAllocTemplate<Threads,inst>::FreeList[NFREELISTS]={0};


template<bool Threads,int inst>
void* DefaultAllocTemplate<Threads,inst>::Allocate(size_t n)
{
	__TRACE_DEBUG("分配内存块对象：%d bit\n", n);
	//如果分配内存大于128字节，则调用一级空间配置器
	if(n > (size_t)MAX_BYTES)
		return malloc_alloc<0>::allocate(n);

	void *result=0;//需要返回的申请的内存
	//先寻找字节所在的位置
	size_t index=FREELIST_INDEX(n);

	if(FreeList[index]==0)//自由链表中还没有内存时
		result=ReFill(ROUND_UP(n));
	else
	{
		//取出链表的头返回
		result=FreeList[index];
		//将链表的头指向链表的下一个节点
		FreeList[index]=((obj*)result)->FreeListLink;
	}
	return result;
}

template<bool Threads,int inst>
void DefaultAllocTemplate<Threads,inst>::DeAllocate(void *p,size_t n)
{
	__TRACE_DEBUG("释放内存块：%p  %d bit\n",p,n);
	//如果大于128，让一级空间配置器回收
	if(n > (size_t)MAX_BYTES)
		malloc_alloc<0>::dellocate(p,n);
	else
	{
		//让不用的内存块连到自由链表中去
		size_t index=FREELIST_INDEX(n);
		((obj*)p)->FreeListLink=FreeList[index];
		FreeList[index]=(obj*)p;
	}
}

//重新分配内存
template<bool Threads,int inst>
void* DefaultAllocTemplate<Threads,inst>::ReAllocate(void *p,size_t old_size,size_t new_size)
{
	__TRACE_DEBUG("重新分配内存块对象：%d bit\n", n);
	//如果需要增加的内存比之前的内存大并且大于128时调用一级空间配置器
	if(old_size > (size_t)MAX_BYTES && new_size > (size_t)MAX_BYTES)
		return malloc_alloc<0>::reallocate(p,new_size);
	//如果上调后两个字节大小相等，则直接返回，出现这种情况的原因是按8的倍数对其
	if(ROUND_UP(old_size) == ROUND_UP(new_size))
		return p;
	void *result=0;
	size_t copy_size=0;
	//先申请空间大小
	result=Allocate(new_size);
	//计算出需要拷贝的字节数，这点得注意
	copy_size=new_size > old_size?old_size : new_size;
	memcpy(result,p,copy_size);
	//释放之前的内存
	DeAllocate(p,old_size);
	return result;
}

template<bool Threads,int inst>
char* DefaultAllocTemplate<Threads,inst>::ChunkAlloc(size_t size, size_t &nobjs)
{
	char *result=0;
	size_t total_bytes=size * nobjs; //总共的字节数
	size_t bytes_left=EndFree-StartFree;//内存池中还剩余的内存大小
	//如果剩余内存足够，直接分配
	if(bytes_left >=total_bytes)
	{
		result=StartFree;
		StartFree+=total_bytes;
		return result;
	}else if(bytes_left >=size)//虽然剩余内存不足，但满足分配size大小的
	{
		nobjs=bytes_left/size;//算出内存应该被分配到的数组下表
		total_bytes=size *nobjs;//总共的字节数

		//返回最自由链表的起始位置，并改变自由链表的起始位置
		result=StartFree;
		StartFree+=total_bytes;
		return result;
	}else
	{	
		//数组中还没有自由链表开辟的内存块时或者已经被分配完了时
		size_t bytes_to_get=2*total_bytes+ROUND_UP(HeapSize>>4);
		__TRACE_DEBUG("内存池没有空间，现在分配空间大小：%d bit\n", bytes_to_get);
		//试着看看还有没有残余可用的内存块
		if(bytes_left >0)
		{
			size_t index=FREELIST_INDEX(bytes_left);
			((obj*)StartFree)->FreeListLink=FreeList[index];
			FreeList[index]=(obj*)StartFree;
		}
		//开辟一块新的内存块
		StartFree=(char*)malloc(bytes_to_get);
		//如果开辟失败
		if(StartFree ==0)
		{
			//检查数组中还有没有可用的内存块
			for(size_t i=size; i<=MAX_BYTES; i+=ALIGN)
			{
				size_t index=FREELIST_INDEX(i);
				if(FreeList[index])
				{
					//表示找到了，然后改变自由链表的头和尾，然后递归的去分配内存
					obj *start=FreeList[index];
					StartFree=(char*)start;
					FreeList[index]=start->FreeListLink;
					EndFree=StartFree+i;
					return ChunkAlloc(size,nobjs);
				}
			}
			//如果没有找到，直接调用一级空间配置器去处理
			StartFree=(char*)malloc_alloc<0>::allocate(bytes_to_get);
		}
		//表示申请内存成功
		//改变自由链表的尾指针，然后递归分配内存
		EndFree=StartFree + bytes_to_get;
		HeapSize+=bytes_to_get;
		return ChunkAlloc(size,nobjs);
	}
}


template<bool Threads,int inst>
void* DefaultAllocTemplate<Threads,inst>::ReFill(size_t n)
{
	size_t nobjs=20;//这是SGI里系统设定的初始值
	char *chunk=ChunkAlloc(n,nobjs);//首先寻找内存块
	__TRACE_DEBUG("找到内存池分配：n: %d bit  nobjs : %d \n", n,nobjs);
	if(1==nobjs)
		return chunk;
	//寻找字节在数组中的下标
	size_t index=FREELIST_INDEX(n);
	obj *cur=(obj*)(chunk+n);
	FreeList[index]=cur;
	//将从系统里申请到的内存块链接到自由链表中
	for(size_t i=2; i<nobjs; ++i)
	{
		obj *next=(obj*)(chunk+n*i);
		cur->FreeListLink=next;
		cur=next;
	}
	//自由链表的最后赋为0
	cur->FreeListLink=0;
	return chunk;
}