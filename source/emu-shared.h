#ifndef __emu_shared__
#define __emu_shared__


#include <stdint.h>

#define EMU_OPTIONS_MAGIC 0xbe61be61

#ifdef __cplusplus
extern "C" {
#endif


    typedef struct {
        uint32_t MAGIC; // used to find the options struct inside the emu elf
        uintptr_t entryPoint;
        char testJsonOut[1024]; // if this defines a string, use test mode and output state on int3
    } EmuOptions;



#ifdef __cplusplus
}
#endif



#endif /* defined(__emu_shared__) */
