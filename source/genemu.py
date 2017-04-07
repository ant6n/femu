import collections
# this will generate the emulator .s file


####### HELPERS/OPCODE HELPERS ##########################################
def format(string, **override):
    formatDict = dict(globals(), **override)
    return string.format(**formatDict)

# represents a 16-bit opcode
class Opcode(object):
    def __init__(self, opcode):
        assert 2**16 > opcode >= 0
        self.opcode = opcode
        self.po = opcode >> 8
        self.so = opcode & 0xff
        self.r  = self.po & 0b111
        
    @staticmethod
    def fromBytes(po, so):
        assert 0 <= po < 256 and 0 <= so < 256
        return Opcode(po << 8 | so)

    # defines the asm code for this opcode
    def define(self, code, **kwargs):
        handlers[self.opcode] = (
"""    .thumb_func
handler_{opcode:04x}: """.format(opcode=self.opcode)
            + format(code.strip("\n"), **kwargs)
        )
            
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
_rodata = [] # defines extra rodata

# append text
def text(code, **kwargs):
    _text.append(format(code, **kwargs))
_text = [] # defines extra text


# returns a label with the given optional suffix
def label(suffix=""):
    label = "L%d" % _currentLabel
    _currentLabel += 1
    if len(suffix) > 0:
        label += "_" + suffix
    return label
_currentLabel = 0







####### DEFINITIONS #####################################################
handlers = [None]*(1<<16) # opcode -> handler code
handlerShift = 5 # number of bits per handler

registerAliases = collections.OrderedDict([
    ('eax', 'r0'),
    ('ecx', 'r1'),
    ('edx', 'r2'),
    ('ebx', 'r3'),
    ('esp', 'r13'),
    ('ebp', 'r5'),
    ('esi', 'r6'),
    ('edi', 'r7'),
    
    ('scratch',    'r8'),  # scratch register
    ('result',     'r9'),  # last result as flag helper
    ('aux',        'r10'), # auxiliary flag helper
    ('handlerBase','r11'), # set all handler bits to 1 to override
    
    ('eip',        'r4'),
    ('word',       'r12'), # opcode word
    ('nextHandler','r14'),
])

globals().update((k, k) for k in registerAliases)  # insert registers into global scope
eregs = [eax, ecx, edx, ebx, esp, ebp, esi, edi] # index -> ereg




nextHandler1_1Byte = format("ubfx nextHandler, word, 8, 16") # extract middle bytes
nextHandler2       = format("orr  nextHandler, handlerBase, nextHandler, lsl {handlerShift}") 

nextWord_1Byte     = format("ldr  word, [eip], 1")
branchNext         = format("bx   nextHandler")








####### HANDLERS ########################################################

for opcode in range(2**16):
    Opcode(opcode).define("""@ missing opcode handler 0x{opcode:04x}
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


for op in byteOpcodes(0x90): # nop
    op.define("""@ nop
    {nextHandler1_1Byte}
    {nextHandler2}
    {nextWord_1Byte}
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


def generateHeader():
    aliases = "\n".join(
        "    {name:11} .req {reg}".format(name=name, reg=reg)
        for name, reg in registerAliases.items()
    )
    return """
    .syntax unified
    .thumb

{aliases}

    .global femuHandlers
    .global fb      @ breakpoint
    .align  15

    """.format(aliases=aliases)


def generateFemuFunction():
    return format(r"""
    .global femuStart
    .thumb_func
    .type femuStart, %function
femuStart:
    push {{lr}}
    push {{r4-r11}}
   
    @ print message
    push {{r0}}
    mov  r0, 1       @ stdout
    ldr  r1, =msg    @ write buffer
    mov  r2, 17      @ size
    mov  r7, 4       @ write syscall
    svc  0
    pop {{r0}}

fb:    
    @ set emulator helper registers
    mov  scratch, 0
    mov  result, 0
    mov  aux, 0
    ldr  handlerBase, =handler_0000
    
    @ set eip, word, nextHandler -- for a NOP
    mov  eip, r0
    ldr  word, [eip]       @ set current word - next 3 bytes and NOP at lowest byte
    lsl  word, word, 8
    add  word, word, 0x90
    ldr  nextHandler, =handler_9000
    
    @ set up emulated registers
    @esp stays the same
    mov  eax, 0
    mov  ecx, 0
    mov  edx, 0
    mov  ebx, 0
    mov  ebp, 0
    mov  esi, 0
    mov  edi, 0
    
    @ start emulation
    bx   nextHandler
    
    
    pop {{r4-r11}}
    pop {{pc}}
    
    .section .rodata
msg:
    .ascii "inside emulator!\n"
    """)


def generateSource(outputFile):    
    code = r"""
{header}
{opcodeHandlers}
{femuFunction}

{text}

    .section .rodata
{rodata}

""".format(header = generateHeader(),
           opcodeHandlers = generateOpcodeHandlers(),
           femuFunction = generateFemuFunction(),
           text = "\n".join(_text),
           rodata = "\n".join(_rodata))

    print "write", code.count("\n"), "lines to", outputFile
    with open(outputFile, "w") as f:
        f.write(code)




def generateGDBRegisterPrint(outputFile):
    
    printStatements = "\n".join(r"""
    printf "  "
    echo \033[32m
    printf " {name:>11} - {reg:<3}:"
    echo \033[0m
    printf "   0x%08X (%d) \n", ${reg}, ${reg}""".
        format(name=name, reg=reg)
        for name, reg in registerAliases.items())
    
    code = r"""
echo __ define print reg function ____\n

define reg
    {printStatements}
end
""".format(printStatements=printStatements)
    print "write", code.count("\n"), "lines to", outputFile
    with open(outputFile, "w") as f:
        f.write(code)
    
    

if __name__ == "__main__":
    generateSource("gen/opcode-handlers.s")
    generateGDBRegisterPrint("gen/register-gdb-print")


    
