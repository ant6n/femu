/** 
 Helper library for the emu, which doesn't use the cstdlib
*/
#ifndef __emu_lib__
#define __emu_lib__

#include <fcntl.h>
#include <sys/stat.h>
/*
#define O_RDONLY 0x00000
#define O_WRONLY 0x00001
#define O_RDWR   0x00002
#define O_CREAT  0x00100
#define O_TRUNC  0x01000

#define S_IRWXU  0x00700
#define S_IRUSR  0x00400
#define S_IWUSR  0x00200
#define S_IXUSR  0x00100

#define S_IRWXG  0x00070
#define S_IRGRP  0x00040
#define S_IWGRP  0x00020
#define S_IXGRP  0x00010

#define S_IRWXO  0x00007
#define S_IROTH  0x00004
#define S_IWOTH  0x00002
#define S_IXOTH  0x00001
*/

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

