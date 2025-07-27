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
	MEM_HEAP_FILL_MAX = 3,
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

void MEMiInitHeapHead(MEMiHeapHead *heap, u32 signature, void *heapStart,
                      void *heapEnd, u16 opt);
void MEMiFinalizeHeap(MEMiHeapHead *pHeapHd);
void MEMiDumpHeapHead(MEMiHeapHead *heap);
MEMiHeapHead *MEMFindContainHeap(void const *block);
MEMiHeapHead *MEMFindParentHeap(MEMiHeapHead *heap);

#ifndef NDEBUG
void MEMDumpHeap(MEMiHeapHead *heap);
u32 MEMSetFillValForHeap(MEMHeapFill type, u32 fillVal);
u32 MEMGetFillValForHeap(MEMHeapFill type);
#endif

#ifdef __cplusplus
}
#endif

#endif
