#ifndef RVL_SDK_MEM_INLINES_H
#define RVL_SDK_MEM_INLINES_H

#include <revolution/mem/heapCommon.h>

#include <string.h>

static inline uintptr_t GetUIntPtr(void const *ptr)
{
	return (uintptr_t)ptr;
}

static inline void *AddU32ToPtr(void *ptr, u32 op)
{
	return (void *)(GetUIntPtr(ptr) + op);
}

static inline void const *AddU32ToCPtr(void const *ptr, u32 op)
{
	return (void const *)(GetUIntPtr(ptr) + op);
}

static inline void *SubU32ToPtr(void *ptr, u32 op)
{
	return (void *)(GetUIntPtr(ptr) - op);
}

static inline void const *SubU32ToCPtr(void const *ptr, u32 op)
{
	return (void const *)(GetUIntPtr(ptr) - op);
}

static inline u32 GetOffsetFromPtr(void *p0, void *p1)
{
	return GetUIntPtr(p1) - GetUIntPtr(p0);
}

static inline s32 ComparePtr(void *p0, void *p1)
{
	u8 *lhs = p0;
	u8 *rhs = p1;
	return lhs - rhs;
}

#define RoundUpPtr(ptr, align) \
	((void *)(((GetUIntPtr(ptr)) + (align) - 1) & (~((align) - 1))))

#define RoundDownPtr(ptr, align) \
	((void *)((GetUIntPtr(ptr)) & (~((align) - 1))))

static inline u16 GetOptForHeap(MEMiHeapHead *pHeapHd)
{
	return pHeapHd->attribute.fields.optFlag;
}

static inline void SetOptForHeap(MEMiHeapHead *pHeapHd, u16 opt)
{
	pHeapHd->attribute.fields.optFlag = (u8)opt;
}

static inline void FillNoUseMemory(MEMiHeapHead *pHeapHd, void *address,
                                   u32 size)
{
#ifndef NDEBUG
	if (GetOptForHeap(pHeapHd) & 2)
		memset(address, MEMGetFillValForHeap(MEM_HEAP_FILL_NO_USE), size);
#endif
}

static inline void FillAllocMemory(MEMiHeapHead *pHeapHd, void *address,
                                   u32 size)
{
	if (GetOptForHeap(pHeapHd) & 1)
		memset(address, 0, size);
#ifndef NDEBUG
	else if (GetOptForHeap(pHeapHd) & 2)
		memset(address, MEMGetFillValForHeap(1), size);
#endif
}

static inline void FillFreeMemory(MEMiHeapHead *pHeapHd, void *address,
                                  u32 size)
{
#ifndef NDEBUG
	if (GetOptForHeap(pHeapHd) & 2)
		memset(address, MEMGetFillValForHeap(MEM_HEAP_FILL_FREE), size);
#endif
}

static inline void LockHeap(MEMiHeapHead *pHeapHd)
{
	if (GetOptForHeap(pHeapHd) & 4)
		OSLockMutex(&pHeapHd->mutex);
}

static inline void UnlockHeap(MEMiHeapHead *pHeapHd)
{
	if (GetOptForHeap(pHeapHd) & 4)
		OSUnlockMutex(&pHeapHd->mutex);
}

#endif
