# Tests: JAL, JALR
.text
    # Test JAL
    jal x1, target_jal  # x1 = PC+4
    addi x2, x0, 1      # Should skip
target_jal:
    addi x3, x0, 2      # x3 = 2
    
    # Test JALR
    # Load target address into x10
    la x10, target_jalr
    
    jalr x4, x10, 0     # x4 = PC+4, jump to target_jalr
    addi x5, x0, 3      # Should skip
target_jalr:
    addi x6, x0, 4      # x6 = 4