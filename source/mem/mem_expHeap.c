#include <revolution/mem/expHeap.h>
#include <revolution/mem/inlines.h>

#include <stdlib.h>

#ifndef __cplusplus
# undef NULL
# define NULL ((void *)0)
#endif

#undef ROUND_UP_PTR
#define ROUND_UP_PTR(ptr, align) \
	((void *)(((GetUIntPtr(ptr)) + (align) - 1) & (~((align) - 1))))

#undef ROUND_DOWN_PTR
#define ROUND_DOWN_PTR(ptr, align) \
	((void *)((GetUIntPtr(ptr)) & (~((align) - 1))))

#define MIN_ALIGNMENT 4
#define MAX_GROUPID 255

#define MEM_HEAP_PRINT(cond, ...) ((cond) && (OSReport(__VA_ARGS__), 0))

typedef struct Region
{
	void *start;
	void *end;
} Region;

static inline BOOL IsValidExpHeapHandle_(MEMHeapHandle *heap)
{
	if (heap == NULL)
		return FALSE;

	{
		MEMiHeapHead *pHeapHd = heap;
		return pHeapHd->signature == 'EXPH';
	}
}

static inline MEMiExpHeapExt *GetExpHeapHeadPtrFromHeapHead_(
	MEMiHeapHead *pHeapHd)
{
	return AddU32ToPtr(pHeapHd, sizeof(MEMiHeapHead));
}

static inline MEMiExpHeapExt *GetExpHeapHeadPtrFromHandle_(MEMHeapHandle *heap)
{
	return GetExpHeapHeadPtrFromHeapHead_(heap);
}

static inline MEMiHeapHead *GetHeapHeadPtrFromExpHeapHead_(MEMiExpHeapExt *ext)
{
	return SubU32ToPtr(ext, sizeof(MEMiHeapHead));
}

static inline u16 GetAllocMode_(MEMiExpHeapExt *ext)
{
	return ext->attribute.fields.allocMode;
}

static inline void SetAllocMode_(MEMiExpHeapExt *ext, u16 mode)
{
	ext->attribute.fields.allocMode = mode;
}

static inline u16 GetAllocDirForMBlock_(MEMiExpBlockHead const *block)
{
	return block->attribute.fields.direction;
}

static inline u16 GetAlignmentForMBlock_(MEMiExpBlockHead const *block)
{
	return block->attribute.fields.alignment;
}

static inline u16 GetGroupIDForMBlock_(MEMiExpBlockHead const *block)
{
	return block->attribute.fields.groupID;
}

static inline void SetAllocDirForMBlock_(MEMiExpBlockHead *block, u16 direction)
{
	block->attribute.fields.direction = direction;
}

static inline void SetAlignmentForMBlock_(MEMiExpBlockHead *block,
                                          u16 alignment)
{
	block->attribute.fields.alignment = alignment;
}

static inline void SetGroupIDForMBlock_(MEMiExpBlockHead *block, u16 groupID)
{
	block->attribute.fields.groupID = (u8)groupID;
}

static inline void *GetMemPtrForMBlock_(MEMiExpBlockHead *block)
{
	return AddU32ToPtr(block, sizeof(MEMiExpBlockHead));
}

static inline void const *GetMemCPtrForMBlock_(MEMiExpBlockHead const *block)
{
	return AddU32ToCPtr(block, sizeof(MEMiExpBlockHead));
}

static inline MEMiExpBlockHead *GetMBlockHeadPtr_(void *block)
{
	return SubU32ToPtr(block, sizeof(MEMiExpBlockHead));
}

static inline MEMiExpBlockHead const *GetMBlockHeadCPtr_(void const *block)
{
	return SubU32ToCPtr(block, sizeof(MEMiExpBlockHead));
}

static inline void *GetMBlockEndAddr_(MEMiExpBlockHead *block)
{
	return AddU32ToPtr(GetMemPtrForMBlock_(block), block->size);
}

static void GetRegionOfMBlock_(Region *region, MEMiExpBlockHead *block)
{
	region->start = SubU32ToPtr(block, GetAlignmentForMBlock_(block));
	region->end = GetMBlockEndAddr_(block);
}

static MEMiExpBlockHead *RemoveMBlock_(MEMiExpBlockList *list,
                                       MEMiExpBlockHead *block)
{
	MEMiExpBlockHead *prev = block->link.prev;
	MEMiExpBlockHead *next = block->link.next;

	if (prev)
		prev->link.next = next;
	else
		list->head = next;

	if (next)
		next->link.prev = prev;
	else
		list->tail = prev;

	return prev;
}

