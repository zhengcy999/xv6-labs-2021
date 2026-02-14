#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"


uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
uint64
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 va;
  int pages;
  uint64 u_address;
  if (argaddr(0,&va)<0) return -1;
  if (argint(1,&pages)<0) return -1;
  if (argaddr(2,&u_address)<0) return -1;
  if (pages < 0 || pages > 32) return -1;
  struct proc *p=myproc();
  uint abits=0;  //掩码 bitmask
  //对齐页边界
  uint64 a = PGROUNDDOWN(va);
  for(int i=0;i<pages;i++){
    uint64 curr=a+(uint64)i*PGSIZE;
    pte_t *pte=walk(p->pagetable,curr,0);
    if (pte==0) return -1;
    if ((*pte &PTE_V)==0) return -1;
    if (((*pte) & (PTE_R | PTE_W | PTE_X)) == 0) continue;
    if (*pte &PTE_A){
      abits |= (1U << i);
      *pte &=~PTE_A;
    }
  }
  //刷新tlb
  sfence_vma();
  // copyout 
  if(copyout(p->pagetable, u_address, (char *)&abits,sizeof(abits)) < 0)
    return -1;
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
