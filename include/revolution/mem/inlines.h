#ifndef RVL_SDK_MEM_INLINES_H
#define RVL_SDK_MEM_INLINES_H

#include <revolution/mem/heapCommon.h>

#include <string.h>

static inline uintptr_t GetUIntPtr(void const *ptr)
{
	return (uintptr_t)ptr;
}

static inline u32 GetOffsetFromPtr(void *p0, void *p1)
{
	return GetUIntPtr(p1) - GetUIntPtr(p0);
}

static inline u16 GetOptForHeap(MEMiHeapHead *heap)
{
	return heap->attribute.fields.optFlag;
}

static inline void SetOptForHeap(MEMiHeapHead *heap, u16 opt)
{
	heap->attribute.fields.optFlag = (u8)opt;
}

static inline void FillNoUseMemory(MEMiHeapHead *heap, void *address, u32 size)
{
#ifndef NDEBUG
	if (GetOptForHeap(heap) & 2)
		memset(address, MEMGetFillValForHeap(MEM_HEAP_FILL_NO_USE), size);
#endif
}

static inline void LockHeap(MEMiHeapHead *heap)
{
	if (GetOptForHeap(heap) & 4)
		OSLockMutex(&heap->mutex);
}

static inline void UnlockHeap(MEMiHeapHead *heap)
{
	if (GetOptForHeap(heap) & 4)
		OSUnlockMutex(&heap->mutex);
}

#endif
