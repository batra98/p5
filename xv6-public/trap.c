#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "fs.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"


// Interrupt descriptor table (shared by all CPUs).
//
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_PGFLT: {
      uint fault_addr = rcr2(); //virtual address which is faulting.
      struct proc *p = myproc();
      int mapped = 0;
      int is_handle_mmap_fault = 0;
      cprintf("Page fault at %p\n", rcr2()); 

      // calculate pte from fault address.
      pte_t* pte = get_pte(p->pgdir, (void*)fault_addr);
      
      if(!pte){
        cprintf("No page table entry found, can't be because of COW\n");
        is_handle_mmap_fault = 1;
      }
      
      if(is_handle_mmap_fault){
        for (int i = 0; i < p->mmap_count; i++) {
          struct mmap_region *region = &p->mmap_regions[i];
          if (fault_addr >= region->addr && fault_addr < (region->addr + region->length)) {
            char *mem = kalloc();
            if (mem == 0) {
                cprintf("Out of memory!\n");
                kill(p->pid);
                break;
            }
            
            memset(mem, 0, PGSIZE);

            if (!(region->flags & MAP_ANONYMOUS) && region->fd != 0) {
              
              if (region->file == 0 || region->file->type != FD_INODE) {
                cprintf("Invalid file descriptor for memory mapping\n");
                kfree(mem);
                kill(p->pid);
                break;
              }

              int offset = fault_addr - region->addr;
              ilock(region->file->ip);
              int bytes_read = readi(region->file->ip, mem, offset, PGSIZE);
              iunlock(region->file->ip);

              if (bytes_read < 0) {
                 cprintf("File read failed at address: %p\n", fault_addr);
                 kfree(mem);
                 kill(p->pid);
                 break;
              }

            }
            
            int result = perform_mapping(p->pgdir, (void*)fault_addr, PGSIZE, V2P(mem), PTE_W | PTE_U);            
            if (result != 0) {
              cprintf("Mapping failed for address: %p\n", fault_addr);
            }
            mapped = 1;
            break;
          }
        }
        if (!mapped) {
          cprintf("Segmentation Fault\n");
          kill(p->pid);
        }
        break; // If it because of lazy allocation, return here and don't run COW handler code.
      }
      // Handler for COW page fault
      cprintf("Now Handle COW write fault checks ---------> \n");
      if(!(*pte & PTE_P)){
        cprintf("PTE Doesn't exist - Page fault \n");
        kill(p->pid);
      }
      // Edge case to handle if user doesn't have access to the page.
      if(!(*pte & PTE_U)){
        cprintf("User doesn't have access to the page.");
        kill(p->pid);
      }
      // if write is not set and copy on write is set
      if(!(*pte & PTE_W) && (*pte & PTE_COW)){
        cprintf("Inside COW \n");
        // the page is write-able. Create a new page and decrease refCount of the page.
        char* mem = kalloc();
        if(mem == 0){
          // New physical memory page allocation failed
          kfree(mem);
          kill(p->pid);
        }
        uint pa = PTE_ADDR(*pte);
        uint flags = PTE_FLAGS(*pte);
        flags &= ~PTE_COW; // Unset COW
        flags |= PTE_W; // Set write bit

        memmove(mem, (char*)P2V(pa), PGSIZE);
        char* page = (char*)PGROUNDDOWN(fault_addr);
        // Map the pages
        if(perform_mapping(p->pgdir, (void*)page, PGSIZE, V2P(mem), flags) < 0){
          // todo: Do we need to reset the flags?
          cprintf("todo: reset flags");
          kfree(mem);
          kill(p->pid);
        }
        kfree(P2V(pa));
        lcr3(V2P(p->pgdir)); // reload the page table of current process.

        // todo:optimization: if the existing reference Count is 1, then don't have to allocate new memory page,
        // just reset W=1 and COW=0 flag.
      }

      break;
  }

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
