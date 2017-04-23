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


const std::string emulatorElfPath = "femu-inject";
const std::string combinedElfPath = "modified-elf";

elf::ElfFile injectElf(const elf::ElfFile& targetElf, const elf::ElfFile& injectElf);
bool injectEmuData(elf::ElfFile& emuElf, const elf::ElfFile& targetElf);


/** returns the smallest number n >= offset s.t. n % alignment == 0 */
static size_t align(int offset, int alignment) {
    printf("alignoffet: %d, %d\n", offset, alignment);
    auto result = ((offset + (alignment - 1)) / alignment) * alignment;
    printf("result: %d\n", result);
    return result;
}

/** create emulator for the spcified elf defined by the target path, by injecting it into the emu */
static bool createEmulator(const std::string& targetPath, const std::string& outputPath) {
    elf::ElfFile targetElf(targetPath);
    elf::ElfFile emuElf(emulatorElfPath);
    
    printf("target elf:\n");
    targetElf.printProgramHeaders();
    printf("emu elf:\n");
    emuElf.printProgramHeaders();

    if (not injectEmuData(emuElf, targetElf)) return false;
    
    elf::ElfFile outputElf = injectElf(emuElf, targetElf);
    std::cout << "write file: " << outputPath << std::endl;
    return outputElf.writeToFile(outputPath);
}

/** append the given injectElf into the targetElf and return new elf */
elf::ElfFile injectElf(const elf::ElfFile& targetElf, const elf::ElfFile& injectElf) {
    
    // get alignment of inect elf
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
    printf("new alloc: %d\n", newSize);
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
        printf("add segment %d, offset: %d, \n", i, headerOffset);
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
    
    printf("output:\n");
    outElf.printHeader();
    outElf.printProgramHeaders();
    return outElf;
}


/** inject emulator data from target into the emulator elf */
bool injectEmuData(elf::ElfFile& emuElf, const elf::ElfFile& targetElf) {
    // find R/W load segment with the magic number
    uintptr_t MAGIC = 0xbe61be61;
    for (int i = 0; i < emuElf.numSegments(); i++) {
        auto segment = emuElf.getSegment(i);
        if (segment.header().p_type == PT_LOAD and
            segment.header().p_flags == (PF_R | PF_W)) {
            printf("searching for entry point in segment %d\n", i);
            uintptr_t* pointer = (uintptr_t*)(emuElf.data() + segment.header().p_offset);
            i = 0;
            while (*pointer != MAGIC) {
                if (i > segment.header().p_filesz) {
                    std::cerr << "entrypoint MAGIC not found in emu!" << std::endl;
                    return false;
                }
                pointer++;
                i += sizeof(uintptr_t);
            }
            printf("magic entrypoint found at offset: %d\n", i * sizeof(uintptr_t));
            *pointer = targetElf.header().e_entry;
            return true;
        }
    }
    return false;
}


int main(int argc, char *const argv[], char *const envp[]) {
    std::string path = argv[1];
    if (not createEmulator(path, combinedElfPath)) {
        std::cerr << "failed to inject emulator into program" << std::endl;
        exit(EXIT_FAILURE);
    }
    
    std::cout << "running combined elf:" << std::endl;
    
    if (execve(combinedElfPath.c_str(), argv, envp) == -1) {
        std::cerr << "failed to start combined emulator elf " << combinedElfPath << std::endl;
    }
    
    return 0;
}



