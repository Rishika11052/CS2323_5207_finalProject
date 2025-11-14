# Verification: NOPs for alu
# Fails in: -
# Passes in: all modes
.text
    addi x1, x0, 5      # x1 written in WB at T5
    nop
    nop
    sub x2, x1, x0      # x2 reads x1 in ID at T5
                        # Read is after WB, so it's safe
    
    # Final: x1=5, x2=5