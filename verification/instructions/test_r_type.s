# Tests: ADD, SUB, AND, OR, XOR, SLL, SRL, SLT
.text
    addi x1, x0, 20
    addi x2, x0, 12

    add x3, x1, x2      # 20 + 12 = 32
    sub x4, x1, x2      # 20 - 12 = 8
    and x5, x1, x2      # 20 & 12 (0x14 & 0x0C) = 4
    or  x6, x1, x2      # 20 | 12 (0x14 | 0x0C) = 28
    xor x7, x1, x2      # 20 ^ 12 (0x14 ^ 0x0C) = 24
    
    addi x8, x0, 2
    sll x9, x1, x8      # 20 << 2 = 80
    srl x10, x1, x8     # 20 >> 2 = 5
    
    slt x11, x1, x2     # 20 < 12 = 0
    slt x12, x2, x1     # 12 < 20 = 1