static MEMiExpBlockHead *InsertMBlock_(MEMiExpBlockList *list,
                                       MEMiExpBlockHead *block,
                                       MEMiExpBlockHead *prev)
{
	MEMiExpBlockHead *next;

	block->link.prev = prev;
	if (prev)
	{
		next = prev->link.next;
		prev->link.next = block;
	}
	else
	{
		next = list->head;
		list->head = block;
	}

	block->link.next = next;
	if (next)
		next->link.prev = block;
	else
		list->tail = block;

	return block;
}

static inline MEMiExpBlockHead *AppendMBlock_(MEMiExpBlockList *list,
                                              MEMiExpBlockHead *block)
{
	return InsertMBlock_(list, block, list->tail);
}

static MEMiExpBlockHead *InitMBlock_(Region *region, u16 signature)
{
	MEMiExpBlockHead *block = region->start;

	block->signature = signature;
	block->attribute.val = 0;
	block->size = GetOffsetFromPtr(GetMemPtrForMBlock_(block), region->end);
	block->link.prev = NULL;
	block->link.next = NULL;

	return block;
}

static inline MEMiExpBlockHead *InitFreeMBlock_(Region *region)
{
	return InitMBlock_(region, 'FR');
}

static MEMiHeapHead *InitExpHeap_(void *startAddress, void *endAddress, u16 opt)
{
	MEMiHeapHead *pHeapHd = (MEMiHeapHead *)startAddress;
	MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHeapHead_(pHeapHd);

	MEMiInitHeapHead(pHeapHd, 'EXPH', AddU32ToPtr(ext, sizeof(MEMiExpHeapExt)),
	                 endAddress, opt);
	ext->groupID = 0;
	ext->attribute.val = 0;
	SetAllocMode_(ext, 0);

	{
		MEMiExpBlockHead *block;
		Region region;

		region.start = pHeapHd->heapStart;
		region.end = pHeapHd->heapEnd;
		block = InitFreeMBlock_(&region);

		ext->freeBlocks.head = block;
		ext->freeBlocks.tail = block;
		ext->usedBlocks.head = NULL;
		ext->usedBlocks.tail = NULL;

		return pHeapHd;
	}
}

static void *AllocUsedBlockFromFreeBlock_(MEMiExpHeapExt *ext,
                                          MEMiExpBlockHead *block,
                                          void *address, u32 size,
                                          u16 direction)
{
	// Assume [address, address + size) is in free [block start, block end)
	// The resulting layout of block's memory region will become:
	// [block.start, address), [address, address + size), [address + size, block.end),
	// where the left and right regions are free blocks and the middle region is used
	Region leftRegion;
	Region rightRegion;
	MEMiExpBlockHead *prev;

	// First, set up the initial regions
	// The used region could be formed from here with [left.end, right.start)
	// However, we want to handle margins first
	GetRegionOfMBlock_(&leftRegion, block);
	rightRegion.end = leftRegion.end;
	rightRegion.start = AddU32ToPtr(address, size);
	leftRegion.end = SubU32ToPtr(address, sizeof(MEMiExpBlockHead));

	// Now we split the free block into the two free regions
	prev = RemoveMBlock_(&ext->freeBlocks, block);

	// For free blocks to be made from the region, it requires:
	// - Enough size for a block and minimum allocation
	// - Allocating from specified direction requires margin usage
	// Head direction affects left region, tail direction affects right region
	// If we don't have enough size or we're not using margins, consider the region allocated here

	if (GetOffsetFromPtr(leftRegion.start, leftRegion.end)
	        < sizeof(MEMiExpBlockHead) + 4
	    || (direction == 0 && !ext->attribute.fields.useMarginOfAlignment))
		// NOTE: This (and only this) is fragmentation
		leftRegion.end = leftRegion.start;
	else
		prev =
			InsertMBlock_(&ext->freeBlocks, InitFreeMBlock_(&leftRegion), prev);

	if (GetOffsetFromPtr(rightRegion.start, rightRegion.end)
	        < sizeof(MEMiExpBlockHead) + 4
	    || (direction == 1 && !ext->attribute.fields.useMarginOfAlignment))
		rightRegion.start = rightRegion.end;
	else
		InsertMBlock_(&ext->freeBlocks, InitFreeMBlock_(&rightRegion), prev);

	// leftRegion.end is not guaranteed to be (address - block head size)
	// Similarly, rightRegion.start is not guaranteed to be (address + size)
	// This allows margins to be considered allocated
	FillAllocMemory(GetHeapHeadPtrFromExpHeapHead_(ext), leftRegion.end,
	                GetOffsetFromPtr(leftRegion.end, rightRegion.start));

	{
		Region usedRegion;
		MEMiExpBlockHead *head;

		// The memory block considers the right margin to be part of the used block
		// It cannot do the same for the left margin because block heads must be 0x10 away
		usedRegion.start = SubU32ToPtr(address, sizeof(MEMiExpBlockHead));
		usedRegion.end = rightRegion.start;
		head = InitMBlock_(&usedRegion, 'UD');

		SetAllocDirForMBlock_(head, direction);
		SetAlignmentForMBlock_(head, GetOffsetFromPtr(leftRegion.end, head));
		SetGroupIDForMBlock_(head, ext->groupID);
		AppendMBlock_(&ext->usedBlocks, head);
	}

	return address;
}

