import collections
# this will generate the emulator .s file


####### HELPERS/OPCODE HELPERS ##########################################
# into given string, inserts macros or format values from the override
# align comments
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


# returns a label with the given optional suffix
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
_aliases = dict()

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

globals().update((k, k) for k in registerAliases)  # insert registers into global scope
eregs = [eax, ecx, edx, ebx, esp, ebp, esi, edi] # index -> ereg



macro('nextHandler1_0Byte', "ubfx nextHandler, word, 0, 16") # extract lower bytes
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

for op in byteOpcodes(0x90): # nop
    op.define("""@ nop
    {nextHandler1_1Byte}
    {nextHandler2}
    {nextWord_1Byte}
    {branchNext}
    """)

for op in byteOpcodes(0x68): # push imm32
    op.define("""@ push imm32
    {nextWord_5Byte}
    ldr  scratch, [eip, -4]
    {nextHandler1_0Byte}
    {nextHandler2}
    push {{ scratch }}
    {branchNext}
    """, reg=eregs[op.r])

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


def generateHeader():
    aliases = "\n".join(
        "    {name:11} .req {reg}".format(name=name, reg=reg)
        for name, reg in registerAliases.items()
    )
    return """
    .syntax unified
    .thumb
    .extern writeHexByte

{aliases}

    .global femuHandlers
    .global fb      @ breakpoint
    .align  15

    """.format(aliases=aliases)


def generateFemuFunction():
    rodata(r"""
msg:
    .ascii "starting emulation...\n"
""")
    data(r"""
stack_pointer:
    .word 0
    """)
    
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
    mov  r2, 22      @ size
    mov  r7, 4       @ write syscall
    svc  0
    pop {{r0}}
    
    ldr  r2, =stack_pointer
    str  sp, [r2]
    
    @ set emulator helper registers
    mov  scratch, 0
    mov  result, 0
    mov  aux, 0
    ldr  handlerBase, =handler_0000
    
    @ set eip, word, nextHandler
    mov  eip, r0
    ldr  word, [eip]
    {nextHandler1_0Byte}
    {nextHandler2}
    
    @ set up emulated registers
    @esp stays the same for now
    mov  eax, 0
    mov  ecx, 0
    mov  edx, 0
    mov  ebx, 0
    mov  ebp, 0
    mov  esi, 0
    mov  edi, 0
fb:    
    @ start emulation
    {branchNext}
    
    
femuEnd:
    ldr sp, =stack_pointer
    ldr sp, [sp]
    pop {{r4-r11}}
    pop {{pc}}
    """)

def generateNotImplementedFunction():
    rodata(r"""
unimplemented_msg:
    .ascii "Unimplemented opcode: "
newline:
    .ascii "\n"
""")
    return format("""
    .thumb_func
notImplementedFunction:
    mov  r0, 1       @ stdout
    ldr  r1, =unimplemented_msg
    mov  r2, 22      @ size
    mov  r7, 4       @ write syscall
    svc  0

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
    
    b    femuEnd
""")


def generateSource(outputFile):    
    code = r"""
{header}
{opcodeHandlers}
{femuFunction}
{notImplementedFunction}

{text}
    .section .data
{data}

    .section .rodata
{rodata}

""".format(header = generateHeader(),
           opcodeHandlers = generateOpcodeHandlers(),
           femuFunction = generateFemuFunction(),
           notImplementedFunction = generateNotImplementedFunction(),
           text = "\n".join(_text),
           data = "\n".join(_data),
           rodata = "\n".join(_rodata))

    print "write", code.count("\n"), "lines to", outputFile
    with open(outputFile, "w") as f:
        f.write(code)




def generateGDBRegisterPrint(outputFile):
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
    print code
    print "write", code.count("\n"), "lines to", outputFile
    with open(outputFile, "w") as f:
        f.write(code)
    
    

if __name__ == "__main__":
    generateSource("gen/opcode-handlers.s")
    generateGDBRegisterPrint("gen/register-gdb-print")


    

