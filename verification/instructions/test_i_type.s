# Tests: ADDI, ANDI, ORI, XORI, SLTI
.text
    addi x1, x0, 50     # x1 = 50
    addi x2, x1, 10     # x2 = 50 + 10 = 60
    
    andi x3, x1, 15     # x3 = 50 & 15 (0x32 & 0x0F) = 2
    ori  x4, x1, 15     # x4 = 50 | 15 (0x32 | 0x0F) = 63
    xori x5, x1, 15     # x5 = 50 ^ 15 (0x32 ^ 0x0F) = 61
    
    slti x6, x1, 100    # x6 = (50 < 100) = 1
    slti x7, x1, 50     # x7 = (50 < 50) = 0