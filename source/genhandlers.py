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
            "handler_{opcode:04x}: ".format(opcode=self.opcode)
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

        
####### DEFINITIONS #####################################################
handlers = [None]*(1<<16) # opcode -> handler code
handlerShift = 5 # number of bits per handler

eax = "r0"
ecx = "r1"
edx = "r2"
ebx = "r3"
esp = "r13"
ebp = "r5"
esi = "r6"
edi = "r7"

helper = "r8"  # scratch register
result = "r9"  # last result as flag helper
aux    = "r10" # auxiliary flag helper
handlerBase = "r11" # set all handler bits to 1 to override

pc = "r4"
word = "r12" # opcode word
nextHandler = "r14"

eregs = [eax, ecx, edx, ebx, esp, ebp, esi, edi]



nextHandler1_1Byte = format("ubfx {nextHandler}, {word}, 8, 16") # extract middle bytes
nextHandler2       = format("orr  {nextHandler}, {handlerBase}, {nextHandler}, lsl {handlerShift}") 

nextWord_1Byte     = format("ldr  {word}, [{pc}], 1")
branchNext         = format("bx   {nextHandler}")



####### HANDLERS ########################################################

for opcode in range(2**16):
    Opcode(opcode).define("""@ missing opcode handler 0x{opcode:04x}
    """, opcode=opcode)

for op in byteOpcodesWithRegister(0x58):
    if op.r == 4: continue # esp
    op.define("""@ pop reg {r}
    {nextHandler1_1Byte}
    {nextHandler2}
    {nextWord_1Byte}
    pop  {{ {reg} }}
    {branchNext}
    """, reg=eregs[op.r], r=op.r)


for op in byteOpcodes(0x90):
    op.define("""@ nop
    {nextHandler1_1Byte}
    {nextHandler2}
    {nextWord_1Byte}
    {branchNext}
    """)

    

####### PUBLIC FUNCTIONS ################################################
def generateHandlers():
    return handlers



def generateInit():
    return format("""
fb:
    mov  {pc}, r0      @ set up pc
    mov  r0,   0
    ldr  {word}, [{pc}]       @ set current word - next 3 bytes and NOP at lowest byte
    lsl  {word}, {word}, 8
    add  {word}, {word}, 0x90
    ldr  {nextHandler}, =handler_9000
    bx   {nextHandler}
""")
