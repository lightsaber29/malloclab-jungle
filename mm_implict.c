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
    "team 7",
    /* First member's full name */
    "Choi subin",
    /* First member's email address */
    "lightsaber2929@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/************************************************************************/
/* Basic constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
/************************************************************************/

/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */

/* declair prototype */
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp, size_t asize);

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        return -1;
    }
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2*WSIZE);
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}


void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* Case 1 */
    if (prev_alloc && next_alloc) {
        return bp;
    /* Case 2 */
    } else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    /* Case 3 */
    } else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    /* Case 4 */
    } else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}


void *mm_malloc(size_t size) {
    size_t asize; /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0) {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) {
        asize = 2*DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
        return NULL;
    }

    place(bp, asize);
    return bp;
}

/**
 * find_fit - 묵시적 가용 리스트에서 first fit 검색을 수행하는 함수
 */
static void *find_fit(size_t size) {
    void *bp;
    /*
        first fit
        가용 리스트를 처음부터 검색해서 크기가 맞는 첫 번째 가용 블록을 선택한다
    */
    // 가용 리스트를 찾는다
    // 처음부터 for 문 돌려가면서 찾는다
    // bp 를 처음 위치에 두고 -> heap_listp
    // size 까지 증가시킴 -> 사이즈를 어케 가져오지 헤더?
    // 그리고 그 다음에는 다음 블럭을 선택한다
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        // 크기가 맞고, 할당 안 되어있으면 담기
        if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }

    return NULL;
}

/**
 * place - 요청한 블록을 가용 블록의 시작 부분에 배치해야하며
 *         나머지 부분의 크기가 최소 블록 크기와 같거나 큰 경우에만 분할함
 * params: 위치, 할당할 크기
 */
static void place(void *bp, size_t asize) {
    // 일단 현재 블럭 사이즈부터 가져오고
    size_t current_size = GET_SIZE(HDRP(bp));

    // 현재 블록 사이즈랑 요청한 블록 사이즈랑 비교해서 현재 블록 사이즈가 더 크면 분할
    if (current_size > asize) {
    // if ((current_size - asize) > 2*DSIZE) {
        PUT(HDRP(bp), PACK(asize, 1)); // 헤더에 asize만큼 넣고 할당처리
        PUT(FTRP(bp), PACK(asize, 1)); // 푸터에 asize만큼 넣고 할당처리
        // 분할한 다음 위치로 이동
        bp = NEXT_BLKP(bp);
        // 쪼갠 나머지는 할당 가능하다고 처리...해야하는데 어케하지
        PUT(HDRP(bp), PACK(current_size-asize, 0));
        PUT(FTRP(bp), PACK(current_size-asize, 0));
    // 요청한 사이즈와 가용 사이즈가 동일할 때
    } else {
        PUT(HDRP(bp), PACK(current_size, 1)); // 헤더에 current_size 넣고 할당처리
        PUT(FTRP(bp), PACK(current_size, 1)); // 푸터에 current_size 넣고 할당처리
    }
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
// void *mm_malloc(size_t size)
// {
//     int newsize = ALIGN(size + SIZE_T_SIZE);
//     void *p = mem_sbrk(newsize);
//     if (p == (void *)-1)
// 	return NULL;
//     else {
//         *(size_t *)p = size;
//         return (void *)((char *)p + SIZE_T_SIZE);
//     }
// }

/*
 * mm_free - Freeing a block does nothing.
 */
// void mm_free(void *ptr)
// {
// }

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (size <= 0) { 
        mm_free(ptr);
        return 0;
    }
    if (ptr == NULL) {
        return mm_malloc(size); 
    }

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);

    if (newptr == NULL) {
        return NULL;
    }

    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(ptr));

    if (size < copySize) {
        copySize = size;
    }

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














