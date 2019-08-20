#include "emu-shared.h"
#include "emu-lib.h"
#include <stdint.h>
#include "shared_constants.h"

extern int femuRun(void* pc, void* stack_pointer);
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


void print_welcome(char* pc) {
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
}

void print_result(int result_status) {
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
    
    switch(result_status) {
    case FEMU_EXIT_SUCCESS:
        write(1, "result: FEMU_EXIT_SUCCESS\n", 26); break;
    case FEMU_EXIT_FAILURE:
        write(1, "result: FEMU_EXIT_FAILURE\n", 26); break;
    case FEMU_EXIT_UNIMPLEMENTED_OPCODE:
        write(1, "result: FEMU_EXIT_UNIMPLEMENTED_OPCODE\n", 40); break;
    case FEMU_EXIT_INT3:
        write(1, "result: FEMU_EXIT_INT3\n", 24); break;
    }
}


void outputJsonResult() {
    write(1, "output json\n", 12);
    int fd = Open(options.testJsonOut, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
    write(1, options.testJsonOut, 12);
    
    if (fd < 0) {
        write(1, "Error when opening file for json output\n", 40);
        return;
    }
    
    write(fd, "{\n", 2);
    // "test_name": "base-0111-mov-0x789ABCDE->[0x800003F4])","
    //"test_code": "mov eax, strict dword 0x789ABCDE;mov [0x800003F4], eax;int3",
    write(fd, "  \"result_registers\": {\n", 24);
#define OUTPUT_REG(REG) if (stored_##REG) { \
        write(fd, "    \"" #REG "\": \"0x", 14); \
        writeHexWord(fd, stored_##REG); \
        write(fd, "\",\n", 3); }
    OUTPUT_REG(eip);
    OUTPUT_REG(eax);
    OUTPUT_REG(ebx);
    OUTPUT_REG(ecx);
    OUTPUT_REG(edx);
    OUTPUT_REG(esi);
    OUTPUT_REG(edi);
    OUTPUT_REG(ebp);
    OUTPUT_REG(esp);
    // TODO eflags
    write(fd, "    \"eflags\": \"0x", 17); writeHexWord(fd, 0); write(fd, "\"\n", 2);
    
    write(fd, "\n  },\n", 6);
    write(fd, "  \"result_memory\": {\n", 21);
    write(fd, "    \"start\": \"0x", 16); writeHexWord(fd, 0x7fffe000); write(fd, "\",\n", 3);
    write(fd, "    \"end\":   \"0x", 16); writeHexWord(fd, 0x80000410); write(fd, "\",\n", 3);
    write(fd, "    \"nonzeros\": {\n", 18);
    // TODO memory
    //"      \"0x800003f0\": \"00 00 00 00 de bc 9a 78\"\n"
    write(fd, "    }\n", 6);
    write(fd, "  }\n", 4);
    write(fd, "}\n", 2);
    
    // close
    write(1, "\nclose: ", 8);
    writeHexWord(1, close(fd));
}


int emu_main(void* x86_stack_pointer) {
    char* pc = (char*)(options.entryPoint);
    
    print_welcome(pc);
    
    // execute
    int result_status = femuRun(pc, x86_stack_pointer);
    
    print_result(result_status);
    
    if (options.testJsonOut[0] && result_status == FEMU_EXIT_INT3) {
        outputJsonResult();
    }
    
    exit(0);
}
