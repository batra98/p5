// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "kalloc.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// A free page. (Page is a just of chunk of storage in bits ex: 4KB chunk of 1's and 0's)
struct run {
  struct run *next;
};

#define MAX_PHYSICAL_PAGES 1<<20 //(1Mi pages)

// Define an array of 1Mi pages for the Physical Frame Numbers. (physical pages)
unsigned char refCounts[MAX_PHYSICAL_PAGES] = {0};

uint get_physical_page(void * va){
  char* p = (char*)PGROUNDDOWN((uint)va); // represents the virtual page cointaining the virtual address
  if((uint)p % PGSIZE || p < end || V2P(p) >= PHYSTOP)
    panic("Virtual address out of range while incrementing refCount");
  return V2P(p);
}

// The caller needs to hold the lock for kmem before trying to increase refCount.
void increase_ref_count(void* va){
  uint physical_page = get_physical_page(va);
  refCounts[physical_page]++;
}

void increase_ref_count_physical_page(uint pa){
  acquire(&kmem.lock);
  refCounts[pa]++;
  release(&kmem.lock);
}

void decrease_ref_count(void *va){
  uint physical_page = get_physical_page(va);
  refCounts[physical_page]--;
}

// void get_reference_count(void* va){
//   uint physical_page = get_physical_page(va);
//   unsigned char refCount;
  
//   if(kmem.use_lock)
//     acquire(&kmem.lock);
  
//   refCount = refCounts[physical_page];
  
//   if(kmem.use_lock)
//     release(&kmem.lock);
//   return refCount;
// }

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  r = (struct run*)v;

  // Now we can release the memory and add it to the free list.
  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);

  uint physical_page_no = V2P(v);
  refCounts[physical_page_no]--;

  if(refCounts[physical_page_no] > 0){
    if(kmem.use_lock)
      release(&kmem.lock);
    return;
  }
  if(refCounts[physical_page_no] < 0){
    if(kmem.use_lock)
      release(&kmem.lock);
    panic("kfree: Trying to free an already freed page");
  }
  
  r->next = kmem.freelist; // Insert at the top of linked list containing free pages.
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next; // Assign a free page from the top of linked list containing free pages.  (Pop operation)
    increase_ref_count(r);
  }
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

// todo: when the page is again referenced, we need to update refCount 
//         -> how to do this? (When doing copy on write, just update the reference Count! Voila!)
