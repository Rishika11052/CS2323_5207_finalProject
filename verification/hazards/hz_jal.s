# Hazard: Control (JAL)
# Fails in: naive_pipeline
# Passes in: stalls_only, forwarding, ...
.text
    jal x1, target      # Jump, x1 = PC+4
    addi x2, x0, 5      # This must be flushed
                        # Mode 2 executes this, x2 becomes 5
                        # Mode 3+ flushes this
    addi x2, x2, 1      # This is never reached
target:
    addi x3, x0, 10
    
    # Final: x1=(addr of addi x2,x0,5), x2=0, x3=10