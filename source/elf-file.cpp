//
//  elf-file.cpp
//  modify-elf
//
//  Created by Anton Dubrau on 2017-03-04.
//  Copyright © 2017 Ant6n. All rights reserved.
//

#include "elf-file.h"
#include <string>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <unistd.h>
#include <errno.h>


/** reads a file and returns its contents, returns the size per reference */
static std::shared_ptr<char> readFile(const std::string &path, std::ifstream::pos_type *sizeReturn) {
    using std::ios;
    
    std::ifstream file (path, ios::in | ios::binary| ios::ate);
    
    if (not file.is_open()) {
        return nullptr;
    }
    
    std::ifstream::pos_type size = file.tellg();
    char* memblock = new char [size];
    file.seekg (0, ios::beg);
    file.read (memblock, static_cast<std::streamsize>(size));
    file.close();
    
    if (sizeReturn != nullptr) {
        *sizeReturn = size;
    }
    
    std::cout << "loaded file, length: " << *sizeReturn << std::endl;
    
    return std::shared_ptr<char>(memblock);
}

static std::pair<std::shared_ptr<char>, std::ifstream::pos_type> readFileToTuple(const std::string &path) {
    std::ifstream::pos_type size;
    auto data = readFile(path, &size);
    return { data, static_cast<size_t>(size) };
}



namespace  elf {
    ElfFile::ElfFile(const std::string& path)
    : ElfFile(readFileToTuple(path)) {
    }
    
    ElfFile::ElfFile(std::shared_ptr<char> data, const size_t size)
    : ElfFile(std::pair<std::shared_ptr<char>, size_t>{ data, size }) {
    }
    
    ElfFile::ElfFile(std::pair<std::shared_ptr<char>, std::ifstream::pos_type> dataAndSize)
    : _data(dataAndSize.first), _size(dataAndSize.second) {
        elfHeader = reinterpret_cast<Elf32_Ehdr*>(data());
        auto ident = elfHeader->e_ident;
        bool correctMagic = (ident[0] == 0x7f and
                             ident[1] == 'E' and ident[2] == 'L' and ident[3] == 'F');
        //std::cout << "correct magic number: " << (correctMagic?"yes":"no") << std::endl;
        //printf("OS-ABI:               %X\n", ident[EI_OSABI]);
        //printf("Machine:              %X\n", elfHeader->e_machine);
    }
    
    Elf32_Ehdr& ElfFile::header() {
        return *elfHeader;
    }
    
    const Elf32_Ehdr& ElfFile::header() const {
        return *elfHeader;
    }

    char* ElfFile::data() {
        return _data.get();
    }
    
    const char* ElfFile::data() const {
        return _data.get();
    }
    
    size_t ElfFile::size() const {
          return _size;
    }
    
    void ElfFile::printHeader() const {
        printf("ELF Header:\n");
        printf("  Magic:   ");//7f 45 4c 46 01 01 01 00 00 00 00 00 00 00 00 00
        for (int i = 0; i < EI_NIDENT; i++) {
            if (1 <= i and i <= 3) {
                printf("'%c' ", elfHeader->e_ident[i]);
            } else {
                printf("%02x ", elfHeader->e_ident[i]);
            }
        }
        printf("\n");
        
        printf("  Class:                             \n");//ELF32
        //printf("  Data:                              \n");//2's complement, little endian
        //printf("  Version:                           \n");//1 (current)
        //printf("  OS/ABI:                            \n");//UNIX - System V
        //printf("  ABI Version:                       \n");//0
        //printf("  Type:                              \n");//EXEC (Executable file)
        //printf("  Machine:                           \n");//ARM
        //printf("  Version:                           \n");//0x1
        printf("  Entry point address:               0x%08x\n", elfHeader->e_entry);
        printf("  Start of program headers:          %d (byte offset)\n", elfHeader->e_phoff);
        printf("  Start of section headers:          %d (byte offset)\n", elfHeader->e_shoff);
        printf("  Flags:                             %x\n", elfHeader->e_flags);//0x0
        printf("  Size of this header:               %d (bytes)\n", elfHeader->e_ehsize);//52 (bytes)
        printf("  Size of program headers:           %d (bytes)\n", elfHeader->e_phentsize);//32 (bytes)
        printf("  Number of program headers:         %d\n", elfHeader->e_phnum);//3
        printf("  Size of section headers:           %d (bytes)\n", elfHeader->e_shentsize);//40 (bytes)
        printf("  Number of section headers:         %d\n", elfHeader->e_shnum);//7
        printf("  Section header string table index: %d\n", elfHeader->e_shstrndx);//4
    }
    
    
    int ElfFile::numSegments() const {
        return header().e_phnum;
    }
  
    Segment ElfFile::getSegment(int programHeaderIndex) {
        return Segment(*this, programHeaderIndex);
    }
    
