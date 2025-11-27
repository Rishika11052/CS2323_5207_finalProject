# Cache Verification Test
# Config: Direct Mapped, 4 Lines, Block Size=4 bytes

.text
    # Setup Base Address (0x10000000)
    lui x1, 0x10000

    # 1. Compulsory Miss (Set 0)
    lw x2, 0(x1)        

    # 2. Hit (Set 0)
    lw x3, 0(x1)        

    # 3. Conflict Miss (Set 0) -> Evicts 0x10000000
    lw x4, 16(x1)       

    # 4. Capacity/Conflict Miss (Set 0) -> Reloads 0x10000000
    lw x5, 0(x1)        

    # 5. Write Miss (Set 1)
    sw x5, 4(x1)