# hazard_test.s
#
# This file tests both ALU-ALU forwarding and Load-Use stalls.

.section .text
.globl _start
_start:

    # --- Test 1: ALU-ALU Forwarding (EX/MEM Hazard) ---
    # We will calculate: x2 = (5 + 5) - 5 = 5
    
    addi x1, x0, 5      # x1 = 5
    add  x2, x1, x1      # x2 = 5 + 5 = 10
                         # HAZARD: 'sub' needs x2, but 'add' is in EX stage.
    sub  x3, x2, x1      # x3 = 10 - 5 = 5
                         # HAZARD: 'add' needs x3, but 'sub' is in EX stage.
    add  x4, x3, x0      # x4 = 5 + 0 = 5 (Final result for this test)


    # --- Test 2: Load-Use Stall ---
    # We will store 42 to memory, load it, and add 8.
    
    addi x10, x0, 42    # x10 = 42
    sd x10, 0(x0)       # Store 42 to memory address 0
                        # (sd has no rd, so no data hazard)
    
    lw x11, 0(x0)       # x11 = 42 (Load 42 from memory)
                        # HAZARD: 'addi' needs x11, but 'lw' is in MEM stage.
    addi x12, x11, 8    # x12 = 42 + 8 = 50 (Final result for this test)