static void *AllocFromHead_(MEMiHeapHead *pHeapHd, u32 size, s32 alignment)
{
	MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHeapHead_(pHeapHd);
	// fastAlloc == TRUE => first-fit allocation
	// fastAlloc == FALSE => best-fit allocation
	BOOL fastAlloc = GetAllocMode_(ext) == 0;
	MEMiExpBlockHead *block = NULL;
	MEMiExpBlockHead *found = NULL;
	u32 blockSize = -1;
	void *bestAddress = NULL;

	for (block = ext->freeBlocks.head; block; block = block->link.next)
	{
		void * const memptr = GetMemPtrForMBlock_(block);
		void * const address = ROUND_UP_PTR(memptr, alignment);
		u32 offset = GetOffsetFromPtr(memptr, address);
		if (block->size < size + offset)
			continue;

		if (blockSize <= block->size)
			continue;

		found = block;
		blockSize = block->size;
		bestAddress = address;

		if (fastAlloc || blockSize == size)
			break;
	}

	if (!found)
		return NULL;

	return AllocUsedBlockFromFreeBlock_(ext, found, bestAddress, size, 0);
}

static void *AllocFromTail_(MEMiHeapHead *pHeapHd, u32 size, s32 alignment)
{
	MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHeapHead_(pHeapHd);
	// fastAlloc == TRUE => first-fit allocation
	// fastAlloc == FALSE => best-fit allocation
	BOOL fastAlloc = GetAllocMode_(ext) == 0;
	MEMiExpBlockHead *block = NULL;
	MEMiExpBlockHead *found = NULL;
	u32 blockSize = -1;
	void *bestAddress = NULL;

	for (block = ext->freeBlocks.tail; block; block = block->link.prev)
	{
		void * const start = GetMemPtrForMBlock_(block);
		void * const endAddr = AddU32ToPtr(start, block->size);
		void * const end =
			ROUND_DOWN_PTR(SubU32ToPtr(endAddr, size), alignment);
		if (ComparePtr(end, start) < 0)
			continue;

		if (blockSize <= block->size)
			continue;

		found = block;
		blockSize = block->size;
		bestAddress = end;

		if (fastAlloc || blockSize == size)
			break;
	}

	if (!found)
		return NULL;

	return AllocUsedBlockFromFreeBlock_(ext, found, bestAddress, size, 1);
}

static BOOL RecycleRegion_(MEMiExpHeapExt *ext, Region *initialRegion)
{
	// Assume we have the following layout:
	// [left.start, region.start), [region.start, region.end), [region.end, right.end),
	// where the left and right regions are free blocks and the middle region is the arg
	// Marking the middle region as free merges all three free blocks into one
	MEMiExpBlockHead *block = NULL;
	Region region = *initialRegion;

	{
		MEMiExpBlockHead *search;
		for (search = ext->freeBlocks.head; search; search = search->link.next)
		{
			if (search < initialRegion->start)
			{
				block = search;
				continue;
			}

			if (search == initialRegion->end)
			{
				// We found the block to the right, include it for merging
				region.end = GetMBlockEndAddr_(search);
				RemoveMBlock_(&ext->freeBlocks, search);
				FillNoUseMemory(GetHeapHeadPtrFromExpHeapHead_(ext), search,
				                sizeof(MEMiExpBlockHead));
			}
			break;
		}
	}

	// Found the closest left region to the middle, see if it can be merged
	// NOTE: Fragmentation from AllocUsedBlockFromFreeBlock_ can prevent merging
	if (block && GetMBlockEndAddr_(block) == initialRegion->start)
	{
		region.start = block;
		block = RemoveMBlock_(&ext->freeBlocks, block);
	}

	// This can create a free block with no allocation, but it's fine
	// The free block head just has to exist so it can be merged later
	if (GetOffsetFromPtr(region.start, region.end) < sizeof(MEMiExpBlockHead))
		return FALSE;

	FillFreeMemory(GetHeapHeadPtrFromExpHeapHead_(ext), initialRegion->start,
	               GetOffsetFromPtr(initialRegion->start, initialRegion->end));
	InsertMBlock_(&ext->freeBlocks, InitFreeMBlock_(&region), block);
	return TRUE;
}

