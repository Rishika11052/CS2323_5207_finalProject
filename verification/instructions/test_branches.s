# Tests: BEQ, BNE, BLT, BGE with data hazards and
# "poison pill" instructions to verify flush logic.
.text
    # --- Setup ---
    lui x5, 0x10000 
    addi x3, x0, 20
    addi x4, x0, 30
    
    # --- Test 1: BEQ (Taken) with EX -> EX Forwarding ---
    # 'beq' needs x1 and x2 right after they're written.
    addi x1, x0, 10
    addi x2, x0, 10
    beq x1, x2, target1 # TAKEN. Needs x1 (from MEM) and x2 (from EX)
    addi x10, x0, 1     # "Poison Pill 1" (flushed in Mode 3+)
target1:
    
    # --- Test 2: BEQ (Not Taken) with EX -> EX Forwarding ---
    # Change x1 right before the branch to test forwarding.
    addi x1, x0, 99     # x1 is now 99
    beq x1, x3, target2 # NOT TAKEN. Needs x1 (from EX)
    addi x11, x0, 4     # Should run in all modes. x11 = 4
target2:
    
    # --- Test 3: BNE (Taken) with MEM -> EX Forwarding ---
    # Add a nop to force a MEM-stage forward.
    addi x1, x0, 10     # x1 is 10
    bne x1, x3, target3 # TAKEN (10 != 20). Needs x1 (from MEM)
    addi x12, x0, 1     # "Poison Pill 1" (flushed in Mode 3+)
target3:
    
    # --- Test 4: BNE (Not Taken) ---
    bne x1, x2, target4 # NOT TAKEN (10 == 10)
    addi x13, x0, 4     # Should run. x13 = 4
target4:
    
    # --- Test 5: BLT (Taken) with Load-Use STALL ---
    # This must force a 1-cycle stall, even with forwarding.
    sw x3, 0(x5)        # Store 20
    lw x1, 0(x5)        # Load 20 into x1
    blt x1, x4, target5 # TAKEN (20 < 30). Must stall 1 cycle for x1.
    addi x14, x0, 1     # "Poison Pill 1" (flushed in Mode 3+)
target5:
    
    # --- Test 6: BLT (Not Taken) ---
    blt x3, x1, target6 # NOT TAKEN (20 < 20 is false)
    addi x15, x0, 4     # Should run. x15 = 4
target6:
    
    # --- Test 7: BGE (Taken) ---
    bge x3, x1, target7 # TAKEN (20 >= 20 is true)
    addi x16, x0, 1     # "Poison Pill 1" (flushed in Mode 3+)
target7:
    
    # --- Test 8: BGE (Not Taken) with EX -> EX Forwarding ---
    addi x1, x0, 50     # x1 is now 50
    bge x3, x1, target8 # NOT TAKEN (20 >= 50 is false). Needs x1 (from EX)
    addi x17, x0, 4     # Should run. x17 = 4
target8: