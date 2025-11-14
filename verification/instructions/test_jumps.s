# Tests: JAL, JALR
# (With NOPs to test naive pipeline)
.text
# --- Test JAL ---
# jal is at PC 0
    jal x1, target_jal  # Jumps to PC 20
    addi x0, x0, 0      # NOP (at PC+4) <-- In Mode 2, this will execute
    addi x0, x0, 0      # NOP (at PC+8)
    addi x0, x0, 0      # NOP (at PC+12)
    addi x2, x0, 1      # (at PC+16) <-- This is never reached
target_jal:             # (at PC+20)
    addi x3, x0, 2      # x3 = 2
    
# --- Test JALR ---
# auipc is at PC 24
    auipc x10, 0        # x10 = 24
# addi is at PC 28. target_jalr is at PC 64.
# Offset is 64 - 24 = 40.
    addi  x10, x10, 40  # x10 = 24 + 40 = 64
    
    addi x0, x0, 0      # NOP (at PC+32)
    addi x0, x0, 0      # NOP (at PC+36)
    addi x0, x0, 0      # NOP (at PC+40)
# jalr is at PC 44
    jalr x4, 0(x10)     # x4 = PC+4 = 48. Jumps to x10 (64).
    addi x0, x0, 0      # NOP (at PC+48) <-- In Mode 2, this will execute
    addi x0, x0, 0      # NOP (at PC+52)
    addi x0, x0, 0      # NOP (at PC+56)
    addi x5, x0, 3      # (at PC+60) <-- This is never reached
target_jalr:            # (at PC+64)
    addi x6, x0, 4      # x6 = 4