static BOOL CheckMBlock_(MEMiExpBlockHead const *block, MEMiHeapHead *pHeapHd,
                         u16 signature, char const *type, u32 flag)
{
	BOOL print = (flag & 1) != 0;
	void const *memBlock = GetMemCPtrForMBlock_(block);

	if (pHeapHd)
	{
		if (GetUIntPtr(block) < GetUIntPtr(pHeapHd->heapStart)
		    || GetUIntPtr(memBlock) > GetUIntPtr(pHeapHd->heapEnd))
		{
			MEM_HEAP_PRINT(
				print,
				"[OS Foundation Exp Heap] Bad %s memory block address. - "
				"address %08X, heap area [%08X - %08X)\n",
				type, memBlock, pHeapHd->heapStart, pHeapHd->heapEnd);
			return FALSE;
		}
	}
	else if (GetUIntPtr(block) < 0x80000000)
	{
		MEM_HEAP_PRINT(
			print,
			"[OS Foundation Exp Heap] Bad %s memory block address. - "
			"address %08X\n",
			type, memBlock);
		return FALSE;
	}

	if (block->signature != signature)
	{
		MEM_HEAP_PRINT(
			print,
			"[OS Foundation Exp Heap] Bad %s memory block signature. - "
			"address %08X, signature %04X\n",
			type, memBlock, block->signature);
		return FALSE;
	}

	if (block->size >= 0x8000000)
	{
		MEM_HEAP_PRINT(print,
		               "[OS Foundation Exp Heap] Too large %s memory block. - "
		               "address %08X, block size %08X\n",
		               type, memBlock, block->size);
		return FALSE;
	}

	if (pHeapHd)
	{
		if (GetUIntPtr(memBlock) + block->size > GetUIntPtr(pHeapHd->heapEnd))
		{
			MEM_HEAP_PRINT(
				print,
				"[OS Foundation Exp Heap] wrong size %s memory block. - "
				"address %08X, block size %08X\n",
				type, memBlock, block->size);
			return FALSE;
		}
	}

	return TRUE;
}

static inline BOOL CheckUsedMBlock_(MEMiExpBlockHead const *block,
                                    MEMiHeapHead *pHeapHd, u32 flag)
{
	if (pHeapHd)
	{
		MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHeapHead_(pHeapHd);
		MEMiExpBlockHead *search = NULL;
		for (search = ext->usedBlocks.head; search; search = search->link.next)
			if (block == search)
				break;

		if (search == NULL)
			return FALSE;
	}
	return CheckMBlock_(block, pHeapHd, 'UD', "used", flag);
}

static inline BOOL CheckFreeMBlock_(MEMiExpBlockHead const *block,
                                    MEMiHeapHead *pHeapHd, u32 flag)
{
	return CheckMBlock_(block, pHeapHd, 'FR', "free", flag);
}

static BOOL CheckMBlockPrevPtr_(MEMiExpBlockHead const *block,
                                MEMiExpBlockHead const *prev, u32 flag)
{
	BOOL print = (flag & 1) != 0;
	if (block->link.prev != prev)
	{
		MEM_HEAP_PRINT(print,
		               "[OS Foundation Exp Heap] Wrong link memory block. - "
		               "address %08X, previous address %08X != %08X\n",
		               GetMemCPtrForMBlock_(block), block->link.prev, prev);
		return FALSE;
	}
	return TRUE;
}

static BOOL CheckMBlockNextPtr_(MEMiExpBlockHead const *block,
                                MEMiExpBlockHead const *next, u32 flag)
{
	BOOL print = (flag & 1) != 0;
	if (block->link.next != next)
	{
		MEM_HEAP_PRINT(print,
		               "[OS Foundation Exp Heap] Wrong link memory block. - "
		               "address %08X, next address %08X != %08X\n",
		               GetMemCPtrForMBlock_(block), block->link.next, next);
		return FALSE;
	}
	return TRUE;
}

static BOOL CheckMBlockLinkTail_(MEMiExpBlockHead const *block,
                                 MEMiExpBlockHead *tail, char const *type,
                                 u32 flag)
{
	BOOL print = (flag & 1) != 0;
	if (block != tail)
	{
		MEM_HEAP_PRINT(print,
		               "[OS Foundation Exp Heap] Wrong memory brock list %s "
		               "pointer. - address %08X, %s address %08X != %08X\n",
		               type, GetMemCPtrForMBlock_(block), type, block, tail);
		return FALSE;
	}

	return TRUE;
}

static BOOL IsValidUsedMBlock_(void const *memBlock, MEMHeapHandle *heap)
{
	MEMiHeapHead *pHeapHd = heap;
	BOOL valid;

	if (!memBlock)
		return FALSE;

	if (heap)
		LockHeap(heap);

	valid = CheckUsedMBlock_(GetMBlockHeadCPtr_(memBlock), pHeapHd, 0);

	if (heap)
		UnlockHeap(heap);

	return valid;
}

