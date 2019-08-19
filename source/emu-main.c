#include "emu-shared.h"
#include "emu-lib.h"
#include <stdint.h>


extern int femuStart(void* pc, void* stack_pointer);
extern uint32_t stored_eip;
extern uint32_t stored_eax;
extern uint32_t stored_ebx;
extern uint32_t stored_ecx;
extern uint32_t stored_edx;
extern uint32_t stored_esi;
extern uint32_t stored_edi;
extern uint32_t stored_ebp;
extern uint32_t stored_esp;


//char* entryPoint = (char*)(EMU_OPTIONS_MAGIC);
EmuOptions options = { EMU_OPTIONS_MAGIC };


/** define alternative stack */
uint32_t arm_stack[1024];


int emu_main(void* x86_stack_pointer);
void _start() __attribute__ ((naked));


void _start() {
    // TODO set up stack
    asm volatile("mov r0, sp\n" // stack pointer -> arg to main
                 "ldr sp, =arm_stack\n" // use arm stack
                 "add sp, sp, 4088\n" // move sp to end of arm stack
                 "b   emu_main\n"
                 : /* output */
                 : /* input */
                 : /* clobber */
                 );

}


int emu_main(void* x86_stack_pointer) {
    char* pc = (char*)(options.entryPoint);
    write(1, "hello femu ARM! x86 code:\n", 27);
    
    // print starting point
    write(1, "entry point: 0x", 15);
    writeHexWord(1, (int)pc);
    write(1, "\n", 1);
    
    // print first 20 bytes of program
    write(1, "first bytes: ", 14);
    int i = 0;
    for (i = 0; i < 20; i++) {
        writeHexByte(1, pc[i]);
        write(1, " ", 1);
    }
    write(1, "\n", 1);
    
    // actually execute
    femuStart(pc, x86_stack_pointer);
    
    write(1, "finished execution\n", 19);
    write(1, "eip: 0x", 7); writeHexWord(1, stored_eip); write(1, "\n", 1);
    write(1, "eax: 0x", 7); writeHexWord(1, stored_eax); write(1, "\n", 1);
    write(1, "ebx: 0x", 7); writeHexWord(1, stored_ebx); write(1, "\n", 1);
    write(1, "ecx: 0x", 7); writeHexWord(1, stored_ecx); write(1, "\n", 1);
    write(1, "edx: 0x", 7); writeHexWord(1, stored_edx); write(1, "\n", 1);
    write(1, "esi: 0x", 7); writeHexWord(1, stored_esi); write(1, "\n", 1);
    write(1, "edi: 0x", 7); writeHexWord(1, stored_edi); write(1, "\n", 1);
    write(1, "ebp: 0x", 7); writeHexWord(1, stored_ebp); write(1, "\n", 1);
    write(1, "esp: 0x", 7); writeHexWord(1, stored_esp); write(1, "\n", 1);
    
    exit(0);
}
