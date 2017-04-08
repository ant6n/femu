//
//  elf-file.h
//  load, view and modify elf files
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
		
        Elf32_Ehdr& header();
        const Elf32_Ehdr& header() const;
        
        char* data();
        const char* data() const;

	size_t size() const;
	
	int numSegments() const;
        Segment getSegment(int programHeaderIndex);
        const Segment getSegment(int programHeaderIndex) const;
        
        void printHeader() const;
        void printProgramHeaders() const;
        
        
    private:
        ElfFile(std::pair<std::shared_ptr<char>, std::ifstream::pos_type> dataAndSize);
        
        std::shared_ptr<char> _data;
        size_t _size;
        Elf32_Ehdr* elfHeader;
        
        friend class Segment;
    };
    
    
    /** represents the view of a segment within an elf file, defined in the Program Header */
    class Segment {
    public:
        Elf32_Phdr& header();
        const Elf32_Phdr& header() const;
        
    private:
        ElfFile& elfFile;
        int index;
        
        Segment(ElfFile& elfFile, int programHeaderIndex);
        
        friend class ElfFile;
    };
}

#endif /* defined(__modify_elf__elf_file__) */
