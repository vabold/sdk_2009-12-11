#include <revolution/mem/frameHeap.h>
#include <revolution/mem/inlines.h>

#include <stdlib.h>

#ifndef __cplusplus
# undef NULL
# define NULL ((void *)0)
#endif

#define MIN_ALIGNMENT 4

static inline BOOL IsValidFrmHeapHandle_(MEMHeapHandle *heap)
{
	if (heap == NULL)
		return FALSE;

	{
		MEMiHeapHead *pHeapHd = heap;
		return pHeapHd->signature == 'FRMH';
	}
}

static inline MEMiFrmHeapExt *GetFrmHeapHeadPtrFromHeapHead_(
	MEMiHeapHead *pHeapHd)
{
	return AddU32ToPtr(pHeapHd, sizeof(MEMiHeapHead));
}

static inline MEMiHeapHead *GetHeapHeadPtrFromFrmHeapHead_(
	MEMiFrmHeapExt *pFrmHeapHd)
{
	return SubU32ToPtr(pFrmHeapHd, sizeof(MEMiHeapHead));
}

static MEMiHeapHead *InitFrameHeap_(void *startAddress, void *endAddress,
                                    u16 opt)
{
	MEMiHeapHead *pHeapHd = (MEMiHeapHead *)startAddress;
	MEMiFrmHeapExt *pFrmHeapHd = GetFrmHeapHeadPtrFromHeapHead_(pHeapHd);

	MEMiInitHeapHead(pHeapHd, 'FRMH',
	                 AddU32ToPtr(pFrmHeapHd, sizeof(MEMiFrmHeapExt)),
	                 endAddress, opt);
	pFrmHeapHd->headAllocator = pHeapHd->heapStart;
	pFrmHeapHd->tailAllocator = pHeapHd->heapEnd;
	pFrmHeapHd->pState = NULL;

	return pHeapHd;
}

static void *AllocFromHead_(MEMiFrmHeapExt *pFrmHeapHd, u32 size, s32 alignment)
{
	void *startAddress = RoundUpPtr(pFrmHeapHd->headAllocator, alignment);
	void *endAddress = AddU32ToPtr(startAddress, size);

	if (GetUIntPtr(endAddress) > GetUIntPtr(pFrmHeapHd->tailAllocator))
		return NULL;

	FillAllocMemory(GetHeapHeadPtrFromFrmHeapHead_(pFrmHeapHd),
	                pFrmHeapHd->headAllocator,
	                GetOffsetFromPtr(pFrmHeapHd->headAllocator, endAddress));
	pFrmHeapHd->headAllocator = endAddress;
	return startAddress;
}

static void *AllocFromTail_(MEMiFrmHeapExt *pFrmHeapHd, u32 size, s32 alignment)
{
	void *startAddress =
		RoundDownPtr(SubU32ToPtr(pFrmHeapHd->tailAllocator, size), alignment);
	if (GetUIntPtr(startAddress) < GetUIntPtr(pFrmHeapHd->headAllocator))
		return NULL;

	FillAllocMemory(GetHeapHeadPtrFromFrmHeapHead_(pFrmHeapHd), startAddress,
	                GetOffsetFromPtr(startAddress, pFrmHeapHd->tailAllocator));
	pFrmHeapHd->tailAllocator = startAddress;
	return startAddress;
}

static void FreeHead_(MEMiHeapHead *pHeapHd)
{
	MEMiFrmHeapExt *pFrmHeapHd = GetFrmHeapHeadPtrFromHeapHead_(pHeapHd);
	FillFreeMemory(
		pHeapHd, pHeapHd->heapStart,
		GetOffsetFromPtr(pHeapHd->heapStart, pFrmHeapHd->headAllocator));
	pFrmHeapHd->headAllocator = pHeapHd->heapStart;
	pFrmHeapHd->pState = NULL;
}

static void FreeTail_(MEMiHeapHead *pHeapHd)
{
	MEMiFrmHeapExt *pFrmHeapHd = GetFrmHeapHeadPtrFromHeapHead_(pHeapHd);
	FillFreeMemory(
		pHeapHd, pFrmHeapHd->tailAllocator,
		GetOffsetFromPtr(pFrmHeapHd->tailAllocator, pHeapHd->heapEnd));
	{
		MEMiFrmHeapState *state;
		for (state = pFrmHeapHd->pState; state; state = state->prev)
			state->tail = pHeapHd->heapEnd;
	}
	pFrmHeapHd->tailAllocator = pHeapHd->heapEnd;
}

static void PrintSize_(u32 size, u32 total)
{
	OSReport("%9d (%6.2f%%)", size, 100.0 * size / total);
}

void *MEMiGetFreeStartForFrmHeap(MEMHeapHandle *heap)
{
	OSAssert_Line(300, IsValidFrmHeapHandle_(heap));
	return GetFrmHeapHeadPtrFromHeapHead_(heap)->headAllocator;
}

void *MEMiGetFreeEndForFrmHeap(MEMHeapHandle *heap)
{
	OSAssert_Line(317, IsValidFrmHeapHandle_(heap));
	return GetFrmHeapHeadPtrFromHeapHead_(heap)->tailAllocator;
}

