#include "emu-lib.h"

void write(int file, const char* s, int numChars) {
    asm volatile("mov r0, %[file]\n" /* file */
                 "mov r1, %[buf]\n"  /* write buffer  */
                 "mov r2, %[size]\n" /* size          */
                 "mov r7, #4\n"      /* write syscall */
                 "svc #0\n"
                 : /* output */
                 : [buf] "r" (s), [size] "r" (numChars), [file] "r" (file)
                 : "r0", "r1", "r2", "r7", "memory"
                 );
}


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

void exit(int errorCode) {
    asm volatile("mov r7, #1\n" /* exit */
                 "svc #0\n"
                 );
    while (1);
}
