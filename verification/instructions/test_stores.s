# Tests: SW, SH, SB
# Verifies by storing, then loading back
.data
.align 3
store_target: .space 8
.text
    la x1, store_target
    
    addi x2, x0, 0xAABBCCDD
    addi x3, x0, 0x1234
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