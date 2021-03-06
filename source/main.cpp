//
//  main.cpp
//  modify-elf
//
//  Created by Anton Dubrau on 2017-03-04.
//  Copyright © 2017 Ant6n. All rights reserved.
//

// TODO - inject x86 program into emulator elf

#include <iostream>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "main.h"
#include "elf-file.h"
#include "emu-shared.h"
#include <getopt.h>


const std::string emulatorElfPath = "femu-inject";
//const std::string combinedElfPath = "modified-elf";

elf::ElfFile injectElf(const elf::ElfFile& targetElf, const elf::ElfFile& injectElf);
bool injectEmuOptions(elf::ElfFile& emuElf, const EmuOptions& options);


/** returns the smallest number n >= offset s.t. n % alignment == 0 */
static size_t align(int offset, int alignment) {
    //printf("alignoffet: %d, %d\n", offset, alignment);
    auto result = ((offset + (alignment - 1)) / alignment) * alignment;
    //printf("result: %d\n", result);
    return result;
}


/** returns the loaded begin/end memory address of the first loadable R/W/noX segment,
    or <0,0> if not found */
std::pair<uintptr_t, uintptr_t> getReadWriteMemory(const elf::ElfFile& elf) {
    auto numSegments = elf.numSegments();
    for (int i = 0; i < numSegments; i++) {
        auto header = elf.getSegment(i).header();
        if ((header.p_type == PT_LOAD)
            && (header.p_flags & PF_R)
            && (header.p_flags & PF_W)
            && (!(header.p_flags & PF_X))) {
            uintptr_t begin = header.p_vaddr;
            uintptr_t size = header.p_memsz;
            printf("found RW segment at address %x, size %x\n", header.p_vaddr, header.p_memsz);
            return { begin, begin + size };
        }
    }
    printf("didn't find RW segment\n");
    return { 0, 0 };
}


/** create emulator for the spcified elf defined by the target path, by injecting it into the emu */
static elf::ElfFile createEmulatedElf(const std::string& x86ElfPath,
                                      const EmuOptions& options) {
    elf::ElfFile x86Elf(x86ElfPath);
    elf::ElfFile emuElf(emulatorElfPath);

    //printf("x86 elf:\n");
    //x86Elf.printProgramHeaders();
    //printf("emu elf:\n");
    //emuElf.printProgramHeaders();

    // set up options
    auto optionsCopy = options;
    optionsCopy.entryPoint = x86Elf.header().e_entry;
    
    if (optionsCopy.testJsonOut[0]) {
        if ((optionsCopy.testMemStart == 0) && (optionsCopy.testMemEnd == 0)) {
            auto readWriteMemory = getReadWriteMemory(x86Elf);
            optionsCopy.testMemStart = readWriteMemory.first;
            optionsCopy.testMemEnd = readWriteMemory.second;
        }
        printf("print output json to '%s' with mem test range 0x%x-0x%x\n",
               optionsCopy.testJsonOut, optionsCopy.testMemStart, optionsCopy.testMemEnd);
    }
    
    // inject options, x85 into emulator
    if (not injectEmuOptions(emuElf, optionsCopy)) {
        std::cerr << "failed to inject emulator into program" << std::endl;
        exit(EXIT_FAILURE);
    }
    elf::ElfFile outputElf = injectElf(emuElf, x86ElfPath);
    
    return outputElf;
}

