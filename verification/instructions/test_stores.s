# Tests: SW, SH, SB
# Verifies by storing, then loading back
.text
    lui x1, 0x10000    # Load upper immediate to x1
    
    addi x2, x0, 0x123
    addi x3, x0, 0x123
    addi x4, x0, 0x56
    
    # Store word
    sw x2, 0(x1)
    lw x5, 0(x1)        # x5 should be 0xAABBCCDD
    
    # Store half-word (overwrites low 2 bytes)
    sh x3, 0(x1)        # Memory is now 0xAABB1234
    lw x6, 0(x1)        # x6 should be 0xAABB1234
    
    # Store byte (overwrites low byte)
    sb x4, 0(x1)        # Memory is now 0xAABB1256
    lw x7, 0(x1)        # x7 should be 0xAABB1256