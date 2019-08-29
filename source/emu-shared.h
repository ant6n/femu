#ifndef __emu_shared__
#define __emu_shared__


#include <stdint.h>

#define EMU_OPTIONS_MAGIC 0xbe61be61
#define EMU_OPTIONS_MAX_FILENAME_LENGTH 1024

#ifdef __cplusplus
extern "C" {
#endif


    typedef struct {
        uint32_t MAGIC; // used to find the options struct inside the emu elf
        uint32_t entryPoint;
        int8_t verbose;
        char testJsonOut[EMU_OPTIONS_MAX_FILENAME_LENGTH]; // if defined, use test mode: output state on int3
        uint32_t testMemStart; // memory to output when using testJsonOut
        uint32_t testMemEnd;
    } EmuOptions;



#ifdef __cplusplus
}
#endif



#endif /* defined(__emu_shared__) */