/** append the given injectElf into the targetElf and return new elf */
elf::ElfFile injectElf(const elf::ElfFile& targetElf, const elf::ElfFile& injectElf) {
    
    // get alignment of inejct elf
    int injectElfAlignment = 1;
    for (int i = 0; i < injectElf.numSegments(); i++) {
        auto segment = injectElf.getSegment(i);
        // TODO - check if there's an overlap with segments in the given elf
        if (segment.header().p_type == PT_LOAD and
            segment.header().p_filesz > 0) {
            // TODO check power of two
            injectElfAlignment = std::max(injectElfAlignment,
                                          static_cast<int>(segment.header().p_align));
        }
    }

    // collect offsets and sizes
    size_t injectElfOffset = align(targetElf.size(), injectElfAlignment);
    size_t newProgramHeaderOffset = align(injectElfOffset + injectElf.size(), alignof(Elf32_Phdr));
    size_t newProgramHeaderSize = targetElf.header().e_phentsize*(targetElf.numSegments() +
                                                                 injectElf.numSegments());
    size_t newSize = newProgramHeaderOffset + newProgramHeaderSize;
    
    // create new data
    //printf("new alloc: %d\n", newSize);
    char* newData = new char [newSize];

    // copy elfs
    memcpy(newData, targetElf.data(), targetElf.size());
    memcpy(newData + injectElfOffset, injectElf.data(), injectElf.size());
    
    // copy program headers from output elf
    size_t inputPHSize = targetElf.header().e_phentsize * targetElf.numSegments();
    memcpy(newData + newProgramHeaderOffset, targetElf.data() + targetElf.header().e_phoff, inputPHSize);
    
    // copy program headers from inject elf
    size_t copyPHEntrySize = std::min(targetElf.header().e_phentsize, injectElf.header().e_phentsize);
    size_t headerOffset = newProgramHeaderOffset + inputPHSize;
    for (int i = 0; i < injectElf.numSegments(); i++) {
        //printf("add segment %d, offset: %d, \n", i, headerOffset);
        // copy program header entry
        auto segment = injectElf.getSegment(i);
        memcpy(newData + headerOffset, &segment.header(), copyPHEntrySize);
    
        // fix program header entry
        Elf32_Phdr* headerEntry = reinterpret_cast<Elf32_Phdr*>(newData + headerOffset);
        headerEntry->p_offset += injectElfOffset;
        if (headerEntry->p_type != PT_LOAD) {
            headerEntry->p_type = PT_NULL;
        }
    
        headerOffset += targetElf.header().e_phentsize;
    }
        
    // create new elf and update/fix header
    std::shared_ptr<char> shared(newData);
    elf::ElfFile outElf(shared, newSize);
    
    outElf.header().e_phoff = newProgramHeaderOffset;
    outElf.header().e_phnum = targetElf.numSegments() + injectElf.numSegments();
    
    //printf("output:\n");
    //outElf.printHeader();
    //outElf.printProgramHeaders();
    return outElf;
}


/** inject emulator data from target into the emulator elf */
bool injectEmuOptions(elf::ElfFile& emuElf, const EmuOptions& emuOptions) {
    // find R/W load segment with the magic number
    uint32_t MAGIC = EMU_OPTIONS_MAGIC;
    for (int i = 0; i < emuElf.numSegments(); i++) {
        auto segment = emuElf.getSegment(i);
        if (segment.header().p_type == PT_LOAD and
            segment.header().p_flags == (PF_R | PF_W)) {
            //printf("searching for entry point in segment %d\n", i);
            uint32_t* pstart = (uint32_t*)(emuElf.data() + segment.header().p_offset);
            uint32_t* pcurrent = pstart;
            while (*pcurrent != MAGIC) {
                if (pcurrent - pstart > segment.header().p_filesz) {
                    std::cerr << "entrypoint MAGIC not found in emu!" << std::endl;
                    return false;
                }
                pcurrent++;
            }
            printf("magic entrypoint found at offset: %d\n", pcurrent - pstart);
            std::cout << sizeof(EmuOptions);
            memcpy(pcurrent, &emuOptions, sizeof(EmuOptions));
            return true;
        }
    }
    return false;
}

typedef struct {
    std::string injectedElfOutputFilename;
    bool writeInjectedElf;
    bool writeInjectedElfAndQuit;
} MainOptions;


/** parses options and returns the options in the reference args,
    as well as the number of consumed argv args */
