# Tests: LW, LH, LB, LHU, LBU
.text
    lui x1, 0x10000    # Load upper immediate to x1
    
    lw x2, 0(x1)        # x2 = 0x12345678
    lh x3, 0(x1)        # x3 = 0x00005678 (sign-extended from 0x5678)
    lb x4, 0(x1)        # x4 = 0x00000078 (sign-extended from 0x78)    
    lhu x5, 0(x1)       # x5 = 0x00005678 (zero-extended)
    lbu x6, 0(x1)       # x6 = 0x00000078 (zero-extended)