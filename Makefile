CC-MAIN := g++
# if we are on aarch64, use armhf cross-compiler
ifeq ($(shell uname -m),aarch64)
	CC-FEMU := arm-linux-gnueabihf-gcc
else
	CC-FEMU := gcc
endif

FEMU-INCLUDE := source/*.h gen/shared_constants.h
MAIN-INCLUDE := source/*.h

FEMU-FLAGS := -ggdb -c -static -nostdlib -ffreestanding -std=gnu99 -fPIC -Os -Igen
MAIN-FLAGS := -ggdb -c -std=c++11 -O0

FEMU-SRCS := emu-main.c emu-lib.c
MAIN-SRCS := main.cpp elf-file.cpp


all: femu femu-inject

bin/elf-file.o: source/elf-file.cpp $(MAIN-INCLUDE)
	@mkdir -p bin
	$(CC-MAIN) $(MAIN-FLAGS) source/elf-file.cpp -o bin/elf-file.o

bin/main.o: source/main.cpp $(MAIN-INCLUDE)
	@mkdir -p bin
	$(CC-MAIN) $(MAIN-FLAGS) source/main.cpp -o bin/main.o

bin/emu-main.o: source/emu-main.c $(FEMU-INCLUDE)
	$(CC-FEMU) $(FEMU-FLAGS) source/emu-main.c -o bin/emu-main.o

bin/emu-lib.o: source/emu-lib.c $(FEMU-INCLUDE)
	$(CC-FEMU) $(FEMU-FLAGS) source/emu-lib.c -o bin/emu-lib.o

gen/opcode-handlers.s gen/register-gdb-print gen/shared_constants.h: source/genemu.py
	python3.7 source/genemu.py

bin/opcode-handlers.o: gen/opcode-handlers.s
	@mkdir -p bin
	$(CC-FEMU) $(FEMU-FLAGS) gen/opcode-handlers.s -o bin/opcode-handlers.o

femu: bin/elf-file.o bin/main.o
	$(CC-MAIN) bin/elf-file.o bin/main.o -o femu

femu-inject: bin/emu-main.o bin/emu-lib.o bin/opcode-handlers.o
	$(CC-FEMU) -static -nostdlib -ffreestanding bin/emu-main.o bin/opcode-handlers.o bin/emu-lib.o -o femu-inject


.PHONY: depend clean

clean:
	rm bin/*.o gen/* femu-inject femu

test: all
	python3 test/runtests.py

