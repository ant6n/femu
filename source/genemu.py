import genhandlers
#
#
# this will generate the emulator .s file
outputFile = "gen/opcode-handlers.s"


def generateSource():
    handlers = genhandlers.generateHandlers()
    assert len(handlers) == (1<<16)
    
    opcodes = [(((i & 0xff) << 8) | ((i & 0xff00) >> 8)) for i in range(len(handlers))] # shuffle
    
    handlersCode = "\n".join(
"""
    .align {handlerAlign}
{code}""".format(opcode=opcode, handlerAlign=genhandlers.handlerShift,
           code=handlers[opcode])
        for i,opcode in enumerate(opcodes))
    
    code = r"""
    .syntax unified
    .thumb
    .global femuHandlers
    .global fb      @ breakpoint
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

%(initCode)s
    
    pop {r4-r11}
    pop {pc}
    
    .section .rodata
msg:
    .ascii "inside emulator!\n"
    

""" % dict(handlersCode=handlersCode, initCode=genhandlers.generateInit())

    print "write", len(code), "chars to", outputFile
    with open(outputFile, "w") as f:
        f.write(code)
    

if __name__ == "__main__":
    generateSource()
