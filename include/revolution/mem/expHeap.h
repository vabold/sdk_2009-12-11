#ifndef RVL_SDK_MEM_EXP_HEAP_H
#define RVL_SDK_MEM_EXP_HEAP_H

#include <revolution/mem/heapCommon.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct MEMiExpBlockHead MEMiExpBlockHead;

typedef struct MEMiExpBlockLink
{
	MEMiExpBlockHead *prev;
	MEMiExpBlockHead *next;
} MEMiExpBlockLink;

typedef struct MEMiExpBlockList
{
	MEMiExpBlockHead *head;
	MEMiExpBlockHead *tail;
} MEMiExpBlockList;

typedef struct MEMiExpBlockHead
{
	u16 signature;
	union
	{
		u16 val;
		struct
		{
			u16 direction : 1;
			u16 alignment : 7;
			u16 groupID : 8;
		} fields;
	} attribute;
	u32 size;
	MEMiExpBlockLink link;
} MEMiExpBlockHead;

typedef struct MEMiExpHeapExt
{
	MEMiExpBlockList freeBlocks;
	MEMiExpBlockList usedBlocks;
	u16 groupID;
	union
	{
		u16 val;
		struct
		{
			u16 _reserved : 14;
			u16 useMarginOfAlignment : 1;
			u16 allocMode : 1;
		} fields;
	} attribute;
} MEMiExpHeapExt;

typedef struct MEMiExpHeapHead
{
	MEMiHeapHead base;
	MEMiExpHeapExt ext;
} MEMiExpHeapHead;

void MEMiDumpExpHeap(MEMHeapHandle *heap);

MEMHeapHandle *MEMCreateExpHeapEx(void *startAddress, u32 size, u16 flag);
static inline MEMHeapHandle *MEMCreateExpHeap(void *startAddress, u32 size)
{
	return MEMCreateExpHeapEx(startAddress, size, 0);
}
void *MEMDestroyExpHeap(MEMHeapHandle *heap);
void *MEMAllocFromExpHeapEx(MEMHeapHandle *heap, u32 size, s32 align);
static inline void *MEMAllocFromExpHeap(MEMHeapHandle *heap, u32 size)
{
	return MEMAllocFromExpHeapEx(heap, size, 4);
}
u32 MEMResizeForMBlockExpHeap(MEMHeapHandle *heap, void *block, u32 size);
void MEMFreeToExpHeap(MEMHeapHandle *heap, void *block);
u32 MEMGetTotalFreeSizeForExpHeap(MEMHeapHandle *heap);
u32 MEMGetAllocatableSizeForExpHeapEx(MEMHeapHandle *heap, s32 align);
static inline u32 MEMGetAllocatableSizeForExpHeap(MEMHeapHandle *heap)
{
	return MEMGetAllocatableSizeForExpHeapEx(heap, 4);
}
BOOL MEMiIsEmptyExpHeap(MEMiHeapHead *pHeapHd);
u16 MEMSetAllocModeForExpHeap(MEMHeapHandle *heap, u16 allocMode);
u16 MEMGetAllocModeForExpHeap(MEMHeapHandle *heap);
BOOL MEMUseMarginOfAlignmentForExpHeap(MEMHeapHandle *heap, BOOL use);
u16 MEMSetGroupIDForExpHeap(MEMHeapHandle *heap, u16 groupID);
u16 MEMGetGroupIDForExpHeap(MEMHeapHandle *heap);
void MEMVisitAllocatedForExpHeap(MEMHeapHandle *heap,
                                 void (*visitor)(void *, MEMHeapHandle *, u32),
                                 u32 param);
u32 MEMGetSizeForMBlockExpHeap(void const *memBlock);
u16 MEMGetGroupIDForMBlockExpHeap(void const *memBlock);
u16 MEMGetAllocDirForMBlockExpHeap(void const *memBlock);
u32 MEMAdjustExpHeap(MEMHeapHandle *heap);
BOOL MEMCheckExpHeap(MEMHeapHandle *heap, u32 flag);
BOOL MEMCheckForMBlockExpHeap(void const *memBlock, MEMHeapHandle *heap,
                              u32 flag);

#ifdef __cplusplus
}
#endif

#endif
