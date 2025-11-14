# Tests: BEQ, BNE, BLT, BGE
.text
    addi x1, x0, 10
    addi x2, x0, 10
    addi x3, x0, 20
    
    # BEQ
    addi x10, x0, 0
    beq x1, x2, target1 # Should take
    addi x10, x0, 1     # Should skip
target1:
    addi x10, x10, 2    # x10 = 2
    
    beq x1, x3, target2 # Should not take
    addi x10, x10, 4    # Should run. x10 = 6
target2:
    
    # BNE
    addi x11, x0, 0
    bne x1, x3, target3 # Should take
    addi x11, x0, 1     # Should skip
target3:
    addi x11, x11, 2    # x11 = 2
    
    bne x1, x2, target4 # Should not take
    addi x11, x11, 4    # Should run. x11 = 6
target4:
    
    # BLT
    addi x12, x0, 0
    blt x1, x3, target5 # (10 < 20) -> Should take
    addi x12, x0, 1     # Should skip
target5:
    addi x12, x12, 2    # x12 = 2
    
    blt x3, x1, target6 # (20 < 10) -> Should not take
    addi x12, x12, 4    # Should run. x12 = 6
target6:
    
    # BGE
    addi x13, x0, 0
    bge x3, x1, target7 # (20 >= 10) -> Should take
    addi x13, x0, 1     # Should skip
target7:
    addi x13, x13, 2    # x13 = 2
    
    bge x1, x3, target8 # (10 >= 20) -> Should not take
    addi x13, x13, 4    # Should run. x13 = 6
target8: