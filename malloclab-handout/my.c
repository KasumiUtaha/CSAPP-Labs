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
#define HDRP(p) ((char*) (p) - WSIZE)
#define FTRP(p) ((char*) (p) + GET_SIZE(HDRP(p)) - ALIGNMENT)
#define NEXT_BLK(p) ((char *) (p) + GET_SIZE( ( (char *) (p) - WSIZE)) )
#define PREV_BLK(p) ((char *) (p) - GET_SIZE( ( (char *) (p) - ALIGNMENT)))
#define MAX(x,y) ((x)>(y) ? (x) : (y))
#define MIN(x,y) ((x)<(y) ? (y) : (x))


/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


void *heap_listp;


static void *coalesce(void * bp)
{
	size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLK(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLK(bp)));
	if(prev_alloc && next_alloc) return bp;
	if(!prev_alloc)
	{
		size_t newsize =  GET_SIZE(HDRP(PREV_BLK(bp))) + GET_SIZE(HDRP(bp));
		PUT(HDRP(PREV_BLK(bp)) , newsize);
		PUT(FTRP(bp) , newsize);
		bp = PREV_BLK(bp);
	}
	if(!next_alloc)
	{
		size_t newsize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(NEXT_BLK(bp)));
		PUT(FTRP(NEXT_BLK(bp)) , newsize);
		PUT(HDRP(bp) , newsize);
	}
	//printf("%x %x %x\n",mem_heap_lo() , bp , mem_heap_hi());
	return bp;
}


static void *ExtendHeap(size_t size)
{
	char *new_heap_listp;
	size = ALIGN(size);
	if( (int) (new_heap_listp = mem_sbrk(size)) == -1) return NULL;
	PUT(HDRP(new_heap_listp) , PACK(size , 0));
	PUT(FTRP(new_heap_listp) , PACK(size , 0));
	PUT(HDRP(NEXT_BLK(new_heap_listp)) , PACK(0,1));
	return (void *) coalesce(new_heap_listp);
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	//printf("mm_init start oo\n");
	if((heap_listp = mem_sbrk(4*WSIZE)) == (void *) -1) return -1;
	PUT(heap_listp + WSIZE , PACK(ALIGNMENT , 1));
	PUT(heap_listp + 2*WSIZE , PACK(ALIGNMENT , 1));
	PUT(heap_listp + 3*WSIZE , PACK(0 , 1));
	heap_listp += 2*WSIZE;
	if(ExtendHeap(CHUNKSIZE) == NULL) return -1;
	//printf("mm_init complete\n");
    return 0;
}

static void CutBlock(void *bp , size_t size)
{
	size_t nowsize = GET_SIZE(HDRP(bp));
	if(nowsize == size) 
	{
		PUT(HDRP(bp) , PACK(size , 0x1));
		PUT(FTRP(bp) , PACK(size , 0x1));
		return ;
	}
	PUT(HDRP(bp) , PACK(size , 0x1));
	PUT(FTRP(bp) , PACK(size , 0x1));
	PUT(HDRP(NEXT_BLK(bp)) , nowsize - size);
	PUT(FTRP(NEXT_BLK(bp)) , nowsize - size);
	return ;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	if(size <= 0) return NULL;
	char *mp = NULL;
	char *bp = heap_listp;
	size = ALIGN(size + 2);
	while(1)
	{
		if(GET_SIZE(HDRP(bp)) == 0 )
		break;
		assert(bp < mem_heap_hi());
		if((GET_SIZE(HDRP(bp)) >= size) && (GET_ALLOC(HDRP(bp)) == 0))
		{
			mp = bp;
			break;
		}
		bp = NEXT_BLK(bp);
	}
	if(mp == NULL)
	{
		mp=ExtendHeap(MAX(size , CHUNKSIZE));
		if(mp == NULL) return NULL;
	}
	assert(GET_ALLOC(HDRP(mp)) == 0);
	CutBlock(mp , size);
	return mp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	if(ptr == NULL || ptr <= heap_listp || ptr >= mem_heap_hi()) return ;
	size_t size = GET_SIZE(HDRP(ptr));
	assert(GET_ALLOC(HDRP(ptr)) == 1);
	PUT(HDRP(ptr) , PACK(size , 0x0));
	PUT(FTRP(ptr) , PACK(size , 0x0));
		
	coalesce(ptr);
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
	size = ALIGN(size+2);
	
	void *newp = mm_malloc(size);
	assert(GET_ALLOC(HDRP(newp)) == 0);
	if(newp == NULL) return NULL;
	CutBlock(newp , size);
	size_t copy_size = MIN(size , nowsize);
	memcpy(HDRP(newp) , HDRP(ptr) , copy_size);
	PUT(HDRP(newp) , PACK(size , 0x1));
	PUT(FTRP(newp) , PACK(size , 0x1));
	mm_free(ptr);
	return newp;
}















