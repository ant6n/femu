import collections
# this will generate the emulator .s file


####### HELPERS/OPCODE HELPERS ##########################################
# given a string, inserts macros or format values from the 'override' definition, aligns comments
# returns new string
def format(string, **override):
    formatDict = dict(_macros, **_aliases)
    formatDict.update(override)
    return alignComments(string.format(**formatDict))

# represents a 16-bit opcode
class Opcode(object):
    def __init__(self, opcode):
        assert 2**16 > opcode >= 0
        self.opcode = opcode
        self.po = opcode >> 8
        self.so = opcode & 0xff
        self.r  = self.po & 0b111 # assumes that lower 3 bits of the primary opcode are a register
        
    @staticmethod
    def fromBytes(po, so):
        assert 0 <= po < 256 and 0 <= so < 256
        return Opcode(po << 8 | so)

    # defines the asm code for this opcode
    def define(self, code, **kwargs):
        code = (
"""    .thumb_func
handler_{opcode:04x}: """.format(opcode=self.opcode)
            + format(code.strip("\n"), **kwargs).lstrip(' ')
        )
        handlers[self.opcode] = code

            
# yields all opcodes that start with (byte+r), r in [0, 7]
def byteOpcodesWithRegister(byte):
    for r in range(8):
        for so in range(256):
            yield Opcode.fromBytes(byte + r, so)

# yields all opcodes that start with (byte)
def byteOpcodes(byte):
    for so in range(256):
        yield Opcode.fromBytes(byte, so)


# append rodata
def rodata(code, **kwargs):
    _rodata.append(format(code, **kwargs))
_rodata = []

# append data
def data(code, **kwargs):
    _data.append(format(code, **kwargs))
_data = []


# append text
def text(code, **kwargs):
    _text.append(format(code, **kwargs))
_text = []


def function(text="", rodata="", data=""):
    globals()['text'](text)
    globals()['rodata'](rodata)
    globals()['data'](data)


# returns a new label with uniq name, with the given optional suffix
def label(suffix=""):
    label = "L%d" % _currentLabel
    _currentLabel += 1
    if len(suffix) > 0:
        label += "_" + suffix
    return label
_currentLabel = 0


# adds a macro with the given name to the list of macros
# will format the given code
# macros need to be multiple lines
# commensts the name of the macro on the first line
# TODO - use asm macro definitions instead?
def macro(name, code):
    code = format(code)
    assert code.count("\n") == 0 # TODO
    code += " - " if '@' in code else " @ "
    code += name
    assert name not in _macros
    _macros[name] = code
_macros = dict()


# adds an expression macro (usually nam -> value)
# aliases will also be added to the global name space
def alias(name, exp):
    globals()[name] = exp
    _aliases[name] = exp
_aliases = collections.OrderedDict()

def shared_constant(name, value):
    _shared_constants[name] = value
_shared_constants = collections.OrderedDict()


    
# given some asm code, will align the @ on the same level
def alignComments(code):
    def hasCode(line):
        return (len (line.split('@', 1)[0].strip()) > 0
                if "@" in line else
                len(line.strip()) > 0)
    def hasComment(line):
        return "@" in line
    
    lines = code.split("\n")
    try:
        width = max(len(line.split("@",1)[0].rstrip()) + 1
                    for line in lines if hasCode(line) and hasComment(line))
    except ValueError:
        return code
    return "\n".join(
        (line)
        if not hasComment(line) else
        ("    @ " + line.split('@', 1)[1].lstrip())
        if not hasCode(line) else
        (line.split("@", 1)[0].ljust(width) + "@" + line.split('@', 1)[1])
        for line in lines
    )
    


####### DEFINITIONS #####################################################
handlers = [None]*(1<<16) # opcode -> handler code
alias('handlerShift', 5) # number of bits per handler


registerAliases = collections.OrderedDict([
    ('eax', 'r7'), # mapping matches x86 and arm syscall registers
    ('ebx', 'r0'),
    ('ecx', 'r1'),
    ('edx', 'r2'),
    ('esi', 'r3'),
    ('edi', 'r4'),
    ('ebp', 'r5'),
    ('esp', 'r13'),
    
    ('scratch',    'r8'),  # scratch register
    ('result',     'r9'),  # last result as flag helper
    ('aux',        'r10'), # auxiliary flag helper
    
    ('eip',        'r6'),  # holds address of current instruction
    ('word',       'r12'), # opcode word
    
    ('handlerBase','r11'), # set all handler bits to 1 to override
    ('nextHandler','r14'),
])

