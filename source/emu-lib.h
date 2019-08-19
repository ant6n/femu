/** 
 Helper library for the emu, which doesn't use the cstdlib
*/
#ifndef __emu_lib__
#define __emu_lib__



void write(int file, const char* s, int numChars);
void writeHexByte(int f, char c);
void writeHexWord(int f, int w);
void exit(int errorCode) __attribute__((noreturn));

#endif /* defined(__emu_lib__) */