#ifndef NDEBUG
void MEMiDumpFrmHeap(MEMHeapHandle *heap)
{
	OSAssert_Line(338, IsValidFrmHeapHandle_(heap));
	{
		MEMiHeapHead *pHeapHd = heap;
		MEMiFrmHeapExt *pFrmHeapHd = GetFrmHeapHeadPtrFromHeapHead_(pHeapHd);
		u32 total = GetOffsetFromPtr(pHeapHd->heapStart, pHeapHd->heapEnd);

		MEMiDumpHeapHead(pHeapHd);
		OSReport("     head [%p - %p) ", pHeapHd->heapStart,
		         pFrmHeapHd->headAllocator);
		PrintSize_(
			GetOffsetFromPtr(pHeapHd->heapStart, pFrmHeapHd->headAllocator),
			total);
		OSReport("\n     free                           ");
		PrintSize_(GetOffsetFromPtr(pFrmHeapHd->headAllocator,
		                            pFrmHeapHd->tailAllocator),
		           total);
		OSReport("\n     tail [%p - %p) ", pFrmHeapHd->tailAllocator,
		         pHeapHd->heapEnd);
		PrintSize_(
			GetOffsetFromPtr(pFrmHeapHd->tailAllocator, pHeapHd->heapEnd),
			total);
		OSReport("\n");

		if (pFrmHeapHd->pState)
		{
			MEMiFrmHeapState *state;
			OSReport("    state : [tag]   [head]      [tail]\n");
			for (state = pFrmHeapHd->pState; state; state = state->prev)
			{
				OSReport("            \'%c%c%c%c\' : %p %p\n", state->tag >> 24,
				         (state->tag >> 16) & 0xff, (state->tag >> 8) & 0xff,
				         (state->tag) & 0xff, state->head, state->tail);
			}
		}

		OSReport("\n");
	}
}
#endif

MEMHeapHandle *MEMCreateFrmHeapEx(void *startAddress, u32 size, u16 opt)
{
	void *endAddress;
	OSAssert_Line(408, startAddress != NULL);
	endAddress = RoundDownPtr((AddU32ToPtr(startAddress, size)), 4);
	startAddress = RoundUpPtr(startAddress, 4);

	if (GetUIntPtr(startAddress) > GetUIntPtr(endAddress)
	    || GetOffsetFromPtr(startAddress, endAddress) < sizeof(MEMiFrmHeapHead))
	{
		return NULL;
	}

	{
		MEMHeapHandle *heap = InitFrameHeap_(startAddress, endAddress, opt);
		return heap;
	}
}

void *MEMDestroyFrmHeap(MEMHeapHandle *heap)
{
	OSAssert_Line(439, IsValidFrmHeapHandle_(heap));
	MEMiFinalizeHeap(heap);
	return heap;
}

void *MEMAllocFromFrmHeapEx(MEMHeapHandle *heap, u32 size, s32 alignment)
{
	void *block = NULL;
	MEMiFrmHeapExt *pFrmHeapHd;
	OSAssert_Line(471, IsValidFrmHeapHandle_(heap));

	OSAssert_Line(474, alignment % MIN_ALIGNMENT == 0);
	OSAssert_Line(475, (abs(alignment) & (abs(alignment) - 1)) == 0);
	OSAssert_Line(476, MIN_ALIGNMENT <= abs(alignment));

	pFrmHeapHd = GetFrmHeapHeadPtrFromHeapHead_(heap);

	if (size == 0)
		size = 1;
	size = ROUND_UP(size, 4);

	LockHeap(heap);
	if (alignment >= 0)
		block = AllocFromHead_(pFrmHeapHd, size, alignment);
	else
		block = AllocFromTail_(pFrmHeapHd, size, -alignment);
	UnlockHeap(heap);
	return block;
}

void MEMFreeToFrmHeap(MEMHeapHandle *heap, u32 flag)
{
	OSAssert_Line(519, IsValidFrmHeapHandle_(heap));

	LockHeap(heap);
	if (flag & 1)
		FreeHead_(heap);
	if (flag & 2)
		FreeTail_(heap);
	UnlockHeap(heap);
}

u32 MEMGetAllocatableSizeForFrmHeapEx(MEMHeapHandle *heap, s32 alignment)
{
	OSAssert_Line(554, IsValidFrmHeapHandle_(heap));

	OSAssert_Line(557, alignment % MIN_ALIGNMENT == 0);
	OSAssert_Line(558, (abs(alignment) & (abs(alignment) - 1)) == 0);
	OSAssert_Line(559, MIN_ALIGNMENT <= abs(alignment));

	alignment = abs(alignment);

	{
		u32 size;
		MEMiFrmHeapExt *pFrmHeapHd = GetFrmHeapHeadPtrFromHeapHead_(heap);
		BOOL interrupts = OSDisableInterrupts();

		void *block = RoundUpPtr(pFrmHeapHd->headAllocator, alignment);
		if (GetUIntPtr(block) > GetUIntPtr(pFrmHeapHd->tailAllocator))
			size = 0;
		else
			size = GetOffsetFromPtr(block, pFrmHeapHd->tailAllocator);

		OSRestoreInterrupts(interrupts);
		return size;
	}
}