shared_constant('FEMU_EXIT_SUCCESS', 0)
shared_constant('FEMU_EXIT_FAILURE', 1)
shared_constant('FEMU_EXIT_UNIMPLEMENTED_OPCODE', 2)
shared_constant('FEMU_EXIT_INT3', 3)


globals().update((k, k) for k in registerAliases)  # insert registers into global scope
eregs = [eax, ecx, edx, ebx, esp, ebp, esi, edi] # index -> ereg



macro('nextHandler1_0Byte', "uxth nextHandler, word") # extract lower bytes
macro('nextHandler1_1Byte', "ubfx nextHandler, word, 8, 16") # extract middle bytes
macro('nextHandler1_2Byte', "ubfx nextHandler, word, 16,16") # extract upper bytes
macro('nextHandler2',       "orr  nextHandler, handlerBase, nextHandler, lsl {handlerShift}") 

macro('nextWord_1Byte', "ldr  word, [eip, 1]!")
macro('nextWord_2Byte', "ldr  word, [eip, 2]!")
macro('nextWord_5Byte', "ldr  word, [eip, 5]!")
macro('branchNext',     "bx   nextHandler")








####### HANDLERS ########################################################

for opcode in range(2**16):
    Opcode(opcode).define("""@ missing opcode handler 0x{opcode:04x}
    b notImplementedFunction
    """, opcode=opcode)

for op in byteOpcodesWithRegister(0x58): # pop reg
    if op.r == 4: continue # esp
    op.define("""@ pop reg {reg}
    {nextHandler1_1Byte}
    {nextHandler2}
    {nextWord_1Byte}
    pop  {{ {reg} }}
    {branchNext}
    """, reg=eregs[op.r])

for op in byteOpcodes(0x68): # push imm32
    op.define("""@ push imm32
    {nextWord_5Byte}
    ldr  scratch, [eip, -4]
    {nextHandler1_0Byte}
    {nextHandler2}
    push {{ scratch }}
    {branchNext}
    """, reg=eregs[op.r])

for op in byteOpcodesWithRegister(0xB8): # mov reg, imm32
    op.define("""@ mov {reg}, imm32
    {nextWord_5Byte}
    ldr  scratch, [eip, -4]
    {nextHandler1_0Byte}
    {nextHandler2}
    mov {reg}, scratch
    {branchNext}
    """, reg=eregs[op.r])

for op in byteOpcodes(0xA3): # mov ax, [imm32]
    op.define("""@ mov ax, [imm32]
    {nextWord_5Byte}
    ldr  scratch, [eip, -4]
    {nextHandler1_0Byte}
    {nextHandler2}
    str eax, [scratch]
    {branchNext}
    """, reg=eregs[op.r])

    
for op in byteOpcodes(0x90): # nop
    op.define("""@ nop
    {nextHandler1_1Byte}
    {nextHandler2}
    {nextWord_1Byte}
    {branchNext}
    """)

for op in byteOpcodes(0xCC): # int3
    op.define("""@ int3
    {nextWord_1Byte}
    mov word, FEMU_EXIT_INT3
    b femuEnd
    """)    




## FLAGS ############################
##         ARM   X86
# Sign/N   31     7
# Zero     30     6
# Carry    29     0
# Overflow 28    11
# parity    -     2 (result)
# Adjust    -     4 (aux)
for op in byteOpcodes(0x9C): # pushfd
    op.define("""@ pushfd
    b pushfd_impl
    """)

function(text="""
    .thumb_func
    .type pushfd_impl, %function
pushfd_impl: @ pushfd
    {nextHandler1_1Byte}
    {nextHandler2}
    {nextWord_1Byte}
    
    ldr  scratch, =mem_arm_sp @ load arm stack at scratch
    ldr  scratch, [scratch]
    stmfd scratch!, {{r0-r3}} @ push registers (store multiple - full descending)
    
    get_eflags
    
    push {{ r0 }}
    
    ldmfd scratch, {{r0-r3}} @ restore regs
    {branchNext}
""")



for op in byteOpcodes(0x9D): # popfd
    op.define("""@ pushfd
    b popfd_impl
    """)
##         ARM   X86
# Sign/N   31     7
# Zero     30     6
# Carry    29     0
# Overflow 28    11
# parity    -     2 (result)
# Adjust    -     4 (aux)