#ifndef NDEBUG
void MEMiDumpExpHeap(MEMHeapHandle *heap)
{
	OSAssert_Line(1193, IsValidExpHeapHandle_(heap));

	{
		u32 usedTotalSize = 0;
		u32 usedBlockCount = 0;
		u32 unused = 0; // free total size?
		u32 freeBlockCount = 0;

		MEMiHeapHead *pHeapHd = heap;
		// Should be "GetExpHeapHeadPtrFromHeapHead_"
		MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHandle_(pHeapHd);
		MEMiDumpHeapHead(pHeapHd);

		// Table header; attribute, address, size, group ID, alignment, links
		OSReport("     attr  address:   size    gid aln   prev_ptr next_ptr\n");
		OSReport("    (Used Blocks)\n");

		if (ext->usedBlocks.head == NULL)
			OSReport("     NONE\n");
		else
		{
			MEMiExpBlockHead *it;
			for (it = ext->usedBlocks.head; it; it = it->link.next)
			{
				if (it->signature != 'UD')
				{
					// Used block list got corrupted somewhere, not safe to read next
					OSReport("    xxxxx %08x: --------  --- ---  (-------- "
					         "--------)\nabort\n",
					         it);
					break;
				}

				OSReport(
					"    %s %08x: %8d  %3d %3d  (%08x %08x)\n",
					GetAllocDirForMBlock_(it) == 1 ? " rear" : "front",
					GetMemPtrForMBlock_(it), it->size, GetGroupIDForMBlock_(it),
					GetAlignmentForMBlock_(it),
					it->link.prev ? GetMemPtrForMBlock_(it->link.prev) : NULL,
					it->link.next ? GetMemPtrForMBlock_(it->link.next) : NULL);

				usedTotalSize += it->size + GetAlignmentForMBlock_(it)
				               + sizeof(MEMiExpBlockHead);
				++usedBlockCount;
			}
		}

		OSReport("    (Free Blocks)\n");

		if (ext->freeBlocks.head == NULL)
			OSReport("     NONE\n");
		else
		{
			MEMiExpBlockHead *it;

			for (it = ext->freeBlocks.head; it; it = it->link.next)
			{
				if (it->signature != 'FR')
				{
					// Free block list got corrupted somewhere, not safe to read next
					OSReport("    xxxxx %08x: --------  --- ---  (-------- "
					         "--------)\nabort\n",
					         it);
					break;
				}

				OSReport(
					"    %s %08x: %8d  %3d %3d  (%08x %08x)\n", " free",
					GetMemPtrForMBlock_(it), it->size, GetGroupIDForMBlock_(it),
					GetAlignmentForMBlock_(it),
					it->link.prev ? GetMemPtrForMBlock_(it->link.prev) : NULL,
					it->link.next ? GetMemPtrForMBlock_(it->link.next) : NULL);

				++freeBlockCount;
			}
		}

		OSReport("\n");

		{
			u32 heapSize =
				GetOffsetFromPtr(pHeapHd->heapStart, pHeapHd->heapEnd);
			OSReport("    %d / %d bytes (%6.2f%%) used (U:%d F:%d)\n",
			         usedTotalSize, heapSize, 100.0 * usedTotalSize / heapSize,
			         usedBlockCount, freeBlockCount);
		}

		OSReport("\n");
	}
}
#endif

MEMHeapHandle *MEMCreateExpHeapEx(void *startAddress, u32 size, u16 opt)
{
	void *endAddress;
	OSAssert_Line(1319, startAddress != NULL);
	endAddress = ROUND_DOWN_PTR((AddU32ToPtr(startAddress, size)), 4);
	startAddress = ROUND_UP_PTR(startAddress, 4);

	if (GetUIntPtr(startAddress) > GetUIntPtr(endAddress)
	    || GetOffsetFromPtr(startAddress, endAddress)
	           < sizeof(MEMiExpHeapHead) + sizeof(MEMiExpBlockHead) + 4)
	{
		return NULL;
	}

	{
		MEMHeapHandle *heap = InitExpHeap_(startAddress, endAddress, opt);
		return heap;
	}
}

void *MEMDestroyExpHeap(MEMHeapHandle *heap)
{
	OSAssert_Line(1350, IsValidExpHeapHandle_(heap));
	MEMiFinalizeHeap(heap);
	return heap;
}

void *MEMAllocFromExpHeapEx(MEMHeapHandle *heap, u32 size, s32 alignment)
{
	void *block = NULL;
	OSAssert_Line(1381, IsValidExpHeapHandle_(heap));

	OSAssert_Line(1384, alignment % MIN_ALIGNMENT == 0);
	OSAssert_Line(1385, (abs(alignment) & (abs(alignment) - 1)) == 0);
	OSAssert_Line(1386, MIN_ALIGNMENT <= abs(alignment));
	OSAssert_Line(1387, (-128 <= alignment) && (alignment <= 128 ));

	if (size == 0)
		size = 1;
	size = ROUND_UP(size, 4);

	LockHeap(heap);
	if (alignment >= 0)
		block = AllocFromHead_(heap, size, alignment);
	else
		block = AllocFromTail_(heap, size, -alignment);
	UnlockHeap(heap);
	return block;
}

