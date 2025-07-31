#ifndef RVL_SDK_MEM_HEAP_COMMON_H
#define RVL_SDK_MEM_HEAP_COMMON_H

#include <revolution/mem/list.h>

#include <context_rvl.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
	MEM_HEAP_FILL_NO_USE,
	MEM_HEAP_FILL_ALLOC,
	MEM_HEAP_FILL_FREE,
	MEM_HEAP_FILL_MAX,
} MEMHeapFill;

typedef struct MEMiHeapHead
{
	u32 signature;
	MEMLink link;
	MEMList childList;
	void *heapStart;
	void *heapEnd;
	OSMutex mutex;
	union
	{
		u32 val;
		struct
		{
			u32 _reserved : 24;
			u32 optFlag : 8;
		} fields;
	} attribute;
} MEMiHeapHead;

typedef MEMiHeapHead MEMHeapHandle;

void MEMiInitHeapHead(MEMiHeapHead *pHeapHd, u32 signature, void *heapStart,
                      void *heapEnd, u16 opt);
void MEMiFinalizeHeap(MEMiHeapHead *pHeapHd);
void MEMiDumpHeapHead(MEMHeapHandle *pHeapHd);
MEMHeapHandle *MEMFindContainHeap(void const *block);
MEMHeapHandle *MEMFindParentHeap(MEMHeapHandle *heap);

static inline void *MEMGetHeapEndAddress(MEMHeapHandle *heap)
{
	return heap->heapEnd;
}

#ifndef NDEBUG
void MEMDumpHeap(MEMHeapHandle *heap);
u32 MEMSetFillValForHeap(MEMHeapFill type, u32 fillVal);
u32 MEMGetFillValForHeap(MEMHeapFill type);
#endif

#ifdef __cplusplus
}
#endif

#endif