function(text="""
    .global popfd_impl
    .thumb_func
    .type popfd_impl, %function
popfd_impl: @ popfd
    {nextHandler1_1Byte}
    {nextHandler2}
    {nextWord_1Byte}
    
    ldr  scratch, =mem_arm_sp @ load arm stack at scratch
    ldr  scratch, [scratch]
    stmfd scratch!, {{r0-r3}} @ push registers (store multiple - full descending)

    pop {{ r0 }} @ get flags from stack in r0
    
    @ register-stored flags
    ubfx result, r0, 2, 1  @ p-flag -> result, by copying bit 2 into result
    and  aux, r0, 0x10     @ aux-flag -> aux (since result has 0 bit result xor aux will be aux)
    
    @ memory-stored flags
    and  r3, r0, 0x0600     @ only DF, IF may be set (see line below for bit(1))
    orr  r3, r3, 0x0002     @ bit(1) will be set always
    orr  r3, r3, 0x0200     @ IF will be set always
    ldr  r1, =mem_eflags    @ store in memory-stored flags
    str  r3, [r1]
    
    @ NZCo flags -> CSPR        
    ubfx r2, r0,11, 1       @ r2 holds 0b000o
    ubfx r1, r0, 6, 2       @ r1 holds 0b00NZ    
    bfi  r2, r0, 1, 1       @ r2 holds 0b00Co
    lsl  r1, r1, 30         @ r1 is    0bNZ00...
    orr  r1, r1, r2, lsl 28 @ r1 is    0bNZCo0...
    msr  cpsr_f, r1         @ place r1 in status
    
    ldmfd scratch, {{r0-r3}} @ restore regs
    {branchNext}

""")
#
#for op in byteOpcodes(0x9D): # popfd
#    op.define("""@ popfd
#    {nextHandler1_1Byte}
#    {nextHandler2}
#    {nextWord_1Byte}
#    mov r0, r0
#    mov r0, r0
#    mov r0, r0
#    mov r0, r0
#    mov r0, r0
#    mov r0, r0
#    mov r0, r0
#    mov r0, r0
#    mov r0, r0
#    {branchNext}
#    """)
#
#    
#    
#
# should we pack aux and result together? - we only need the lowest bytes...
#
# arm->x86
# SZCO
#
#   read move the bit
# parity:
#
#
#
#
#
#

    
Opcode(0xcd80).define( # int 0x80
"""
    {nextHandler1_2Byte}
    svc  0  @ the registers are already in the right place, no mapping necessary
    {nextHandler2}
    {nextWord_2Byte}
    {branchNext}
""")

    
    
####### GENERATOR FUNCTIONS #############################################
# one string of all the handlers
def generateOpcodeHandlers():
    assert len(handlers) == (1<<16)
    
    opcodes = [(((i & 0xff) << 8) | ((i & 0xff00) >> 8))
               for i in range(len(handlers))] # shuffle bytes for endianness
    
    return "\n".join("""
    .align {handlerAlign}
{code}""".format(opcode=opcode,
                 handlerAlign=handlerShift,
                 code=handlers[opcode])
        for i,opcode in enumerate(opcodes))




