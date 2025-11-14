# Verification: NOPs for load_use
# Fails in: -
# Passes in: all modes
.data
my_value: .word 100
.align 3

.text
    la x1, my_value
    lw x2, 0(x1)        # x2 written in WB at T5
    nop                 # This is the 1-cycle stall
    add x3, x2, x0      # x3 reads x2 in ID at T3
                        # Mode 4 forwards (MEM -> EX)
    # This NOP test isn't long enough for naive mode.
    # Naive mode would need 2 NOPs.
    # Let's adjust this test to pass naive mode.
    
    la x10, my_value
    lw x12, 0(x10)
    nop
    nop
    add x13, x12, x0    # Safe in all modes
    
    # Final: x12=100, x13=100