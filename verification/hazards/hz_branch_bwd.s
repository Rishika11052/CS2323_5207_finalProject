# Hazard: Control (Backward Branch)
# Fails in: naive_pipeline
# Passes in: stalls_only, forwarding, ...
.text
    addi x1, x0, 3      # Loop counter
    addi x10, x0, 0     # Test register
loop:
    addi x10, x10, 1    # Should run 3 times
    addi x1, x1, -1
    bne x1, x0, loop    # Backward branch
                        # Mode 2 fetches next instr, fails
                        # Mode 3+ flushes
    
    addi x11, x0, 99    # Instruction after loop
    
    # Final: x1=0, x10=3, x11=99