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

/** create emulator for the spcified elf defined by the target path, by injecting it into the emu */
static elf::ElfFile createEmulatedElf(const std::string& x86ElfPath,
                                      const EmuOptions& options) { //, const std::string& outputPath) {
    elf::ElfFile x86Elf(x86ElfPath);
    elf::ElfFile emuElf(emulatorElfPath);
    
    /*
    printf("target elf:\n");
    targetElf.printProgramHeaders();
    printf("emu elf:\n");
    emuElf.printProgramHeaders();
    */
    auto optionsCopy = options;
    optionsCopy.entryPoint = x86Elf.header().e_entry;
    
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
    uintptr_t MAGIC = EMU_OPTIONS_MAGIC;
    for (int i = 0; i < emuElf.numSegments(); i++) {
        auto segment = emuElf.getSegment(i);
		if (segment.header().p_type == PT_LOAD and
			segment.header().p_flags == (PF_R | PF_W)) {
			//printf("searching for entry point in segment %d\n", i);
            uintptr_t* pstart = (uintptr_t*)(emuElf.data() + segment.header().p_offset);
			uintptr_t* pcurrent = pstart;
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


int main(int argc, char *const argv[], char *const envp[]) {
    std::string path = argv[1];
    
    // parse options
    EmuOptions options;
    
    elf::ElfFile emulatedElf = createEmulatedElf(path, options); //, combinedElfPath);
    
	std::cout << "running emulated elf:" << std::endl;
    
    emulatedElf.execute(&argv[1], envp);
    
	//if (execve(combinedElfPath.c_str(), argv, envp) == -1) {
    //std::cerr << "failed to start combined emulator elf " << combinedElfPath << std::endl;
	//}
	
    return 0;
}



