addi x1, x0, 100      # x1 = 100 (base address for the load)

lw   x5, 0(x1)        # load value from memory[100] into x5 (ready only in WB)
add  x6, x5, x2       # uses x5 immediately

