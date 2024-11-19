#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "wmap.h"
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
      uint fault_addr = rcr2();
      fault_addr = PGROUNDDOWN(fault_addr);
      struct proc *p = myproc();
      int mapped = 0;

      pte_t* pte = get_pte(p->pgdir, (void *)fault_addr);

      if(pte!=0){
        uint pa = PTE_ADDR(*pte);

        if(get_ref_count(pa)==0){
            pte = 0;
        }
      }
    
    if(pte != 0) { 
        uint pa = PTE_ADDR(*pte);
        //allowed to write
        if(*pte & PTE_COW) {
          // PAGE can be written
          uint ref_cnt = get_ref_count(pa);
          if(ref_cnt == 1) {
            *pte|=PTE_W;

          } else if(ref_cnt > 1) {
              char *mem;
              *pte = 0;

            if((mem = kalloc()) == 0) {
              cprintf("Could not allocate memory");
            }
          
            memmove(mem, (char*)P2V(pa), PGSIZE);
            perform_mapping(p->pgdir, (char*)fault_addr, PGSIZE, V2P(mem), PTE_W | PTE_U);
            decrement_ref_count(pa);
            lcr3(V2P(p->pgdir));
          }
        } else {
          cprintf("Segmentation Fault\n");
          exit();
        }
    } else {
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
