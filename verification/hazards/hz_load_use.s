# Hazard: Load-Use (MEM to EX)
# Fails in: naive_pipeline
# Passes in: stalls_only, forwarding, ...
.data
my_value: .word 100
.align 3

.text
    la x1, my_value
    lw x2, 0(x1)        # x2 written in WB at T5
    add x3, x2, x0      # x3 reads x2 in ID at T2
                        # Mode 2 fails (reads old x2)
                        # Mode 3 stalls for 1 cycle
                        # Mode 4 stalls for 1 cycle (Load-Use stall)
    
    # Final: x2=100, x3=100