// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
struct {
	struct spinlock lock;
	int cnt[PHYSTOP / PGSIZE];
} page_ref_cnt;

void
add_pgcnt(uint64 pa, int val)
{
	acquire(&page_ref_cnt.lock);
	page_ref_cnt.cnt[pa/PGSIZE]+=val;
	release(&page_ref_cnt.lock);
}

int
get_pgcnt(uint64 pa)
{
	int ret;
	acquire(&page_ref_cnt.lock);
	ret = page_ref_cnt.cnt[pa/PGSIZE];
	release(&page_ref_cnt.lock);
	return ret;
}

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&page_ref_cnt.lock, "page_ref_cnt");
  initlock(&kmem.lock, "kmem");

  acquire(&page_ref_cnt.lock);
  for(int i=0; i<PHYSTOP/PGSIZE; i++)
	page_ref_cnt.cnt[i] = 1;
  release(&page_ref_cnt.lock);
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if(get_pgcnt((uint64)pa) <= 0)
  {
	  printf("%p %d\n", (uint64)pa, get_pgcnt((uint64)pa));
	  panic("kfree: page_ref_cnt<=0");
  } 
  /*if((get_pgcnt((uint64)pa)) > 1)
  {
	 add_pgcnt((uint64)pa, -1);
	 return;
  }	*/
 // 順序顛倒也爛 
//	 add_pgcnt((uint64)pa, -1);
// if(get_pgcnt((uint64)pa) > 0)
//	  return;
// 沒鎖沒事 有鎖爆炸
  page_ref_cnt.cnt[(uint64)pa/PGSIZE]--;
	if(page_ref_cnt.cnt[(uint64)pa/PGSIZE]>0)
		return;
	// panic: acquire
/*  acquire(&page_ref_cnt.lock);
    page_ref_cnt.cnt[(uint64)pa/PGSIZE]--;
	if(page_ref_cnt.cnt[(uint64)pa/PGSIZE]>0)
		return;
	release(&page_ref_cnt.lock);
	*/
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
  {
    memset((char*)r, 5, PGSIZE); // fill with junk
	acquire(&page_ref_cnt.lock);
	page_ref_cnt.cnt[(uint64)r/PGSIZE] = 1;
	release(&page_ref_cnt.lock);
	
  }
  return (void*)r;
}
