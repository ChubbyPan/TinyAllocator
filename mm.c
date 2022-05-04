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
    "Panny",
    /* First member's full name */
    "Panny",
    /* First member's email address */
    "rollypanny@gmail.com",
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


#define WSIZE 4  //word and header , footer sizes
#define DSIZE 8 // double word sizes
#define CHUNKSIZE (1<<12) //每次heap不够大,扩容4kB

#define MAX(x, y) ((x) > (y) ? (x) : (y))

//将块大小和末尾的alloc标记位打包进一个word
#define PACK(size, alloc) ((size) | (alloc)) 

// read and write a word at add P
#define GET(p) (*(unsigned int *) (p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// read the size and allocated fields from add P
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// given block ptr bp, compute add of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// given block ptr bp, compute add of next and previous blocks
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char* )(bp) - DSIZE)))




// heap 初地址
static char *heap_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *first_fit(size_t asize);
static void place(void *bp, size_t asize);


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char *bp;
    // 初始化一个空堆,堆内需要有prologue block(header+footer) 和epilogue block header
    // 当前堆头不足存放header和footer， 初始化失败
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) return -1;
    printf("\n init heap address: %p \n", heap_listp);
    fflush(stdout);
    PUT(heap_listp, 0);  // alingment padding
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1)); // prologue block header
    PUT(heap_listp + DSIZE, PACK(DSIZE, 1)); // prologue block footer
    PUT(heap_listp + 3 * WSIZE, PACK(0,1)); // epilogue block footer
    heap_listp += DSIZE;
    printf("after prologue blcok, heaplistp at: %p \n", heap_listp);
    fflush(stdout);
    // 填充空堆，空堆需要chunksize大小
    if ((bp = extend_heap(CHUNKSIZE/WSIZE)) == NULL) return -1;
    else{
        printf("fisrt extend heap, now use heap at address: %p \n", bp);
        fflush(stdout);
    }
    return 0;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // if size <= 0 ,ignore invalid request
    // adjust size to align
    // search the free list for a fit(first fit)
    // no fit found , get more memory and place the block
    size_t asize;
    size_t extendsize;
    char *bp;
    if (size <= 0) return NULL;
    // minimun block size == 16 bytes, header and footer both 4 bytes, atleast 8 bytes for malloc(alignment)
    if (size <= DSIZE) asize = 2 * DSIZE; 
    else { 
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE );
    }
    
    // first fit succeed
    if ((bp = first_fit(asize)) != NULL){
        printf("fit, at address %p, size:%zx\n", bp, asize);
        fflush(stdout);
        place(bp, asize);
        return bp;
    }
    printf("not fit, need more memory\n");
    fflush(stdout);
    // no fit found, get more memory 
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL ) return NULL;
    printf("have extend heap, continue bp address: %p, now totally size: %p\n", bp, mem_heap_hi());
    fflush(stdout);
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    // modify header and footer 
    // coalescing if possible
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}


static void *coalesce(void *bp){
    // 分四种情况对free block进行coalescing；
    // case 1： prev and next都不free
    // case 2： prev free ， next 不free
    // case 3： prev 不free， next free
    // case 4： prev and next 都 free
    // find prev and next blocks' alloc bit
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // get cur block size
    size_t size = GET_SIZE(HDRP(bp));
    // case 1:
    if (prev_alloc && next_alloc) return bp;
    // case 2:
    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp),PACK(size, 0)); // update cur block's ftr 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // update prev block's header
        bp = PREV_BLKP(bp);
    }
    // case 3:
    else if (prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0)); //update cur block's hdr
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // update next block's ftr
    }
    // case 4:
    else{
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // update prev block's hdr
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // update next block's ftr
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;
    // allocate an even number of words (alignment)
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == (void *)-1) return NULL; // out of max heap range 
    // init whole extend heap as a free block 
    PUT(HDRP(bp), PACK(size, 0)); // header
    PUT(FTRP(bp), PACK(size, 0)); // footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); // update new epilogue block header
    // if previous block free, coalesce;
    printf("have extended succeed , extend size: %zx \n", size);
    fflush(stdout);
    return coalesce(bp);
}

static void *first_fit(size_t asize){
    char *bp;
    bp = heap_listp + 2 * WSIZE; //traverse from head
    while(GET_SIZE(HDRP(bp)) > 0 ){ //if touch epilogue block header , stops
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize )
            return bp;
        bp = NEXT_BLKP(bp);
    }
    // not fit
    return NULL;
}

static void *next_fit(size_t asize){

}

static void *best_fit(size_t asize){

}

static void *seg_fit(size_t asize){
    
}

static void place(void *bp, size_t asize){
    // place free block
    // modify header, if need split, add footer
    // if need split, add header, modify footer
    size_t csize = GET_SIZE(HDRP(bp));
    if((csize - asize) >= (2 * DSIZE)){ // can split
        printf("when add at:%p, size: %zx , can split\n", bp, asize);
        fflush(stdout);
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK((csize - asize), 0));
        PUT(FTRP(bp), PACK((csize - asize), 0));
    }
    else{// can't split
        printf("when add at:%p, size: %zx , can not split\n", bp, asize);
        fflush(stdout);
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