    const Segment ElfFile::getSegment(int programHeaderIndex) const {
        return Segment(*const_cast<ElfFile*>(this), programHeaderIndex);
    }
    
    
    void ElfFile::printProgramHeaders() const {
      int numHeaders = numSegments();
        if (numHeaders == 0) {
            printf("There are no program headers in this file.\n");
        }
        
        printf("Program Headers:\n");
        printf("  Type        Offset      VirtAddr    PhysAddr    FileSiz     MemSiz      Flg  Align\n");
        for (int i = 0; i < numHeaders; i++) {
            const Segment segment = getSegment(i);
            auto header = segment.header();
            
            switch (header.p_type) {
                case PT_NULL:     printf("  PT_NULL   "); break;
                case PT_LOAD:     printf("  PT_LOAD   "); break;
                case PT_DYNAMIC:  printf("  PT_DYNAMIC"); break;
                case PT_INTERP:   printf("  PT_INTERP "); break;
                case PT_NOTE:     printf("  PT_NOTE   "); break;
                case PT_SHLIB:    printf("  PT_SHLIB  "); break;
                case PT_PHDR:     printf("  PT_PHDR   "); break;
                case PT_TLS:      printf("  PT_TLS    "); break;
                default:          printf("  0x%08x", header.p_type); break;
            }
            
            printf("  0x%08x  0x%08x  0x%08x  %-11d %-11d ",
                   header.p_offset, header.p_vaddr, header.p_paddr, header.p_filesz, header.p_memsz);
            printf("%c%c%c  ",
                   header.p_flags & PF_R ? 'R' : ' ',
                   header.p_flags & PF_W ? 'W' : ' ',
                   header.p_flags & PF_X ? 'X' : ' ');
            printf("%-d\n", header.p_align);
        }
    }
    
    
    
    bool ElfFile::writeToFile(const std::string& path) {
        using std::ofstream;

        std::ofstream file(path, ofstream::out | ofstream::binary | ofstream::trunc);
        if (not file.is_open()) {
            std::cerr << "failed to open for write: " << path << std::endl;
            return false;
        }
        
		// make file writeable if it exists
		if (chmod(path.c_str(), S_IWUSR) != 0) {
            std::cerr << "failed to make existing file writeable " << path << std::endl;
            return false;
        }
        
		printf("elf-write %d bytes\n", size());
		file.write(data(), size());
        printf("done\n");
        
        if (file.fail()) {
            std::cerr << "failed to write elf to: " << path << std::endl;
            file.close();
            return false;
        }
        
        file.close();
        
        if (chmod(path.c_str(), S_IXUSR | S_IRUSR | S_IXGRP  | S_IRGRP | S_IXOTH | S_IROTH) != 0) {
            std::cerr << "failed to make executable: " << path << std::endl;
            return false;
        }
        
        return true;
    }
    
    
    bool ElfFile::execute(char *const argv[], char *const envp[]) {
        std::cout << "execute elf as " << argv[0] << std::endl;
        
        // create in-memory file - by writing it to /tmp/
        //std::string filename = std::string("a.out") + '-' + std::to_string(getpid());
        std::vector<char> filename = {'/','t','m','p','/','e','l','f','-','X','X','X','X','X','X', '\0'};
        std::cout << "modified" << std::endl;
        int fd = mkstemp(filename.data()); //, O_RDWR | O_CREAT); //memfd_create(argv[0], MFD_CLOEXEC);
        std::cout << "after mkostemp" << std::endl;
        if (fd < 0) {
            std::cout << "failed creating file" << std::endl;
            if (errno == EACCES) std::cout << "EACCESS" << std::endl;
            if (errno == EEXIST) std::cout << "EEXIST" << std::endl;
            if (errno == EINVAL) std::cout << "EINVAL" << std::endl;
            if (errno == EMFILE) std::cout << "EMFILE" << std::endl;
            if (errno == ENAMETOOLONG) std::cout << "ENAMETOOLONG" << std::endl;
            if (errno == ENFILE) std::cout << "ENFILE" << std::endl;
            if (errno == ENOENT) std::cout << "ENOENT" << std::endl;
            return false;
        }
            
        //std::string filename = std::string("/proc/self/fd/") + std::to_string(fd);
        std::cout << "created in memory file: " << std::endl;
        
        // write elf to that file
        write(fd, data(), size());        
        std::cout << "wrote file" << std::endl;
        
        // make executable
        fchmod(fd, S_IXUSR | S_IWUSR | S_IRUSR);
        
        // close
        close(fd);
        
        // open and unlink (delete after execution ends)
        fd = open(filename.data(), O_RDONLY);
        unlink(filename.data());

        // execute from file handle
        std::cout << "execute" << std::endl;
        std::string fdFilename = std::string("/proc/self/fd/") + std::to_string(fd);
        execve(fdFilename.data(), argv, envp);
        
        return false; // exec only returns if there's an error
    }
    
    
    Segment::Segment(ElfFile& elfFile, int programHeaderIndex)
    : elfFile(elfFile), index(programHeaderIndex) {
        if (programHeaderIndex >= elfFile.header().e_phnum) {
            // error
        }
    }
    
    Elf32_Phdr& Segment::header() {
        int programHeaderOffset = elfFile.header().e_phoff;
        int programHeaderSize = elfFile.header().e_phentsize;
        char* elfData = elfFile.data();
        char* pointer = elfData + programHeaderOffset + programHeaderSize*index;
        return *reinterpret_cast<Elf32_Phdr*>(pointer);
    }

    const Elf32_Phdr& Segment::header() const {
        // TODO remove this copy
        int programHeaderOffset = elfFile.header().e_phoff;
        int programHeaderSize = elfFile.header().e_phentsize;
        char* elfData = elfFile.data();
        char* pointer = elfData + programHeaderOffset + programHeaderSize*index;
        return *reinterpret_cast<const Elf32_Phdr*>(pointer);
    }

}



















