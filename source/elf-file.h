//
//  elf-file.h
//  modify-elf
//
//  Created by Anton Dubrau on 2017-03-04.
//  Copyright © 2017 Ant6n. All rights reserved.
//

#ifndef __modify_elf__elf_file__
#define __modify_elf__elf_file__

#include <iostream>
#include <fstream>
#include <memory>
#include <utility>
#include <elf.h>


namespace elf {
    class Segment;
    
    class ElfFile {
    public:
        ElfFile(const std::string& path);
        ElfFile(std::shared_ptr<char> data, const size_t size);
        
        /** write elf to file, returns true for success */
        bool writeToFile(const std::string& path);

        void addSegment();
        
        /** allows modifying the header - this will only be valid until the next modication operation */
        Elf32_Ehdr& getHeader();
        const Elf32_Ehdr& getHeader() const;
        
        char* getData();
        const char* getData() const;

	size_t getSize() const;
	
	int getNumSegments() const;
        Segment getSegment(int programHeaderIndex);
        const Segment getSegment(int programHeaderIndex) const;
        
        void printHeader() const;
        void printProgramHeaders() const;
        
        
    private:
        ElfFile(std::pair<std::shared_ptr<char>, std::ifstream::pos_type> dataAndSize);
        
        std::shared_ptr<char> data;
        size_t size;
        Elf32_Ehdr* elfHeader;
        
        friend class Segment;
    };
    
    
    /** represents the view of a segment within an elf file, defined in the Program Header */
    class Segment {
    public:
        Elf32_Phdr& getProgramHeader();
        const Elf32_Phdr& getProgramHeader() const;
        
    private:
        ElfFile& elfFile;
        int index;
        
        Segment(ElfFile& elfFile, int programHeaderIndex);
        
        friend class ElfFile;
    };
}

#endif /* defined(__modify_elf__elf_file__) */
