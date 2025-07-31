#include <revolution/mem/inlines.h>

#ifndef __cplusplus
# undef NULL
# define NULL ((void *)0)
#endif

static MEMList sRootList;
static OSMutex sRootMutex;
static BOOL sRootListInitialized;

#ifndef NDEBUG
static u32 sFillVals[3] = {
	0xc3c3c3c3,
	0xf3f3f3f3,
	0xd3d3d3d3,
};
#endif

static MEMiHeapHead *FindContainHeap_(MEMList *list, void const *block)
{
	MEMiHeapHead *heap = NULL;

	while (NULL != (heap = (MEMiHeapHead *)MEMGetNextListObject(list, heap)))
	{
		if (GetUIntPtr(heap->heapStart) > GetUIntPtr(block)
		    || GetUIntPtr(block) >= GetUIntPtr(heap->heapEnd))
		{
			continue;
		}

		MEMiHeapHead *search = FindContainHeap_(&heap->childList, block);
		if (search)
			return search;
		return heap;
	}

	return NULL;
}

static MEMiHeapHead *FindParentHeap_(MEMiHeapHead *search,
                                     MEMiHeapHead *pHeapHd)
{
	MEMList *list = &search->childList;
	MEMiHeapHead *curr = NULL;

	while (NULL != (curr = (MEMiHeapHead *)MEMGetNextListObject(list, curr)))
	{
		if (curr == pHeapHd)
			return search;

		if (GetUIntPtr(curr->heapStart) > GetUIntPtr(pHeapHd)
		    || GetUIntPtr(pHeapHd) >= GetUIntPtr(curr->heapEnd))
		{
			continue;
		}

		return FindParentHeap_(curr, pHeapHd);
	}

	return NULL;
}

static MEMList *FindListContainHeap_(MEMiHeapHead *pHeapHd)
{
	MEMList *list = &sRootList;
	MEMiHeapHead *containHeap = FindContainHeap_(&sRootList, pHeapHd);
	if (containHeap)
		list = &containHeap->childList;

	return list;
}

static BOOL ListContainsHeap_(MEMList *list, MEMiHeapHead *pHeapHd)
{
	MEMiHeapHead *search = NULL;

	while (NULL
	       != (search = (MEMiHeapHead *)MEMGetNextListObject(list, search)))
	{
		if (search == pHeapHd)
			return TRUE;
	}

	return FALSE;
}

void MEMiInitHeapHead(MEMiHeapHead *pHeapHd, u32 signature, void *heapStart,
                      void *heapEnd, u16 opt)
{
	pHeapHd->signature = signature;
	pHeapHd->heapStart = heapStart;
	pHeapHd->heapEnd = heapEnd;
	pHeapHd->attribute.val = 0;
	SetOptForHeap(pHeapHd, opt);
	FillNoUseMemory(pHeapHd, heapStart, GetOffsetFromPtr(heapStart, heapEnd));

	MEMInitList(&pHeapHd->childList, offsetof(MEMiHeapHead, link));
	if (!sRootListInitialized)
	{
		MEMInitList(&sRootList, offsetof(MEMiHeapHead, link));
		OSInitMutex(&sRootMutex);
		sRootListInitialized = TRUE;
	}

	OSInitMutex(&pHeapHd->mutex);
	OSLockMutex(&sRootMutex);
	MEMAppendListObject(FindListContainHeap_(pHeapHd), pHeapHd);
	OSUnlockMutex(&sRootMutex);
}

void MEMiFinalizeHeap(MEMiHeapHead *pHeapHd)
{
	MEMList *pList;

	OSLockMutex(&sRootMutex);
	pList = FindListContainHeap_(pHeapHd);
	OSAssert_Line(264, ListContainsHeap_( pList, pHeapHd ));
	MEMRemoveListObject(pList, pHeapHd);
	OSUnlockMutex(&sRootMutex);

	pHeapHd->signature = 0;
}

void MEMiDumpHeapHead(MEMiHeapHead *pHeapHd)
{
	OSReport("[OS Foundation ");
	switch (pHeapHd->signature)
	{
	case 'EXPH':
		OSReport("Exp");
		break;
	case 'FRMH':
		OSReport("Frame");
		break;
	case 'UNTH':
		OSReport("Unit");
		break;
	default:
		OSAssert_Line(291, FALSE);
	}

	OSReport(" Heap]\n");
	OSReport("    whole [%p - %p)\n", pHeapHd, pHeapHd->heapEnd);
}

MEMHeapHandle *MEMFindContainHeap(void const *block)
{
	return FindContainHeap_(&sRootList, block);
}

MEMHeapHandle *MEMFindParentHeap(MEMHeapHandle *heap)
{
	MEMiHeapHead *curr = NULL;

	while (NULL
	       != (curr = (MEMiHeapHead *)MEMGetNextListObject(&sRootList, curr)))
	{
		if (curr == heap)
			return NULL;

		if (GetUIntPtr(curr->heapStart) > GetUIntPtr(heap)
		    || GetUIntPtr(heap) >= GetUIntPtr(curr->heapEnd))
		{
			continue;
		}

		return FindParentHeap_(curr, heap);
	}

	return NULL;
}

#ifndef NDEBUG
void MEMiDumpExpHeap(MEMHeapHandle *);
void MEMiDumpFrmHeap(MEMHeapHandle *);
void MEMiDumpUnitHeap(MEMHeapHandle *);

void MEMDumpHeap(MEMHeapHandle *heap)
{
	MEMiHeapHead *pHeapHd = heap;
	switch (pHeapHd->signature)
	{
	case 'EXPH':
		LockHeap(heap);
		MEMiDumpExpHeap(heap);
		UnlockHeap(heap);
		break;
	case 'FRMH':
		LockHeap(heap);
		MEMiDumpFrmHeap(heap);
		UnlockHeap(heap);
		break;
	case 'UNTH':
		LockHeap(heap);
		MEMiDumpUnitHeap(heap);
		UnlockHeap(heap);
		break;
	default:
		OSReport("[OS Foundation] dump heap : unknown heap. - %p\n", heap);
	}
}

u32 MEMSetFillValForHeap(MEMHeapFill type, u32 fillVal)
{
	OSAssert_Line(422, type < MEM_HEAP_FILL_MAX);
	{
		u32 old = sFillVals[type];
		sFillVals[type] = fillVal;
		return old;
	}
}

u32 MEMGetFillValForHeap(MEMHeapFill type)
{
	OSAssert_Line(447, type < MEM_HEAP_FILL_MAX);
	return sFillVals[type];
}
#endif
