#include "inject.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

void mmap_report(long ret) {
  errno = 0 - ret;
  perror("mmap(): ");
  exit(1);
}

void open_report(long ret) {
  errno = 0 - ret;
  perror("open(): ");
  exit(1);
}

void close_report(long ret) {
  errno = 0 - ret;
  perror("close(): ");
  exit(1);
}


breakpoint mmap_break = {
  .name = "break after mmap",
  .is_fault = generic_fault,
  .report_fault = mmap_report,
};

breakpoint open_break = {
  .name = "break after mmap",
  .is_fault = generic_fault,
  .report_fault = open_report,
};

breakpoint close_break = {
  .name = "break after close",
  .is_fault = generic_fault,
  .report_fault = close_report,
};

breakpoint end_break = {
  .name = "break for read mmaped address",
  .is_fault = generic_fault,
  .report_fault = NULL,
};


long ptrace_mmapfd(pid_t pid,char *path)
{
  size_t size;
  struct stat sb;
  long *ptr,ret;

  char sc64[] =
    "\xeb\x45"
    "\x5f"
    "\x48\x31\xd2"
    "\x48\x31\xf6"
    "\xb8\x02\x00\x00\x00"
    "\x0f\x05"
    "\xcc"
    "\x49\x89\xc0"
    "\x48\xbe\x41\x41\x41\x41\x41\x41\x41\x00"
    "\x4d\x31\xc9"
    "\x48\x31\xff"
    "\xba\x05\x00\x00\x00"
    "\xb9\x02\x00\x00\x00"
    "\x49\x89\xca"
    "\xb8\x09\x00\x00\x00"
    "\x0f\x05"
    "\xcc"
    "\x50"
    "\x4c\x89\xc7"
    "\xb8\x03\x00\x00\x00"
    "\x0f\x05"
    "\xcc"
    "\x58"
    "\xcc"
    "\xe8\xb6\xff\xff\xff";


  char sc32[] = "";

  char *sce;
  bit_type type = get_type(pid);
  char *sc =  type == BITS32 ? sc32 : sc64;
  size_t scsize = type == BITS32 ? sizeof sc32 : sizeof sc64;
  breakpoint breaks[] = { end_break,close_break,mmap_break,open_break};

  int fd = open(path,0,0);
  fstat(fd,&sb);
  close(fd);
  printf("[*] Prepering shellcode for mmaping %s (size: %lu)\n",path,sb.st_size);
  printf("[*] Attemt to inject read_mmap shellcode (size: %lu)\n",scsize);


  // replace dumy
  ptr  = (long*) memchr(sc,0x41,scsize);
  *ptr = (long) sb.st_size;

  sce = malloc(scsize + strlen(path) + 1);
  memcpy(sce,sc,scsize);
  memcpy(sce+scsize-1,path,strlen(path));
  sce[scsize+strlen(path)] = '\x00';

  /** inject shellcode for mapping **/

  /*  64bit            32bit
                  jmp end
                  begin:   |
     pop rdi ; file_path
     xor rdx,rdx
     xor rsi,rsi
     mov eax,0x2
     syscall
                  int3

     mov r8, rax
     mov rsi, size         |  xor ebp, ebp
     xor rdi,rdi           |  xor edi, edi
     xor r9,r9             |  mov esi, 0x2
     mov edx,0x5           |  mov edx, 0x5
     mov ecx,0x2           |  mov ecx, x->size
     mov r10,rcx           |  mov ebx, x->addr
     mov eax,0x09          |  mov eax, 0xc0
     syscall               |  int 80h | call gs:0x10
     mov rdi, r8
     mov eax, 0x3
     syscall
                  int3
		  call begin
		  /path/to/library.so\x00
  */
  ret = inject_scode(pid,sce,scsize+strlen(path),type,breaks,4);
  free(sce);
  return ret;

}
