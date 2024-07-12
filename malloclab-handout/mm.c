/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4
#define CHUNKSIZE (1<<12)
#define PUT(p,val) (*(unsigned int *) (p) = (val))
#define GET(p) (* (unsigned int *) (p))
#define PACK(size , info) ( (size) | (info) )
#define GET_SIZE(p) ( GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_NEAR_ALLOC(p) (GET(p) & 0x2)
#define HDRP(p) ((char*) (p) - 3*WSIZE)
#define FTRP(p) ((char*) (p) + GET_SIZE(HDRP(p)) - 4*WSIZE)
#define NEXT_BLK(p) ((char *) (p) + GET_SIZE( ( (char *) (p) - 3*WSIZE)) )
#define PREV_BLK(p) ((char *) (p) - GET_SIZE( ( (char *) (p) - 4*WSIZE)))
#define NEXT_FREE_BLK(p) ((char *)(p) - WSIZE)
#define PREV_FREE_BLK(p) ((char *)(p) - 2*WSIZE)
#define MAX(x,y) ((x)>(y) ? (x) : (y))
#define MIN(x,y) ((x)<(y) ? (y) : (x))


/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


void *heap_listp;
void *free_listp;

static void AddFreeList(void *bp)
{
	PUT(PREV_FREE_BLK(bp) , 0);
	PUT(NEXT_FREE_BLK(bp) , (unsigned int)free_listp);
	if(free_listp != NULL) PUT(PREV_FREE_BLK(free_listp) , (unsigned)bp);
	free_listp = bp;
}

static void RemoveFromFreeList(void *bp)
{
	void *prev_free_blk = (void *)GET(PREV_FREE_BLK(bp));
	void *next_free_blk = (void *)GET(NEXT_FREE_BLK(bp));
	if(prev_free_blk == NULL && next_free_blk == NULL)
	{
		free_listp = NULL;
	}
	else if(prev_free_blk == NULL && next_free_blk != NULL)
	{
		free_listp = next_free_blk;
		PUT(PREV_FREE_BLK(next_free_blk) , 0);
	}
	else if(prev_free_blk != NULL && next_free_blk ==NULL)
	{
		PUT(NEXT_FREE_BLK(prev_free_blk) , 0);
	}
	else
	{
		PUT(NEXT_FREE_BLK(prev_free_blk) , (unsigned)next_free_blk);
		PUT(PREV_FREE_BLK(next_free_blk) , (unsigned)prev_free_blk);
	}
	return ;
}

static void *coalesce(void * bp)
{
	size_t prev_alloc = GET_NEAR_ALLOC(HDRP(bp));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLK(bp)));
	//printf("%d %d\n",prev_alloc , next_alloc);
	//printf("%x\n",NEXT_BLK(bp));
	if(next_alloc && prev_alloc)
	{
		PUT(HDRP(bp) , PACK(GET_SIZE(HDRP(bp)) , 0x2));
		PUT(FTRP(bp) , PACK(GET_SIZE(HDRP(bp)) , 0x2));
	}
	else if(next_alloc && !prev_alloc)
	{
		RemoveFromFreeList(PREV_BLK(bp));

		size_t newsize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(PREV_BLK(bp)));
		
		PUT(FTRP(bp) , PACK(newsize , 0x2));
		PUT(HDRP(PREV_BLK(bp)) , PACK(newsize , 0x2));

		bp = PREV_BLK(bp);
	}
	else if(!next_alloc && prev_alloc)
	{
		RemoveFromFreeList(NEXT_BLK(bp));

		size_t newsize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(NEXT_BLK(bp)));
		PUT(FTRP(NEXT_BLK(bp)) , PACK(newsize , 0x2));
		PUT(HDRP(bp) , PACK(newsize , 0x2));
	}
	else
	{
		RemoveFromFreeList(PREV_BLK(bp));
		RemoveFromFreeList(NEXT_BLK(bp));
		size_t newsize = GET_SIZE(HDRP(PREV_BLK(bp))) + GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(NEXT_BLK(bp)));
		//printf("%d %d %d\n",GET_SIZE(HDRP(PREV_BLK(bp))) , GET_SIZE(HDRP(bp)) , GET_SIZE(HDRP(NEXT_BLK(bp))));
		PUT(HDRP(PREV_BLK(bp)) , PACK(newsize , 0x2));
		PUT(FTRP(NEXT_BLK(bp)) , PACK(newsize , 0x2));
		bp = PREV_BLK(bp);
	}
	//printf("%d %x\n",GET_ALLOC(HDRP(NEXT_BLK(bp))),NEXT_BLK(bp));
	//printf("%x %x %x\n",mem_heap_lo() , bp , mem_heap_hi());
	AddFreeList(bp);
	return bp;
}


