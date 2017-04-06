#
#
# this will generate the emulator .s file
outputFile = "gen/opcode-handlers.s"
handlerAlign = 5

def generateHandlers():
    handlers = [None]*(1<<16)
    return handlers

def generateInvalidOpcodeHandler(i):
    return "   # missing opcode handler %04x" % i

def generateSource():
    handlers = generateHandlers()
    assert len(handlers) == (1<<16)

    handlersCode = "\n".join(
"""

handler_{i:04x}:
    .align {handlerAlign}
{code}
""".format(i=i, handlerAlign=handlerAlign,
           code=h if h is not None else generateInvalidOpcodeHandler(i))
        for i, h in enumerate(handlers))
    
    code = r"""
    .syntax unified
    .thumb
    .global femuHandlers
    .align  15

%(handlersCode)s


    .global femuStart
    .thumb_func
    .type femuStart, %%function
femuStart:
    push {lr}
    push {r4-r11}
    push {r0}
    
    mov  r0, 1       @ stdout
	  ldr  r1, =msg    @ write buffer
	  mov  r2, 17      @ size
	  mov  r7, 4       @ write syscall
	  svc  0
    
    pop {r0}
    
    mov  r12, r0     @ set up pc
    ldr  r11, [r12]  @ current word
    
    pop  {r4-r11}
    pop  {pc}


    .section .rodata
msg:
    .ascii "inside emulator!\n"
    

""" % dict(handlersCode=handlersCode)

    print "write", len(code), "chars to", outputFile
    with open(outputFile, "w") as f:
        f.write(code)
    

if __name__ == "__main__":
    generateSource()
