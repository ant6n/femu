char* entryPoint = (char*)0xbe61be61;
extern int femuStart(void* pc);


int main();

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

void _start() __attribute__ ((naked));

void _start() {
  main();
  asm volatile(
	       "mov r7, #1\n" /* exit */
	               "svc #0\n"
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

int main() {
  char* ip = entryPoint;
  write(1, "hello femu ARM! x86 code:\n", 27);

  // print starting point
  write(1, "entry point: 0x", 15);
  writeHexByte(1, (char)((int)ip >> 24));
  writeHexByte(1, (char)((int)ip >> 16));
  writeHexByte(1, (char)((int)ip >> 8));
  writeHexByte(1, (char)((int)ip));
  write(1, "\n", 1);
  
  // print first 20 bytes of program
  write(1, "first bytes: ", 14);
  int i = 0;
  for (i = 0; i < 20; i++) {
    writeHexByte(1, ip[i]);
    write(1, " ", 1);
  }
  write(1, "\n", 1);

  // actually execute
  femuStart(entryPoint);
  
  write(1, "finished execution\n", 19);
}
