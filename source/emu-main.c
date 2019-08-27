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
extern uint32_t stored_eflags;
extern uint32_t stored_unimplemented_opcode;


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
    fprints(1, "hello femu ARM! x86 code:\n");
    
    // print starting point
    fprints(1, "entry point: 0x");
    fprintx(1, (int)pc);
    fprints(1, "\n");
    
    // print first 20 bytes of program
    fprints(1, "first bytes: ");
    int i = 0;
    for (i = 0; i < 20; i++) {
        writeHexByte(1, pc[i]);
        fprints(1, " ");
    }
    fprints(1, "\n");
}

void print_result(int result_status) {
    fprints(1, "finished execution\n");
    fprints(1, "eip: 0x"); fprintx(1, stored_eip); fprints(1, "\n");
    fprints(1, "eax: 0x"); fprintx(1, stored_eax); fprints(1, "\n");
    fprints(1, "ebx: 0x"); fprintx(1, stored_ebx); fprints(1, "\n");
    fprints(1, "ecx: 0x"); fprintx(1, stored_ecx); fprints(1, "\n");
    fprints(1, "edx: 0x"); fprintx(1, stored_edx); fprints(1, "\n");
    fprints(1, "esi: 0x"); fprintx(1, stored_esi); fprints(1, "\n");
    fprints(1, "edi: 0x"); fprintx(1, stored_edi); fprints(1, "\n");
    fprints(1, "ebp: 0x"); fprintx(1, stored_ebp); fprints(1, "\n");
    fprints(1, "esp: 0x"); fprintx(1, stored_esp); fprints(1, "\n");
    fprints(1, "eflags: 0x"); fprintx(1, stored_eflags); fprints(1, "\n");
    
    switch(result_status) {
    case FEMU_EXIT_SUCCESS:
        fprints(1, "result: FEMU_EXIT_SUCCESS\n"); break;
    case FEMU_EXIT_FAILURE:
        fprints(1, "result: FEMU_EXIT_FAILURE\n"); break;
    case FEMU_EXIT_UNIMPLEMENTED_OPCODE:
        fprints(1, "result: FEMU_EXIT_UNIMPLEMENTED_OPCODE: ");
        writeHexByte(1, stored_unimplemented_opcode & 0xFF);
        fprints(1, " ");
        writeHexByte(1, (stored_unimplemented_opcode >> 8) & 0xFF);
        fprints(1, "\n");
        break;
    case FEMU_EXIT_INT3:
        fprints(1, "result: FEMU_EXIT_INT3\n"); break;
    }
}


// writes a json '    <name>: 0x<hexvalue>(,)\n' to fd
void writeJsonPair(int fd, char* name, int value, int testValue, int putComma) {
    if ((value != 0) || (!testValue)) {
        fprints(fd, "    \"");
        fprints(fd, name);
        fprints(fd, "\": \"0x");
        fprintx(fd, value);
        if (putComma) {
            fprints(fd, "\",\n");
        } else {
            fprints(fd, "\"\n");
        }
    }
}

void outputJsonResult(int result_status) {
    fprints(1, "output json\n");
    int fd = Open(options.testJsonOut, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
    //fprints(1, "write json out: ");
    //fprints(1, options.testJsonOut);
    //fprints(1, "\n");
    
    if (fd < 0) {
        fprints(1, "Error opening file for json output\n");
        return;
    }

    // write result status
    int failure = 0;
    switch(result_status) {
    case FEMU_EXIT_SUCCESS:
        fprints(fd, "{\n  \"result_status\": \"EXIT_SUCCESS\",\n"); break;
    case FEMU_EXIT_INT3:
        fprints(fd, "{\n  \"result_status\": \"EXIT_INT3\",\n"); break;
    case FEMU_EXIT_FAILURE:
        fprints(fd, "{\n  \"result_status\": \"EXIT_FAILURE\",\n"); failure=1; break;
    case FEMU_EXIT_UNIMPLEMENTED_OPCODE:
        fprints(fd, "{\n  \"result_status\": \"EXIT_UNIMPLEMENTED_OPCODE\",\n"
                    "  \"unimplemented_opcode\": \"");
        writeHexByte(fd, stored_unimplemented_opcode & 0xFF);
        fprints(fd, " ");
        writeHexByte(fd, (stored_unimplemented_opcode >> 8) & 0xFF);
        fprints(fd, "\",\n");
        failure=1;
        break;
    }
    if (failure) {
        fprints(fd, "  \"result_status_test_failed\": true\n}\n");
        close(fd);
        return;
    } else {
        fprints(fd, "  \"result_status_test_failed\": false,\n");
    }
        
    // write registers
    fprints(fd,
            "  \"result_registers\": {\n");
    
    writeJsonPair(fd, "eip", stored_eip, 0, 1);
    writeJsonPair(fd, "eax", stored_eax, 1, 1);
    writeJsonPair(fd, "ebx", stored_ebx, 1, 1);
    writeJsonPair(fd, "ecx", stored_ecx, 1, 1);
    writeJsonPair(fd, "edx", stored_edx, 1, 1);
    writeJsonPair(fd, "esi", stored_esi, 1, 1);
    writeJsonPair(fd, "edi", stored_edi, 1, 1);
    writeJsonPair(fd, "ebp", stored_ebp, 1, 1);
    writeJsonPair(fd, "esp", stored_esp, 1, 1);
    writeJsonPair(fd, "eflags", stored_eflags, 0, 0);
    
    // write nonzero memory in test range
    fprints(fd,
          "  },\n"
          "  \"result_memory\": {\n");
    writeJsonPair(fd, "start", options.testMemStart, 1, 1);
    writeJsonPair(fd, "end",   options.testMemEnd, 1, 1);
    fprints(fd, "    \"nonzeros\": {\n");
    
    uint64_t* p = (void*)((options.testMemStart >> 3) << 3);
    void* end = (void*)(options.testMemEnd);
    char* delim = "";
    while ((void*)p <= end) {
        if (*p) {
            fprints(fd, delim);
            fprints(fd, "      \"0x");
            fprintx(fd, (uint32_t)p);
            fprints(fd, "\": \"");
            for (int i = 0; i < 8; i++) {
                if (i != 0) fprints(fd, " ");
		if (i < 4) writeHexByte(fd, shiftRight(*p, 8*i) & 0xFF);
		else       writeHexByte(fd, shiftRight((*p >> 32), 8*(i-4)) & 0xFF);
		  
            }
            fprints(fd, "\"");
            delim = ",\n";
        }
        p++;
    }
    fprints(fd,
          "\n"
          "    }\n"
          "  }\n"
          "}\n");
    
    close(fd);
}



int emu_main(void* x86_stack_pointer) {
    char* pc = (char*)(options.entryPoint);
    
    print_welcome(pc);
    
    // execute
    int result_status = femuRun(pc, x86_stack_pointer);
    
    print_result(result_status);
    
    if (options.testJsonOut[0]) {
        outputJsonResult(result_status);
    }
    
    exit(0);
}
