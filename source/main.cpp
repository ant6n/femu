//
//  main.cpp
//  modify-elf
//
//  Created by Anton Dubrau on 2017-03-04.
//  Copyright © 2017 Ant6n. All rights reserved.
//

#include <iostream>
#include <stdio.h>
#include <sys/stat.h>
#include "main.h"
#include "elf-file.h"
#include <string.h>


const std::string emulatorElfPath = "femu-inject";

elf::ElfFile injectElf(const elf::ElfFile& inputElf, const elf::ElfFile& injectElf);
elf::ElfFile injectData(const elf::ElfFile& elf, const char* data, size_t size);

/** returns the smallest number n >= offset s.t. n % alignment == 0 */
static size_t align(int offset, int alignment) {
    printf("alignoffet: %d, %d\n", offset, alignment);
    auto result = ((offset + (alignment - 1)) / alignment) * alignment;
    printf("result: %d\n", result);
    return result;
}

static bool injectEmulator(const std::string& inputPath, const std::string& outputPath) {
    elf::ElfFile inputElf(inputPath);
    elf::ElfFile emuElf(emulatorElfPath);
    
    printf("elf:\n");
    //inputElf.printHeader();
    inputElf.printProgramHeaders();
    printf("inject:\n");
    //injectElf.printHeader();
    emuElf.printProgramHeaders();
    
    elf::ElfFile outputElf = injectElf(inputElf, emuElf);
    std::cout << "write file: " << outputPath << std::endl;
    return outputElf.writeToFile(outputPath);
}

elf::ElfFile injectElf(const elf::ElfFile& inputElf, const elf::ElfFile& injectElf) {
    
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
    size_t injectElfOffset = align(inputElf.size(), injectElfAlignment);
    size_t programHeaderOffset = align(injectElfOffset + injectElf.size(), alignof(Elf32_Phdr));
    size_t newProgramHeaderSize = inputElf.header().e_phentsize*(inputElf.numSegments() +
								 injectElf.numSegments());
    size_t newSize = programHeaderOffset + newProgramHeaderSize;
    
    // create new data
    printf("new alloc: %d\n", newSize);
    char* newData = new char [newSize];

    // copy elfs
    memcpy(newData, inputElf.data(), inputElf.size());
    memcpy(newData + injectElfOffset, injectElf.data(), injectElf.size());
    
    // copy segment headers from output elf
    size_t inputPHSize = inputElf.header().e_phentsize * inputElf.numSegments();
    memcpy(newData + programHeaderOffset, inputElf.data() + inputElf.header().e_phoff, inputPHSize);
    
    // copy program headers from inject elf
    size_t copyPHEntrySize = std::min(inputElf.header().e_phentsize, injectElf.header().e_phentsize);
    size_t headerOffset = programHeaderOffset + inputPHSize;
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
	
	headerOffset += inputElf.header().e_phentsize;
    }
        
    // create new elf and update/fix header
    std::shared_ptr<char> shared(newData);
    elf::ElfFile outElf(shared, newSize);
    outElf.header().e_machine = EM_ARM;
    outElf.header().e_phoff = programHeaderOffset;
    outElf.header().e_phnum = inputElf.numSegments() + injectElf.numSegments();
    outElf.header().e_entry = injectElf.header().e_entry;
    outElf.header().e_shoff = 0;
    outElf.header().e_shnum = 0;
    outElf.header().e_shstrndx = 0;
    
    printf("output:\n");
    outElf.printHeader();
    outElf.printProgramHeaders();
    return outElf;
}

int main(int argc, const char * argv[]) {
    std::string path = argv[1];
    injectEmulator(path, "modified-elf");
    
    return 0;
}



