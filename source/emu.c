
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

int main() {
  write(1, "hello world ARM!\n", 17);
}
