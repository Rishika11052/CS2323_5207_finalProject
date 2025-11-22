# Tests: JAL, JALR
# (With 2-cycle "poison pills" to test naive pipeline)
.text
# --- Test JAL ---
# jal is at PC 0
    jal x1, target_jal  # x1=PC+4=4. Jumps to target_jal (PC 12)
    addi x2, x0, 1     # x2 = 1 (should be skipped)
target_jal:             # PC=12
    addi x3, x0, 2      # x3 = 2
    
# --- Test JALR ---
# auipc is at PC 16
    auipc x10, 0        # x10 = 16 (address of this instruction)
# addi is at PC 20. target_jalr is at PC 32.
# The offset is 32 - 16 = 16.
    addi  x10, x10, 16  # x10 = 16 + 16 = 32
# jalr is at PC 24
    jalr x4, 0(x10)     # x4=PC+4=28. Jumps to x10 (32).
    addi x10, x0, 3     # x10 = 3 (should be skipped)
target_jalr:            # PC=36
    addi x6, x0, 4      # x6 = 4