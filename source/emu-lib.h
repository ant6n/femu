/** 
 Helper library for the emu, which doesn't use the cstdlib
*/
#ifndef __emu_lib__
#define __emu_lib__

#include <fcntl.h>
#include <sys/stat.h>

/* ensure shift left by register is available, even if gcc doesn't prove it (e.g. '-static' omits libgcc) */
static inline int shiftRight(unsigned int value, int shift) {
  int result;
  asm volatile("mov %[result], %[value], lsr %[shift]" :
	       [result] "=r" (result) : [value] "r" (value), [shift] "r" (shift));
  return result;
}


int syscall0(int code);
int syscall1(int code, int arg0);
int syscall2(int code, int arg0, int arg1);
int syscall3(int code, int arg0, int arg1, int arg2);
int syscall4(int code, int arg0, int arg1, int arg2, int arg3);


int write(int file, const char* s, int numChars);
int Open(const char *filename, int flags, int mode);
int close(int fd);
void exit(int errorCode) __attribute__((noreturn));

void writeHexByte(int f, char c);
void writeHexWord(int f, unsigned int w);

int strlen(const char* s);

void fprints(int f, const char* s);
void fprintx(int f, unsigned int w); // alias for writeHexWord


#endif /* defined(__emu_lib__) */

