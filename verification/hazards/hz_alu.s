# Hazard: ALU-ALU (EX to EX)
# Fails in: naive_pipeline
# Passes in: stalls_only, forwarding, ...
.text
    addi x1, x0, 5      # x1 written in WB at T5
    sub x2, x1, x0      # x2 reads x1 in ID at T2
                        # Mode 2 fails (reads old x1)
                        # Mode 3 stalls for 2 cycles
                        # Mode 4 forwards (EX -> EX)
    
    # Final: x1=5, x2=5