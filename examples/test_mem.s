.data
    # No data needed, we'll use the default data section address

.text
    # Load 100 (0x64) into x1
    addi x1, x0, 100        
    
    lui x2, 0x10000         # lui x2, 0x10000 (Hi 20 bits)
    # addi x2, x0, 268435456  # 268435456 is 0x10000000

    # Manually stall (with NOPs) to let addi write back to x1 and x2
    # before sw needs to read them in its ID stage.
    addi x0, x0, 0
    addi x0, x0, 0
    addi x0, x0, 0
    addi x0, x0, 0

    # Test Store Word:
    # sw rs2, imm(rs1) -> sw x1, 0(x2)
    # Store value of x1 (100) at address in x2 (0x10000000)
    sw x1, 0(x2)

    # Manually stall to let sw finish its memory write
    # before lw tries to read from the same address.
    addi x0, x0, 0
    addi x0, x0, 0
    addi x0, x0, 0
    addi x0, x0, 0
    addi x0, x0, 0

    # Test Load Word:
    # lw rd, imm(rs1) -> lw x3, 0(x2)
    # Load value from address in x2 (0x10000000) into x3
    lw x3, 0(x2)