static void *ExtendHeap(size_t size)
{
	char *new_heap_listp;
	size = ALIGN(size + WSIZE);
	if( (int) (new_heap_listp = mem_sbrk(size)) == -1) 
	{
		printf("Error!\n");
		return NULL;
	}
	new_heap_listp += 2*WSIZE;
	size_t prev_alloc = GET_NEAR_ALLOC(new_heap_listp - 3*WSIZE);
	PUT(HDRP(new_heap_listp) , PACK(size , prev_alloc));
	PUT(FTRP(new_heap_listp) , PACK(size , prev_alloc));
	PUT(PREV_FREE_BLK(new_heap_listp) , 0);
	PUT(NEXT_FREE_BLK(new_heap_listp) , 0);
	PUT(FTRP(new_heap_listp) + WSIZE , PACK(0,1));
	return (void *) coalesce(new_heap_listp);
	//return new_heap_listp;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	//printf("mm_init start oo\n");
	if((heap_listp = mem_sbrk(6*WSIZE)) == (void *) -1) return -1;
	PUT(heap_listp + WSIZE , PACK(4 * WSIZE , 0x1));
	PUT(heap_listp + 2*WSIZE , 0);
	PUT(heap_listp + 3*WSIZE , 0);
	PUT(heap_listp + 4*WSIZE , PACK(4*WSIZE , 0x1));
	PUT(heap_listp + 5*WSIZE , PACK(0 , 0x3));
	heap_listp += 4*WSIZE;
	free_listp = NULL;
	//printf("Before ExtendHeap\n");
	if(ExtendHeap(CHUNKSIZE) == NULL) return -1;
	//printf("mm_init complete\n");
    return 0;
}

static void CutBlock(void *bp , size_t size)
{
	size_t nowsize = GET_SIZE(HDRP(bp));
	if(nowsize - size <= 4*WSIZE) 
	{
		PUT(HDRP(bp) , PACK(nowsize , 0x3));
		PUT(FTRP(bp) , PACK(nowsize , 0x3));
		PUT(HDRP(NEXT_BLK(bp)) , PACK(GET(HDRP(NEXT_BLK(bp))) , 0x2));

		RemoveFromFreeList(bp);
		return ;
	}
	assert(bp <= mem_heap_hi());
	PUT(HDRP(bp) , PACK(size , 0x3));
	//printf("HERE %x\n",PREV_FREE_BLK(bp));

	RemoveFromFreeList(bp);

	PUT(HDRP(NEXT_BLK(bp)) , PACK(nowsize - size , 0x2));
	PUT(FTRP(NEXT_BLK(bp)) , PACK(nowsize - size , 0x2));

	AddFreeList(NEXT_BLK(bp));
	return ;
}

void *FindList(size_t size)
{
	for(void *bp = free_listp ; bp != NULL ; bp = (void *)GET(NEXT_FREE_BLK(bp)))
	{
		assert(GET_ALLOC(HDRP(bp)) == 0);
		if(GET_SIZE(HDRP(bp)) >= size)
		{
			return bp;
		}
	}
	return NULL;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	//printf("malloc start %x\n",free_listp);
	if(size <= 0) return NULL;
	void *mp = NULL;
	size = ALIGN(size + 4*WSIZE);
	mp = FindList(size);
	//printf("mp : %x\n" , mp);
	if(mp == NULL)
	{
		mp=ExtendHeap(MAX(CHUNKSIZE , size));
		//printf("Extend %x\n",mp);
		if(mp == NULL) return NULL;
	}
	//assert(GET_ALLOC(HDRP(mp)) == 0);
	CutBlock(mp , size);
	//printf("malloc end\n");
	//printf("%x %d\n",mp,GET_NEAR_ALLOC(HDRP(mp)));
	return mp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	//printf("free start %x\n",ptr);
	if(ptr == NULL || ptr <= heap_listp || ptr >= mem_heap_hi()) return ;
	size_t size = GET_SIZE(HDRP(ptr));
	assert(GET_ALLOC(HDRP(ptr)) == 1);

	PUT(HDRP(ptr) , PACK( PACK(size , GET_NEAR_ALLOC(HDRP(ptr)))  , 0x0));
	PUT(FTRP(ptr) , PACK( PACK(size , GET_NEAR_ALLOC(HDRP(ptr))) , 0x0));
	ptr = coalesce(ptr);
	size = GET_SIZE(HDRP(NEXT_BLK(ptr)));
	assert(GET_ALLOC(HDRP(NEXT_BLK(ptr))) == 1);
	PUT(HDRP(NEXT_BLK(ptr)) , PACK(size , 0x1));
	//printf("free end\n");
	return ;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	if(ptr == NULL) return mm_malloc(size);
	if(size == 0)
	{
		mm_free(ptr);
		return NULL;
	}
	size_t nowsize = GET_SIZE(HDRP(ptr));
	void *newp = mm_malloc(size);
	if(newp == NULL)
	{
		printf("Error in mm_realloc\n");
		return NULL;
	}
	
	nowsize = GET_SIZE(HDRP(newp));
	//nowsize = GET_SIZE(HDRP(newp));
	size_t copy_size = MIN(size , nowsize-3*WSIZE ) ;
	memcpy(newp , ptr , copy_size);
	mm_free(ptr);
	return newp;
}















