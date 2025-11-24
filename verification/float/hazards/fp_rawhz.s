# Test: Read-After-Write (RAW) Hazard - Float
# Config: hazard=true, forwarding=false

.text
# .global main
main:
    # 1. Setup Initial Values
    addi x1, x0, 10
    addi x2, x0, 5
    fcvt.s.w f1, x1      # f1 = 10.0
    fcvt.s.w f2, x2      # f2 = 5.0

    # 2. Trigger Hazard
    #    fadd writes to f3. 
    #    The next instruction needs f3 immediately.
    fadd.s f3, f1, f2    # f3 = 15.0

    # --- STALL SHOULD HAPPEN HERE (2 Cycles) ---
    fsub.s f4, f3, f1    # f4 = 15.0 - 10.0 = 5.0
                         # Needs f3, which is still in the pipeline!

    # 3. Dummy instructions to let pipeline finish
    addi x0, x0, 0
    addi x0, x0, 0
    addi x0, x0, 0
    addi x0, x0, 0

    # End (Loop forever or just stop)
    beq x0, x0, end
end:
    addi x0, x0, 0