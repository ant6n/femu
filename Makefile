
CC-FEMU := gcc
CC-MAIN := g++

FEMU-INCLUDE := source/*.h
MAIN-INCLUDE := source/*.h

FEMU-FLAGS := -ggdb -c -static -nostdlib -std=gnu99 -fPIC -Os
MAIN-FLAGS := -ggdb -c -std=c++11 -O0

FEMU-SRCS := emu-main.c emu-lib.c
MAIN-SRCS := main.cpp elf-file.cpp


all: femu femu-inject

bin/elf-file.o: source/elf-file.cpp $(MAIN-INCLUDE)
	$(CC-MAIN) $(MAIN-FLAGS) source/elf-file.cpp -o bin/elf-file.o

bin/main.o: source/main.cpp $(MAIN-INCLUDE)
	$(CC-MAIN) $(MAIN-FLAGS) source/main.cpp -o bin/main.o

bin/emu-main.o: source/emu-main.c $(FEMU-INCLUDE)
	$(CC-FEMU) $(FEMU-FLAGS) source/emu-main.c -o bin/emu-main.o

bin/emu-lib.o: source/emu-lib.c $(FEMU-INCLUDE)
	$(CC-FEMU) $(FEMU-FLAGS) source/emu-lib.c -o bin/emu-lib.o

gen/opcode-handlers.s gen/register-gdb-print: source/genemu.py
	python3 source/genemu.py

bin/opcode-handlers.o: gen/opcode-handlers.s
	$(CC-FEMU) $(FEMU-FLAGS) gen/opcode-handlers.s -o bin/opcode-handlers.o

femu: bin/elf-file.o bin/main.o
	$(CC-MAIN) bin/elf-file.o bin/main.o -o femu

femu-inject: bin/emu-main.o bin/emu-lib.o bin/opcode-handlers.o
	$(CC-FEMU) -static -nostdlib bin/emu-main.o bin/opcode-handlers.o bin/emu-lib.o -o femu-inject


.PHONY: depend clean

clean:
	rm bin/*.o gen/* femu-inject femu