u32 MEMResizeForMBlockExpHeap(MEMHeapHandle *heap, void *memBlock, u32 size)
{
	OSAssert_Line(1434, IsValidExpHeapHandle_(heap));
	OSAssert_Line(1435, memBlock != NULL);

	MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHandle_(heap);
	MEMiExpBlockHead *head = GetMBlockHeadPtr_(memBlock);

	size = ROUND_UP(size, 4);
	if (size == head->size)
		return size;

	LockHeap(heap);
	OSAssert_Line(1448, IsValidUsedMBlock_(memBlock, heap));

	if (size > head->size)
	{
		void *end = GetMBlockEndAddr_(head);
		MEMiExpBlockHead *search;
		for (search = ext->freeBlocks.head; search; search = search->link.next)
			if (search == end)
				break;

		if (!search
		    || size > head->size + search->size + sizeof(MEMiExpBlockHead))
		{
			UnlockHeap(heap);
			return 0;
		}
		else
		{
			Region region;
			void *start;
			MEMiExpBlockHead *prev;

			GetRegionOfMBlock_(&region, search);
			prev = RemoveMBlock_(&ext->freeBlocks, search);
			start = region.start;
			region.start = AddU32ToPtr(memBlock, size);

			if (GetOffsetFromPtr(region.start, region.end)
			    < sizeof(MEMiExpBlockHead))
				region.start = region.end;

			head->size = GetOffsetFromPtr(memBlock, region.start);

			if (GetOffsetFromPtr(region.start, region.end)
			    >= sizeof(MEMiExpBlockHead))
				InsertMBlock_(&ext->freeBlocks, InitFreeMBlock_(&region), prev);

			FillAllocMemory(heap, start, GetOffsetFromPtr(start, region.start));
		}
	}
	else
	{
		Region region;
		u32 origSize = head->size;

		region.start = AddU32ToPtr(memBlock, size);
		region.end = GetMBlockEndAddr_(head);

		head->size = size;

		if (!RecycleRegion_(ext, &region))
			head->size = origSize;
	}

	UnlockHeap(heap);
	return head->size;
}

void MEMFreeToExpHeap(MEMHeapHandle *heap, void *memBlock)
{
	OSAssert_Line(1542, IsValidExpHeapHandle_(heap));
	if (memBlock == NULL)
		return;

	LockHeap(heap);
	{
		MEMiHeapHead *pHeapHd = heap;
		// Should be "GetExpHeapHeadPtrFromHeapHead_"
		MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHandle_(pHeapHd);
		MEMiExpBlockHead *head = GetMBlockHeadPtr_(memBlock);
		Region region;

		OSAssert_Line(1555, IsValidUsedMBlock_(memBlock, heap));
		OSAssert_Line(1558, pHeapHd->heapStart <= memBlock && memBlock < pHeapHd->heapEnd);

		GetRegionOfMBlock_(&region, head);
		RemoveMBlock_(&ext->usedBlocks, head);
		RecycleRegion_(ext, &region);
	}
	UnlockHeap(heap);
}

u32 MEMGetTotalFreeSizeForExpHeap(MEMHeapHandle *heap)
{
	u32 size = 0;
	OSAssert_Line(1581, IsValidExpHeapHandle_(heap));

	LockHeap(heap);
	{
		MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHandle_(heap);
		MEMiExpBlockHead *block;

		for (block = ext->freeBlocks.head; block; block = block->link.next)
			size += block->size;
	}
	UnlockHeap(heap);

	return size;
}

u32 MEMGetAllocatableSizeForExpHeapEx(MEMHeapHandle *heap, s32 alignment)
{
	OSAssert_Line(1617, IsValidExpHeapHandle_(heap));
	OSAssert_Line(1620, alignment % MIN_ALIGNMENT == 0);
	OSAssert_Line(1621, (abs(alignment) & (abs(alignment) - 1)) == 0);
	OSAssert_Line(1622, MIN_ALIGNMENT <= abs(alignment));

	alignment = abs(alignment);
	LockHeap(heap);

	{
		MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHandle_(heap);
		MEMiExpBlockHead *block;
		u32 maxSize = 0;
		u32 x = -1;
		for (block = ext->freeBlocks.head; block; block = block->link.next)
		{
			void *address = ROUND_UP_PTR(GetMemPtrForMBlock_(block), alignment);

			if (GetUIntPtr(address) < GetUIntPtr(GetMBlockEndAddr_(block)))
			{
				u32 size = GetOffsetFromPtr(address, GetMBlockEndAddr_(block));
				u32 offset =
					GetOffsetFromPtr(GetMemPtrForMBlock_(block), address);

				if (maxSize < size || (maxSize == size && x > offset))
				{
					maxSize = size;
					x = offset;
				}
			}
		}

		UnlockHeap(heap);
		return maxSize;
	}
}

