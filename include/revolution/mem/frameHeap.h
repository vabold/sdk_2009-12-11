#ifndef RVL_SDK_MEM_FRAME_HEAP_H
#define RVL_SDK_MEM_FRAME_HEAP_H

#include <revolution/mem/heapCommon.h>

typedef struct MEMiFrmHeapState MEMiFrmHeapState;

typedef struct MEMiFrmHeapState
{
	u32 tag;
	void *head;
	void *tail;
	MEMiFrmHeapState *prev;
} MEMiFrmHeapState;

typedef struct MEMiFrmHeapExt
{
	void *headAllocator;
	void *tailAllocator;
	MEMiFrmHeapState *pState;
} MEMiFrmHeapExt;

typedef struct MEMiFrmHeapHead
{
	MEMiHeapHead base;
	MEMiFrmHeapExt ext;
} MEMiFrmHeapHead;

void *MEMiGetFreeStartForFrmHeap(MEMHeapHandle *heap);
void *MEMiGetFreeEndForFrmHeap(MEMHeapHandle *heap);
void MEMiDumpFrmHeap(MEMHeapHandle *heap);

MEMHeapHandle *MEMCreateFrmHeapEx(void *startAddress, u32 size, u16 opt);
static inline MEMHeapHandle *MEMCreateFrmHeap(void *startAddress, u32 size)
{
	return MEMCreateFrmHeapEx(startAddress, size, 0);
}
void *MEMDestroyFrmHeap(MEMHeapHandle *heap);
void *MEMAllocFromFrmHeapEx(MEMHeapHandle *heap, u32 size, s32 alignment);
static inline void *MEMAllocFromFrmHeap(MEMHeapHandle *heap, u32 size)
{
	return MEMAllocFromFrmHeapEx(heap, size, 4);
}
void MEMFreeToFrmHeap(MEMHeapHandle *heap, u32 flag);
u32 MEMGetAllocatableSizeForFrmHeapEx(MEMHeapHandle *heap, s32 alignment);
static inline u32 MEMGetAllocatableSizeForFrmHeap(MEMHeapHandle *heap)
{
	return MEMGetAllocatableSizeForFrmHeapEx(heap, 4);
}
BOOL MEMRecordStateForFrmHeap(MEMHeapHandle *heap, u32 tag);
BOOL MEMFreeByStateToFrmHeap(MEMHeapHandle *heap, u32 tag);
u32 MEMAdjustFrmHeap(MEMHeapHandle *heap);
u32 MEMResizeForMBlockFrmHeap(MEMHeapHandle *heap, void *memBlock, u32 size);

#endif
