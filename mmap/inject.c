
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include <sys/user.h>


#include "inject.h"

#include <string.h>
char *read_string(pid_t child, unsigned long addr) {


    char *val = malloc(4096);
    int allocated = 4096, read=0;
    unsigned long tmp;
    while (1) {
        if (read + sizeof tmp > allocated) {
            allocated *= 2;
            val = realloc(val, allocated);
        }

        tmp = ptrace(PTRACE_PEEKDATA, child, addr + read,NULL);
        if(errno != 0) {
            val[read] = 0;
            break;
        }

        memcpy(val + read, &tmp, sizeof tmp);
        if (memchr(&tmp, 0, sizeof tmp) != NULL)
            break;
        read += sizeof tmp;

    }
    return val;
}


/* XXX: no 64 -> 32 bit processes :( */
long inject_scode(pid_t pid,char *sc,size_t size,bit_type type,breakpoint * breaks, int int_count) {

  int status;
  int SPTR = (sizeof(void*));
  long *ptr,*ret,*pc;
  size_t bsize;
  char * buff;
  int i = 0,e=0;

  struct user_regs_struct old_regs;
  struct user_regs_struct regs;

#if  __WORDSIZE == 64
  if(type == BITS64){
    SPTR = 8;
    ret = &regs.rax;
    pc = &old_regs.rip;
  }
#else
  if (type == BITS32){
    SPTR = 4;
    pc = &old_regs.eip;
    ret = &regs.eax;
  }
#endif
  else { die("UNKOWN BIT SIZE"); }

  bsize = size % SPTR == 0 ? size : (size/SPTR+1)*SPTR;
  buff  = malloc(bsize);

  if (ptrace(PTRACE_ATTACH,pid,NULL,NULL) < 0)
    die("ptrace(ATTACH)");

  ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);
  waitpid(pid, &status, 0);

  printf("[*] saving registers\n");
  if (ptrace(PTRACE_GETREGS,pid,NULL,&old_regs)<0)
    die("ptrace(SAVEREGS)");

  // make place for shellcode...
  printf("[*] making place for shellcode\n");

  ptr = (long*) buff;
  for(i=0;i<bsize;i+=SPTR){
    *ptr = ptrace(PTRACE_PEEKTEXT,pid,*pc+i+SPTR,NULL);
    ptr++;
  }

  ptr = (long *) sc;
  printf("[+] Copy shellcode... ");
  //copy shellcode
  for(i=0;i<bsize;i+=SPTR)
    if(ptrace(PTRACE_POKETEXT,pid,*pc+i+SPTR,*ptr++) < 0)
      die("ptrace(POKE_SCODE)");
  printf("done.\n");
  fflush(stdout);
  printf("[+] Executing... ");

  ptrace(PTRACE_POKETEXT,pid,*pc,0x9090909090909090);
  if (ptrace(PTRACE_POKEUSER, pid, sizeof(long)*RIP,*pc+1)<0)
    die("SET_RIP");


  // we will switch to debugger after each syscall

  while(int_count--) {
    if (ptrace(PTRACE_SYSCALL,pid,NULL,NULL) < 0)
      die("ptrace(CONT)") ;

    waitpid(pid,&status,0);
    printf("get sig: %x\n",status);
    /* if(WSTOPSIG(status)  != SIGTRAP) */
    /*   die("uncool somthing interupted.."); */
    if(!WIFSTOPPED(status) && !(WSTOPSIG(status) & 0x80))
      die("uncool somthing interupted..");



    if (ptrace(PTRACE_GETREGS,pid,NULL,&regs)<0)
      die("ptrace(SAVEREGS)");
    printf("\nrip: %lx rax: %lx orgi_rax: %lx rcx: %lx rdx: %lx rdi: %lx rsi: %lx r8: %lx r9: %lx r10: %lx",
	   regs.rip,regs.rax,regs.orig_rax,regs.rcx,regs.rdx,regs.rdi,regs.rsi,regs.r8,regs.r9,regs.r10);
    long foo;
    if (int_count == 1) {

      printf("\nfile: %s\n",read_string(pid,regs.rdi));
    }
    if (ptrace(PTRACE_SYSCALL,pid,NULL,NULL) < 0)
      die("ptrace(CONT)") ;

    waitpid(pid,&status,0);
    if(!WIFSTOPPED(status) && !(WSTOPSIG(status) & 0x80))
      die("uncool2 somthing interupted..");

    if (ptrace(PTRACE_GETREGS,pid,NULL,&regs)<0)
      die("ptrace(SAVEREGS)");
    printf("\nrip: %lx rax: %lx orgi_rax: %lx rcx: %lx rdx: %lx rdi: %lx rsi: %lx r8: %lx r9: %lx r10: %lx",
	   regs.rip, regs.rax,regs.orig_rax,regs.rcx,regs.rdx,regs.rdi,regs.rsi,regs.r8,regs.r9,regs.r10);
    /*
    if(breaks[int_count-1].is_fault(*ret)) {
      e=1;
      break;
      }*/
  }
  if(e != 0)
    printf("Sth wrong going down..\n");
  else
    printf("\n done\n");

  // restore code
  ptr = (long *) buff;
  for(i=0;i<bsize;i+=SPTR)
    if ( ptrace(PTRACE_POKETEXT,pid,*pc+i+SPTR,*ptr++) <0)
      die("ptrace(RESOTRE)");

  // set rip and regs back
  if (ptrace(PTRACE_SETREGS,pid,NULL,&old_regs)<0)
    die("ptrace(RESOTRE_REGS)");

  if(ptrace(PTRACE_DETACH,pid,NULL,NULL))
    die("ptrace(DETACH)");

  free(buff);

  if(e !=0 ) breaks[int_count-1].report_fault(*ret);
  return *ret;
}