BOOL MEMiIsEmptyExpHeap(MEMiHeapHead *pHeapHd)
{
	MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHeapHead_(pHeapHd);
	BOOL ret;

	LockHeap(pHeapHd);
	// BUG: This should be checking used blocks head
	ret = ext->freeBlocks.head == NULL;
	UnlockHeap(pHeapHd);

	return ret;
}

u16 MEMSetAllocModeForExpHeap(MEMHeapHandle *heap, u16 allocMode)
{
	BOOL interrupts;
	u16 prev;

	OSAssert_Line(1710, IsValidExpHeapHandle_(heap));

	interrupts = OSDisableInterrupts();
	{
		MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHandle_(heap);
		prev = GetAllocMode_(ext);
		SetAllocMode_(ext, allocMode);
	}
	OSRestoreInterrupts(interrupts);
	return prev;
}

u16 MEMGetAllocModeForExpHeap(MEMHeapHandle *heap)
{
	OSAssert_Line(1735, IsValidExpHeapHandle_(heap));
	return GetAllocMode_(GetExpHeapHeadPtrFromHandle_(heap));
}

BOOL MEMUseMarginOfAlignmentForExpHeap(MEMHeapHandle *heap, BOOL use)
{
	MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHandle_(heap);
	BOOL before;
	OSAssert_Line(1764, IsValidExpHeapHandle_(heap));

	before = ext->attribute.fields.useMarginOfAlignment;
	ext->attribute.fields.useMarginOfAlignment = use;
	return before;
}

u16 MEMSetGroupIDForExpHeap(MEMHeapHandle *heap, u16 groupID)
{
	u16 prev;
	BOOL interrupts;

	OSAssert_Line(1792, IsValidExpHeapHandle_(heap));
	OSAssert_Line(1793, groupID <= MAX_GROUPID);

	interrupts = OSDisableInterrupts();
	{
		MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHandle_(heap);
		prev = ext->groupID;
		ext->groupID = groupID;
	}
	OSRestoreInterrupts(interrupts);
	return prev;
}

u16 MEMGetGroupIDForExpHeap(MEMHeapHandle *heap)
{
	OSAssert_Line(1819, IsValidExpHeapHandle_(heap));
	return GetExpHeapHeadPtrFromHandle_(heap)->groupID;
}

void MEMVisitAllocatedForExpHeap(MEMHeapHandle *heap,
                                 void (*visitor)(void *, MEMHeapHandle *, u32),
                                 u32 param)
{
	OSAssert_Line(1855, IsValidExpHeapHandle_(heap));
	OSAssert_Line(1856, visitor != NULL);

	LockHeap(heap);
	{
		MEMiExpBlockHead *block =
			GetExpHeapHeadPtrFromHandle_(heap)->usedBlocks.head;

		while (block)
		{
			MEMiExpBlockHead *next = block->link.next;
			(*visitor)(GetMemPtrForMBlock_(block), heap, param);
			block = next;
		}
	}
	UnlockHeap(heap);
}

u32 MEMGetSizeForMBlockExpHeap(void const *memBlock)
{
	OSAssert_Line(1884, IsValidUsedMBlock_(memBlock, NULL));
	return GetMBlockHeadCPtr_(memBlock)->size;
}

u16 MEMGetGroupIDForMBlockExpHeap(void const *memBlock)
{
	OSAssert_Line(1901, IsValidUsedMBlock_(memBlock, NULL));
	return GetGroupIDForMBlock_(GetMBlockHeadCPtr_(memBlock));
}

u16 MEMGetAllocDirForMBlockExpHeap(void const *memBlock)
{
	OSAssert_Line(1918, IsValidUsedMBlock_( memBlock, NULL ));
	return GetAllocDirForMBlock_(GetMBlockHeadCPtr_(memBlock));
}