####### ASSEMBLER HEADER #############################################
def generateHeader():
    aliases = "\n".join(
        f"    {name:11} .req {reg}"
        for name, reg in registerAliases.items()
    )
    comma = ','
    sharedConstants = "\n".join(
        f"    .equ {name+comma:20} {value}"
        for name, value in _shared_constants.items()
    )
    return f"""
    .syntax unified
    .thumb
    .extern writeHexByte

    @ aliases
{aliases}

    @ shared constants
{sharedConstants}

    .global femuHandlers
    .global fb      @ breakpoint
    
    @ register state
    .global stored_eip
    .global stored_eax
    .global stored_ebx
    .global stored_ecx
    .global stored_edx
    .global stored_esi
    .global stored_edi
    .global stored_ebp
    .global stored_esp
    .global stored_eflags

    @ other state
    .global stored_unimplemented_opcode

    .align  15

@ macros
@ place (x86 registers, esp, eip) in store_(reg), stores cspr flags
@ frees up at least registers r0-r7 as well as cspr
@ switch to arm stack - uses scratch. note: does not write stored_eflags
@ intention: switch to C-code, running code that uses a lot of registers
.macro store_state
    @ store state
    ldr scratch, =stored_eip; str eip, [scratch]
    ldr scratch, =stored_eax; str eax, [scratch]
    ldr scratch, =stored_ebx; str ebx, [scratch]
    ldr scratch, =stored_ecx; str ecx, [scratch]
    ldr scratch, =stored_edx; str edx, [scratch]
    ldr scratch, =stored_esi; str esi, [scratch]
    ldr scratch, =stored_edi; str edi, [scratch]
    ldr scratch, =stored_ebp; str ebp, [scratch]
    ldr scratch, =stored_esp; str esp, [scratch]
    
    mrs scratch, cpsr @ get status-register stored flag bits
    ldr sp, =mem_cspr @ address where to store them
    str scratch, [sp]
    
    @ restore stack and return
    ldr sp, =mem_arm_sp; ldr sp, [sp]
.endm

@ undoes store_state
@ load (x86 registers, esp, eip) from store_reg, load cspr flags
@ implied: switch to x86 stack) - uses scratch
.macro restore_state
    ldr sp, =mem_cspr   @ address where status is stored
    ldr scratch, [sp]
    msr cpsr_f, scratch @ get status-register stored flag bits

    ldr scratch, =stored_eip; ldr eip, [scratch]
    ldr scratch, =stored_eax; ldr eax, [scratch]
    ldr scratch, =stored_ebx; ldr ebx, [scratch]
    ldr scratch, =stored_ecx; ldr ecx, [scratch]
    ldr scratch, =stored_edx; ldr edx, [scratch]
    ldr scratch, =stored_esi; ldr esi, [scratch]
    ldr scratch, =stored_edi; ldr edi, [scratch]
    ldr scratch, =stored_ebp; ldr ebp, [scratch]
    ldr scratch, =stored_esp; ldr esp, [scratch]
.endm

// TODO macro store_flag_state? - restore_flag_state


@ macro reads eflags from cspr, aux, result, memory
@ and stores it in r0
@ uses r0-r3
.macro get_eflags
    ldr  r3, =mem_eflags @ get memory-stored flags
    ldr  r3, [r3]
    
    mrs  r0, cpsr       @ get status-register stored flags 
    ubfx r1, r0, 30, 2  @ r1 holds 0b00NZ
    ubfx r2, r0, 28, 1  @ r2 holds 0b000o 
    ubfx r0, r0, 29, 1  @ r0 holds 0b000C
   
    orr  r0, r0, r2, lsl 11 @ r0 holds 0bo000 0000 000C
    orr  r1, r3, r1, lsl 6  @ r1 holds mem_eflags | 0b NZ00 0000
    
    eor  r2, aux, result  @ r2 holds ...0b???A ????
    and  r2, r2, 0x10  @ r2 holds 0b000A 0000 
    
    eor  r3, result, result, lsr 4 @ combine parity
    eor  r3, r3, r3, lsl #2
    eor  r3, r3, r3, lsr #1        @ r3 holds 0b?P??
    and  r3, r3, 0x4
    
    orr  r3, r2  @ combine the various results
    orr  r0, r1
    orr  r0, r3  @ r0 holds eflags
.endm
    """





####### ASSEMBLER FUNCTIONS #############################################
function(
    rodata = r"""
msg:
    .ascii "starting emulation...\n"
""",
    data = r"""
@ emulator state
mem_arm_sp: .word 0
mem_eflags: .word 0x0202 @ the direction flag and reserved bit 1
mem_cspr:   .word 0      @ to store/restore cspr flag registers

@ register state
stored_eip: .word 0
stored_eax: .word 0
stored_ebx: .word 0
stored_ecx: .word 0
stored_edx: .word 0
stored_esi: .word 0
stored_edi: .word 0
stored_ebp: .word 0
stored_esp: .word 0
stored_eflags: .word 0


@other public values
stored_unimplemented_opcode: .word 0
    """,
    text = r"""
    # femuRun(void* pc, void* sp) -> result status
    .global femuRun
    .thumb_func
    .type femuRun, %function
femuRun:
    push {{lr}}
    push {{r4-r11}}
   
    @ print message
    push {{r0}}
    push {{r1}}
    mov  r0, 1       @ stdout
    ldr  r1, =msg    @ write buffer
    mov  r2, 22      @ size
    mov  r7, 4       @ write syscall
    svc  0
    pop {{r1}}
    pop {{r0}}

    @ store arm stack pointer
    ldr  r2, =mem_arm_sp
    str  sp, [r2]
    
    @ set up eip, x86 stack pointer (esp)
    mov  eip, r0
    mov  esp, r1
    
    @ set emulator helper registers
    mov  scratch, 0
    mov  result, 0
    mov  aux, 0
    ldr  handlerBase, =handler_0000
    
    @ set up word, nextHandler
    ldr  word, [eip]
    {nextHandler1_0Byte}
    {nextHandler2}
    
    @ set up emulated registers
    mov  eax, 0
    mov  ecx, 0
    mov  edx, 0
    mov  ebx, 0
    mov  ebp, 0
    mov  esi, 0
    mov  edi, 0
    
    @ set up flags
    msr cpsr_f, eax @ clear status flags
fb:
    @ start emulation
    {branchNext}
    
    
@ femuEnd expects return value to be stored in register 'word'
femuEnd:
    
    store_state

    get_eflags
    ldr r1, =stored_eflags
    str r0, [r1] @ store flags
    
    mov r0, word @ store return value in r0

    pop {{r4-r11}}
    pop {{pc}}
    """)

