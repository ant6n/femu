#include "emu-lib.h"

/****** GENERIC SYSCALLS ****************************************************************/
int syscall0(int code) {
    int result;
    asm volatile("mov r7, %[code]\n"
                 "svc #0\n"
                 "mov %[result], r0\n"
                 : [result] "=r" (result)
                 : [code] "r" (code)
                 : "r0", "memory"
                 );
    return result;
}

int syscall1(int code, int arg0) {
    int result;
    asm volatile("mov r0, %[arg0]\n"
                 "mov r7, %[code]\n"
                 "svc #0\n"
                 "mov %[result], r0\n"
                 : [result] "=r" (result)
                 : [code] "r" (code), [arg0] "r" (arg0)
                 : "r0", "r7", "memory"
                 );
    return result;
}

int syscall2(int code, int arg0, int arg1) {
    int result;
    asm volatile("mov r0, %[arg0]\n"
                 "mov r1, %[arg1]\n"
                 "mov r7, %[code]\n"
                 "svc #0\n"
                 "mov %[result], r0\n"
                 : [result] "=r" (result)
                 : [code] "r" (code), [arg0] "r" (arg0), [arg1] "r" (arg1)
                 : "r0", "r1", "r7", "memory"
                 );
    return result;
}

int syscall3(int code, int arg0, int arg1, int arg2) {
    int result;
    asm volatile("mov r0, %[arg0]\n"
                 "mov r1, %[arg1]\n"
                 "mov r2, %[arg2]\n"
                 "mov r7, %[code]\n"
                 "svc #0\n"
                 "mov %[result], r0\n"
                 : [result] "=r" (result)
                 : [code] "r" (code), [arg0] "r" (arg0), [arg1] "r" (arg1), [arg2] "r" (arg2)
                 : "r0", "r1", "r2", "r7", "memory"
                 );
    return result;
}

int syscall4(int code, int arg0, int arg1, int arg2, int arg3) {
    int result;
    asm volatile("mov r0, %[arg0]\n"
                 "mov r1, %[arg1]\n"
                 "mov r2, %[arg2]\n"
                 "mov r3, %[arg3]\n"
                 "mov r7, %[code]\n"
                 "svc #0\n"
                 "mov %[result], r0\n"
                 : [result] "=r" (result)
                 : [code] "r" (code), [arg0] "r" (arg0), [arg1] "r" (arg1), [arg2] "r" (arg2), [arg3] "r" (arg3)
                 : "r0", "r1", "r2", "r3", "r7", "memory"
                 );
    return result;
}




/****** SYSCALLS *************************************************************************/
int write(int file, const char* s, int numChars) {
    return syscall3(4, file, (int)s, numChars);
}

int Open(const char *filename, int flags, int mode) {
    return syscall3(0x05, (int)filename, flags, mode);
}

int close(int fd) {
    return syscall1(0x06, fd);
}

void exit(int errorCode) {
    syscall1(1, errorCode);
    while (1); // since this function is marked noreturn
}


/****** HELPERS ****************************************************************************/
static char hexDigit(int h) {
    h = h & 0xf;
    return ((h >= 10)?('a'-10):'0') + h;
}


// writes a byte as two hex values
void writeHexByte(int f, char c) {
    char s[2];
    int i = (unsigned int)c;
    s[0] = hexDigit(i >> 4);
    s[1] = hexDigit(i & 0xf);
    write(f, s, 2);
}

void writeHexWord(int f, int w) {
    writeHexByte(f, (char)((int)w >> 24));
    writeHexByte(f, (char)((int)w >> 16));
    writeHexByte(f, (char)((int)w >> 8));
    writeHexByte(f, (char)((int)w));
}

