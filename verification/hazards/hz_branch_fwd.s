# Hazard: Control (Forward Branch)
# Fails in: naive_pipeline
# Passes in: stalls_only, forwarding, ...
.text
    addi x1, x0, 0
    beq x0, x0, target  # Should be taken
    addi x1, x0, 5      # This instruction must be flushed
                        # Mode 2 executes this, x1 becomes 5
                        # Mode 3+ flushes this
target:
    addi x2, x0, 10
    
    # Final: x1=0, x2=10