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

/** returns the smallest number n >= offset s.t. n % alignment == 0 */
static int alignOffset(int offset, int alignment) {
    printf("alignoffet: %d, %d\n", offset, alignment);
    auto result = ((offset + (alignment - 1)) / alignment) * alignment;
    printf("result: %d\n", result);
    return result;
}

static bool injectEmulator(const std::string& inputPath, const std::string& outputPath) {
    std::cout << "load file: '" <<inputPath << "'" << std::endl;
    elf::ElfFile elf(inputPath);
    elf::ElfFile injectElf(emulatorElfPath);
    
    printf("elf:\n");
    //elf.printHeader();
    elf.printProgramHeaders();
    printf("inject:\n");
    //injectElf.printHeader();
    injectElf.printProgramHeaders();
    
    // determine new elf file size, and check overlap
    size_t copiedSegmentSize = 0;
    int numInjectSegments = injectElf.getNumSegments();
    int numElfSegments = elf.getNumSegments();
    for (int i = 0; i < numInjectSegments; i++) {
        auto segment = injectElf.getSegment(i);
	copiedSegmentSize += segment.getProgramHeader().p_filesz;
	// TODO - check if there's an overlap with segments in the given elf
    }
    size_t newProgramHeaderSize = elf.getHeader().e_phentsize*(numInjectSegments + numElfSegments);
    size_t newHeaderOffset = alignOffset(elf.getSize() + copiedSegmentSize, alignof(Elf32_Phdr));
    size_t newSize = newHeaderOffset + newProgramHeaderSize;
    
    
    // create new data
    printf("new alloc: %d\n", newSize);
    char* newData = new char [newSize];
    
    // copy target elf
    size_t offset = 0;
    memcpy(newData + offset, elf.getData(), elf.getSize());
    offset += elf.getSize();
    
    // copy target headers
    size_t headerOffset = newHeaderOffset;
    size_t elfProgramHeaderSize = elf.getHeader().e_phentsize * numElfSegments;
    memcpy(newData + headerOffset, elf.getData() + elf.getHeader().e_phoff, elfProgramHeaderSize);
    headerOffset += elfProgramHeaderSize;
    int newNumSegments = numElfSegments;
    
    // copy segments and headers from inject elf
    size_t copyProgramHeaderEntrySize = std::min(elf.getHeader().e_phentsize,
						 injectElf.getHeader().e_phentsize);
    for (int i = 0; i < numInjectSegments; i++) {
        printf("add segment %d, offset: %d, \n", i, offset);
        // copy program header entry
        auto segment = injectElf.getSegment(i);
	memcpy(newData + headerOffset, &segment.getProgramHeader(), copyProgramHeaderEntrySize);
	
        // fix program header entry
	Elf32_Phdr* headerEntry = reinterpret_cast<Elf32_Phdr*>(newData + headerOffset);
	headerEntry->p_offset = offset;

	if (headerEntry->p_type != PT_LOAD) {
  	    headerEntry->p_type = PT_NULL;
	}
	
	// copy segment
        size_t segmentFileSize = segment.getProgramHeader().p_filesz;
	if (segmentFileSize > 0) {
	    memcpy(newData + offset,
		   injectElf.getData() + segment.getProgramHeader().p_offset,
		   segmentFileSize);
	    offset += segmentFileSize;
        }
	
        newNumSegments += 1;
	headerOffset += elf.getHeader().e_phentsize;
    }
    
    // create new elf and update/fix header
    std::shared_ptr<char> shared(newData);
    elf::ElfFile outElf(shared, newSize);
    auto& outHeader = outElf.getHeader();
    outHeader.e_machine = EM_ARM;
    outHeader.e_phoff = newHeaderOffset;
    outHeader.e_phnum = newNumSegments;
    outHeader.e_entry = injectElf.getHeader().e_entry;
    outHeader.e_shoff = 0;
    outHeader.e_shnum = 0;
    outHeader.e_shstrndx = 0;
    
    printf("output:\n");
    //outElf.printHeader();
    outElf.printProgramHeaders();
    std::cout << "write file: " << outputPath << std::endl;
    
    return outElf.writeToFile(outputPath);  
}



int main(int argc, const char * argv[]) {
    std::string path = argv[1];
    injectEmulator(path, "modified-elf");
    return 0;
}