function(
    rodata = r"""
unimplemented_msg:
    .ascii "Unimplemented opcode: "
newline:
    .ascii "\n"
""",
    text = r"""
    .thumb_func
notImplementedFunction:
    store_state
    
    mov  r0, 1       @ stdout
    ldr  r1, =unimplemented_msg
    mov  r2, 22      @ size
    mov  r7, 4       @ write syscall
    svc  0
    
    ldr  r0, =stored_unimplemented_opcode
    ubfx r1, word, 0, 16
    str  r1, [r0]
    
    mov  r0, 1
    and  r1, word, 0xff
    bl   writeHexByte    
    
    mov  r0, 1
    ubfx r1, word, 8, 8
    bl   writeHexByte
    
    mov  r0, 1       @ stdout
    ldr  r1, =newline
    mov  r2, 1       @ size
    mov  r7, 4       @ write syscall
    svc  0
    
    restore_state
    
    mov  word, FEMU_EXIT_UNIMPLEMENTED_OPCODE

    b    femuEnd
""")


def generateSource(outputFile):    
    code = r"""
{header}


{opcodeHandlers}

{text}

    .section .data
{data}

    .section .rodata
{rodata}

    .section text

""".format(header = generateHeader(),
           opcodeHandlers = generateOpcodeHandlers(),
           text = "\n".join(_text),
           data = "\n".join(_data),
           rodata = "\n".join(_rodata))

    print("write", code.count("\n"), "lines to", outputFile)
    with open(outputFile, "w") as f:
        f.write(code)



##### GENERATE OTHER FILES #########################################################################
def generateGDBRegisterPrint(outputFilename):
    def replace(name):
        return (
            name
            .replace("handlerBase", "hBas")
            .replace("nextHandler", "nxtH")
            .replace("scratch", "scrat")
            .replace("result", "reslt")
        )
    
    items = [(replace(name), reg) for name, reg in registerAliases.items()]
    names = ["%s-%s" % (name, reg) for name, reg in items]
    lengths = [max(8, len(name)) for name in names]
    prints = []
    prints.append('printf "' + '|'.join(name.rjust(lengths[i])
                                        for i,name in enumerate(names)) + r'\n"' )
    prints.append('printf "' + '|'.join("%08X".rjust(length + 6 - 10)
                                        for length in lengths)
                  + r'\n", ' + ', '.join('$' + reg for _, reg in items) + '\n')
    for i, (name, reg) in enumerate(items):
        length = lengths[i]
        if i != 0:
            prints.append("echo |")
        prints.append("""if ${reg} >= 100000000
        printf "{hash}"
    else
        printf "{space}%8d", ${reg}
    end""".format(reg=reg, hash=length*'#', space=' '*(length-8)))
    prints.append(r'echo \n%s\n' % ('-'*(sum(lengths) + 1*(len(lengths)-1))))

    code = r"""
echo __ define print reg function ____\n

define reg
    {printStatements}
end

define sr
    si
    reg
end
""".format(printStatements="\n    ".join(prints))
    #print(code)
    print("write", code.count("\n"), "lines to", outputFilename)
    with open(outputFilename, "w") as f:
        f.write(code)
    

def generateSharedConstantsHeader(outputFilename):
    code = (
        """
#ifndef __shared_constants_h__
#define __shared_constants_h__


""" +
        "\n".join(
            f"#define {name:20} {value}" for name, value in _shared_constants.items()
        )
        + """


#endif /* defined(__shared_constants_h__) */
""")
    print("write", code.count("\n"), "lines to", outputFilename)
    with open(outputFilename, "w") as f:
        f.write(code)


##### MAIN #########################################################################################
if __name__ == "__main__":
    generateSource("gen/opcode-handlers.s")
    generateGDBRegisterPrint("gen/register-gdb-print")
    generateSharedConstantsHeader("gen/shared_constants.h")

    