u32 MEMAdjustExpHeap(MEMHeapHandle *heap)
{
	OSAssert_Line(1940, IsValidExpHeapHandle_(heap));

	{
		MEMiHeapHead *pHeapHd = heap;
		MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHeapHead_(heap);
		MEMiExpBlockHead *block;
		u32 newSize;

		LockHeap(heap);

		block = ext->freeBlocks.tail;
		if (block == NULL)
		{
			// No free blocks, nothing to adjust
			newSize = 0;
		}
		else
		{
			void *memBlock = GetMemPtrForMBlock_(block);
			void *memBlockEnd = AddU32ToPtr(memBlock, block->size);
			u32 blockSize;

			if (memBlockEnd != MEMGetHeapEndAddress(heap))
			{
				// There's a used block at the end of the heap, we can't adjust
				newSize = 0;
			}
			else
			{
				RemoveMBlock_(&ext->freeBlocks, block);
				blockSize = block->size + sizeof(MEMiExpBlockHead);
				pHeapHd->heapEnd = SubU32ToPtr(pHeapHd->heapEnd, blockSize);
				newSize = GetOffsetFromPtr(pHeapHd, pHeapHd->heapEnd);
			}
		}

		UnlockHeap(heap);
		return newSize;
	}
}

#ifndef NDEBUG
BOOL MEMCheckExpHeap(MEMHeapHandle *heap, u32 flag)
{
	BOOL print = (flag & 1) != 0;
	u32 totalSize = 0;
	BOOL valid;

	if (!IsValidExpHeapHandle_(heap))
	{
		MEM_HEAP_PRINT(print,
		               "[OS Foundation Exp Heap] Invalid heap handle. - %08X\n",
		               heap);
		return FALSE;
	}

	LockHeap(heap);
	// Because the heap is locked, we need to unlock it when we leave the function
	// Goto statements are preferred here to simulate unlocking the heap in the epilogue
	{
		MEMiHeapHead *pHeapHd = heap;
		MEMiExpHeapExt *ext = GetExpHeapHeadPtrFromHeapHead_(pHeapHd);
		MEMiExpBlockHead *block = NULL;
		MEMiExpBlockHead *prev = NULL;

		for (block = ext->usedBlocks.head; block;
		     prev = block, block = block->link.next)
		{
			if (!CheckUsedMBlock_(block, pHeapHd, flag)
			    || !CheckMBlockPrevPtr_(block, prev, flag))
			{
				valid = FALSE;
				goto ret;
			}

			totalSize += block->size + GetAlignmentForMBlock_(block)
			           + sizeof(MEMiExpBlockHead);
		}

		if (!CheckMBlockLinkTail_(prev, ext->usedBlocks.tail, "tail", flag))
		{
			valid = FALSE;
			goto ret;
		}

		block = NULL;
		prev = NULL;
		for (block = ext->freeBlocks.head; block;
		     prev = block, block = block->link.next)
		{
			if (!CheckFreeMBlock_(block, pHeapHd, flag)
			    || !CheckMBlockPrevPtr_(block, prev, flag))
			{
				valid = FALSE;
				goto ret;
			}

			totalSize += block->size + sizeof(MEMiExpBlockHead);
		}

		if (!CheckMBlockLinkTail_(prev, ext->freeBlocks.tail, "tail", flag))
		{
			valid = FALSE;
			goto ret;
		}

		if (totalSize != GetOffsetFromPtr(pHeapHd->heapStart, pHeapHd->heapEnd))
		{
			MEM_HEAP_PRINT(
				print,
				"[OS Foundation Exp Heap] Incorrect total memory block size. - "
				"heap size %08X, sum size %08X\n",
				GetOffsetFromPtr(pHeapHd->heapStart, pHeapHd->heapEnd),
				totalSize);
			valid = FALSE;
			goto ret;
		}

		valid = TRUE;
	}
ret:
	UnlockHeap(heap);
	return valid;
}

BOOL MEMCheckForMBlockExpHeap(void const *memBlock, MEMHeapHandle *heap,
                              u32 flag)
{
	MEMiExpBlockHead const *block = NULL;
	MEMiHeapHead *pHeapHd = heap;

	if (!memBlock)
		return FALSE;

	block = GetMBlockHeadCPtr_(memBlock);
	if (!CheckUsedMBlock_(block, pHeapHd, flag))
		return FALSE;

	if (block->link.prev)
	{
		if (!CheckUsedMBlock_(block->link.prev, pHeapHd, flag)
		    || !CheckMBlockNextPtr_(block->link.prev, block, flag))
			return FALSE;
	}
	else if (pHeapHd)
	{
		if (!CheckMBlockLinkTail_(
				block, GetExpHeapHeadPtrFromHeapHead_(pHeapHd)->usedBlocks.head,
				"head", flag))
			return FALSE;
	}

	if (block->link.next)
	{
		if (!CheckUsedMBlock_(block->link.next, pHeapHd, flag)
		    || !CheckMBlockPrevPtr_(block->link.next, block, flag))
			return FALSE;
	}
	else if (pHeapHd)
	{
		if (!CheckMBlockLinkTail_(
				block, GetExpHeapHeadPtrFromHeapHead_(pHeapHd)->usedBlocks.tail,
				"tail", flag))
			return FALSE;
	}

	return TRUE;
}
#endif
