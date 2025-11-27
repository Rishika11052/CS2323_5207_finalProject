# Tests: BEQ, BNE, BLT, BGE with data hazards and
# "poison pill" instructions to verify flush logic.
.text
    # --- Setup ---
    lui x5, 0x10000 
    addi x3, x0, 20
    addi x4, x0, 30
    
    # --- Test 1: BEQ (Taken) with EX -> EX Forwarding ---
    addi x1, x0, 10
    addi x2, x0, 10
    beq x1, x2, target1 # TAKEN. 
    addi x10, x0, 1 # "Poison Pill 1"
target1:
    
    # --- Test 2: BEQ (Not Taken) with EX -> EX Forwarding ---
    addi x1, x0, 99     
    beq x1, x3, target2 # NOT TAKEN.
    addi x11, x0, 4 # Should run.
target2:
    
    # --- Test 3: BNE (Taken) with MEM -> EX Forwarding ---
    addi x1, x0, 10 # x1 is 10
    bne x1, x3, target3 # TAKEN 
    addi x12, x0, 1 # "Poison Pill 2" 
target3:
    
    # --- Test 4: BNE (Not Taken) ---
    bne x1, x2, target4 # NOT TAKEN 
    addi x13, x0, 4 # Should run.
target4:
    
    # --- Test 5: BLT (Taken) with Load-Use STALL ---
    sw x3, 0(x5)        
    lw x1, 0(x5)        
    blt x1, x4, target5 
    addi x14, x0, 1 # "Poison Pill 3"
target5:
    
    # --- Test 6: BLT (Not Taken) ---
    blt x3, x1, target6 # NOT TAKEN 
    addi x15, x0, 4 # Should run.
target6:
    
    # --- Test 7: BGE (Taken) ---
    bge x3, x1, target7 # TAKEN
    addi x16, x0, 1 # "Poison Pill 4" 
target7:
    
    # --- Test 8: BGE (Not Taken) with EX -> EX Forwarding ---
    addi x1, x0, 50     
    bge x3, x1, target8 # NOT TAKEN 
    addi x17, x0, 4 # Should run.
target8:

# ==============================================================
# TEST 1: The Backward Loop (Standard Loop)
# Iterate 5 times.
# Backward Branch: Taken 4 times, Not Taken 1 time.
#
# NONE:    4 Taken * 2 stalls = 8 Stalls
# STATIC:  Predicts Taken. 4 Hits. 1 Miss (Exit). = 1 Stall
# DYNAMIC: 1 Miss (Warmup) + 3 Hits + 1 Miss (Exit) = 2 Stalls
# ==============================================================
    
    addi x1, x0, 5 # Loop counter = 5
    addi x10, x0, 0 # Accumulator

loop_bk:
    addi x10, x10, 1 # Do work
    addi x1, x1, -1 # Decrement
    bne x1, x0, loop_bk # Backward Branch

# ==============================================================
# TEST 2: The Repeated Forward Taken Branch
# We repeat a "Forward Taken" jump 5 times.
#
# NONE:    5 Taken * 2 stalls = 10 Stalls
# STATIC:  Predicts Not Taken (Forward). All 5 are Misses. = 5 Stalls
# DYNAMIC: 1 Miss (Warmup) + 4 Hits (Learned). = 1 Stall
# ==============================================================

addi x2, x0, 5 # Outer counter = 5
addi x11, x0, 0 # Accumulator

loop_fw_test:
    addi x2, x2, -1
    
    beq x0, x0, skip_target 
    
    addi x11, x11, 100  # (Poison pill)

skip_target:
    addi x11, x11, 1 # Do work
    
    bne x2, x0, loop_fw_test