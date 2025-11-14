# Tests: ADD, SUB, AND, OR, XOR, SLL, SRL, SLT
.text
    addi x1, x0, 20     # Answer in hex 0x14
    addi x2, x0, 12     # Answer in hex 0x0C

    add x3, x1, x2      # 20 + 12 = 32 (0x14 + 0x0C = 0x20)
    sub x4, x1, x2      # 20 - 12 = 8 (0x14 - 0x0C = 0x08)
    and x5, x1, x2      # 20 & 12 (0x14 & 0x0C) = 4 (0x04)
    or  x6, x1, x2      # 20 | 12 (0x14 | 0x0C) = 28 (0x1C)
    xor x7, x1, x2      # 20 ^ 12 (0x14 ^ 0x0C) = 24 (0x18)
    
    addi x8, x0, 2      # Answer in x8 : 2 (0x02)
    sll x9, x1, x8      # 20 << 2 = 80 (0x14 << 0x02 = 0x50)
    srl x10, x1, x8     # 20 >> 2 = 5 (0x14 >> 0x02 = 0x05)
    
    slt x11, x1, x2     # 20 < 12 = 0 (0x14 < 0x0C = 0)
    slt x12, x2, x1     # 12 < 20 = 1 (0x0C < 0x14 = 1)