int parseArgs(int argc, char *const argv[], char* const envp[],
              EmuOptions& emuOptions, MainOptions& mainOptions) {
    // parse options
    const char* short_options = "vht:m:e:E:";
    static struct option long_options[] = {
        {"verbose",           no_argument,       0, 'v'},
        {"help",              no_argument,       0, 'h'},
        {"test",              required_argument, 0, 't'},
        {"test-memory",       required_argument, 0, 'm'},
        {"write-emu-elf",     required_argument, 0, 'e'},
        {"write-emu-elf-only",required_argument, 0, 'E'},
        {0, 0, 0, 0}
    };
    while (1) {
        int option_index = 0; // getopt_long stores the option index here.
        int c = getopt_long (argc, argv, short_options, long_options, &option_index);
        if (c == -1) break; // Detect the end of the options.
        
        switch (c) {
        case 0:
            printf("error - case 0 in getopt\n");
            exit(1);

        case 'h':
            printf("femu x86 emulator\n"
                   "Usage: femu [femu-OPTIONS]... x86-elf-file [x86-args]...\n"
                   "\n"
                   "  -v, --verbose                   print more output\n"
                   "  -h, --help                      print this help\n"
                   "  -t, --test JSON-FILE            unit-testing mode: output state at int3 into JSON\n"
                   "  -m, --test-memory 0xSTART,0xEND in unit-testing mode, output given memory-range into JSON\n"
                   "  -e, --write-emu-elf [FILE]      "
                   "write-out internal x86 elf with emu injected, for debugging\n"
                   "  -E, --write-emu-elf-only [FILE] "
                   "write-out internal x86 elf with emu injected, for debugging, and quit\n");
            exit(0);
        case 'v':
            emuOptions.verbose = 1;
            break;

        case 'e':
            mainOptions.injectedElfOutputFilename = optarg;
            mainOptions.writeInjectedElf = true;
            break;
        
        case 'E':
            mainOptions.injectedElfOutputFilename = optarg;
            mainOptions.writeInjectedElfAndQuit = true;
            break;
        
        case 't': {
            int numChars = strlen(optarg);
            if (numChars + 1 >= EMU_OPTIONS_MAX_FILENAME_LENGTH) {
                printf("test filename too long\n");
                exit(1);
            }
            memcpy(emuOptions.testJsonOut, optarg, numChars + 1);
            break;
        }
        case 'm':
            if (sscanf(optarg,"%x,%x", &emuOptions.testMemStart, &emuOptions.testMemEnd) != 2) {
                printf("failed scanning parameter '%s', expected 0xAAA,0xBBB\n", optarg);
                exit(1);
            }
            break;
        
        case 'f':
            printf ("option -f with value `%s'\n", optarg);
            break;
        
        case '?':
            /* getopt_long already printed an error message. */
            exit(1);
            break;
        
        default:
            printf("default case in opt-parsing, unknown case (%d): abort", c);
            exit(1);
        }
    }

    if (optind >= argc) {
        printf("no file supplied for emulation\n");
        exit(1);
    }    
    return optind;
}



int main(int argc, char *const argv[], char *const envp[]) {
    
    // parse/set up options
    EmuOptions emuOptions{};
    MainOptions mainOptions{};
    int numParsedArgs = parseArgs(argc, argv, envp, emuOptions, mainOptions);
    auto new_argv = &(argv[numParsedArgs]);
    int new_argc = argc - numParsedArgs;
    std::string path = new_argv[0];
    
    // create combined elf
    elf::ElfFile emulatedElf = createEmulatedElf(path, emuOptions);
    
    // output elf if necessary (and quit)
    if (mainOptions.writeInjectedElf || mainOptions.writeInjectedElfAndQuit) {
        std::cout << "write emulated elf to " << mainOptions.injectedElfOutputFilename << std::endl;
        emulatedElf.writeToFile(mainOptions.injectedElfOutputFilename);
        if (mainOptions.writeInjectedElfAndQuit) {
            exit(1);
        }
    }
    
    // run
    std::cout << "running emulated elf:" << std::endl;
    emulatedElf.execute(new_argv, envp);
    
    printf("error on executing emulated elf");
    exit(1);
}



