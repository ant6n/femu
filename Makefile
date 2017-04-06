
CC-FEMU := gcc-4.7
CC-MAIN := g++-4.7

FEMU-INCLUDE := source/*.h
MAIN-INCLUDE := source/*.h

FEMU-FLAGS := -g -c -static -nostdlib -std=gnu99 -fPIC -Os
MAIN-FLAGS := -g -c -std=c++11 -O0

FEMU-SRCS := emu-entry.c
MAIN-SRCS := main.cpp elf-file.cpp


all: femu femu-inject

bin/elf-file.o: source/elf-file.cpp $(MAIN-INCLUDE)
	$(CC-MAIN) $(MAIN-FLAGS) source/elf-file.cpp -o bin/elf-file.o

bin/main.o: source/main.cpp $(MAIN-INCLUDE)
	$(CC-MAIN) $(MAIN-FLAGS) source/main.cpp -o bin/main.o

bin/emu-entry.o: source/emu-entry.c $(FEMU-INCLUDE)
	$(CC-FEMU) $(FEMU-FLAGS) source/emu-entry.c -o bin/emu-entry.o

gen/opcode-handlers.s: source/generate-emu.py
	python source/generate-emu.py

bin/opcode-handlers.o: gen/opcode-handlers.s
	$(CC-FEMU) $(FEMU-FLAGS) gen/opcode-handlers.s -o bin/opcode-handlers.o

femu: bin/elf-file.o bin/main.o
	$(CC-MAIN) bin/elf-file.o bin/main.o -o femu

femu-inject: bin/emu-entry.o bin/opcode-handlers.o
	$(CC-FEMU) -static -nostdlib bin/emu-entry.o bin/opcode-handlers.o -o femu-inject


.PHONY: depend clean

clean:
	rm bin/*.o gen/* femu-inject femu