BOOL MEMRecordStateForFrmHeap(MEMHeapHandle *heap, u32 tag)
{
	BOOL success;
	OSAssert_Line(603, IsValidFrmHeapHandle_(heap));

	LockHeap(heap);
	{
		MEMiFrmHeapExt *pFrmHeapHd = GetFrmHeapHeadPtrFromHeapHead_(heap);
		void *head = pFrmHeapHd->headAllocator;

		MEMiFrmHeapState *state =
			AllocFromHead_(pFrmHeapHd, sizeof(MEMiFrmHeapState), MIN_ALIGNMENT);
		if (!state)
		{
			success = FALSE;
		}
		else
		{
			state->tag = tag;
			state->head = head;
			state->tail = pFrmHeapHd->tailAllocator;
			state->prev = pFrmHeapHd->pState;
			pFrmHeapHd->pState = state;
			success = TRUE;
		}
	}
	UnlockHeap(heap);
	return success;
}

BOOL MEMFreeByStateToFrmHeap(MEMHeapHandle *heap, u32 tag)
{
	BOOL success;
	OSAssert_Line(660, IsValidFrmHeapHandle_(heap));

	LockHeap(heap);
	{
		MEMiFrmHeapExt *pFrmHeapHd = GetFrmHeapHeadPtrFromHeapHead_(heap);
		MEMiFrmHeapState *state = pFrmHeapHd->pState;

		if (tag != 0)
		{
			for (; state; state = state->prev)
				if (state->tag == tag)
					break;
		}

		if (!state)
		{
			success = FALSE;
		}
		else
		{
			void *head = pFrmHeapHd->headAllocator;
			void *tail = pFrmHeapHd->tailAllocator;
			pFrmHeapHd->headAllocator = state->head;
			pFrmHeapHd->tailAllocator = state->tail;
			pFrmHeapHd->pState = state->prev;

			FillFreeMemory(heap, pFrmHeapHd->headAllocator,
			               GetOffsetFromPtr(pFrmHeapHd->headAllocator, head));
			FillFreeMemory(heap, tail,
			               GetOffsetFromPtr(tail, pFrmHeapHd->tailAllocator));

			success = TRUE;
		}
	}
	UnlockHeap(heap);
	return success;
}

u32 MEMAdjustFrmHeap(MEMHeapHandle *heap)
{
	OSAssert_Line(724, IsValidFrmHeapHandle_(heap));

	{
		MEMiHeapHead *pHeapHd = heap;
		MEMiFrmHeapExt *pFrmHeapHd = GetFrmHeapHeadPtrFromHeapHead_(pHeapHd);
		u32 newSize;

		LockHeap(heap);

		if (GetOffsetFromPtr(pFrmHeapHd->tailAllocator, pHeapHd->heapEnd) > 0)
		{
			newSize = 0;
		}
		else
		{
			pFrmHeapHd->tailAllocator = pHeapHd->heapEnd =
				pFrmHeapHd->headAllocator;
			newSize = GetOffsetFromPtr(heap, pHeapHd->heapEnd);
		}

		UnlockHeap(heap);
		return newSize;
	}
}

u32 MEMResizeForMBlockFrmHeap(MEMHeapHandle *heap, void *memBlock, u32 size)
{
	MEMiHeapHead *pHeapHd = NULL;
	MEMiFrmHeapExt *pFrmHeapHd = NULL;

	OSAssert_Line(775, IsValidFrmHeapHandle_(heap));
	OSAssert_Line(776, memBlock == RoundDownPtr(memBlock, MIN_ALIGNMENT));

	pHeapHd = heap;
	pFrmHeapHd = GetFrmHeapHeadPtrFromHeapHead_(pHeapHd);

	OSAssert_Line(782, ComparePtr(pHeapHd->heapStart, memBlock) <= 0 && ComparePtr(pFrmHeapHd->headAllocator, memBlock) > 0);
	OSAssert_Line(784, pFrmHeapHd->pState == NULL || ComparePtr(pFrmHeapHd->pState, memBlock) < 0);

	if (size == 0)
		size = 1;
	size = ROUND_UP(size, MIN_ALIGNMENT);

	LockHeap(heap);
	{
		u32 currentSize = GetOffsetFromPtr(memBlock, pFrmHeapHd->headAllocator);
		void *end = AddU32ToPtr(memBlock, size);

		if (size == currentSize)
			goto ret;

		if (size > currentSize)
		{
			if (ComparePtr(end, pFrmHeapHd->tailAllocator) > 0)
			{
				size = 0;
				goto ret;
			}

			FillAllocMemory(heap, pFrmHeapHd->headAllocator,
			                size - currentSize);
		}
		else
		{
			FillFreeMemory(heap, end, currentSize - size);
		}

		pFrmHeapHd->headAllocator = end;
	}
ret:
	UnlockHeap(heap);
	return size;
}
