# Cache Verification Test
# Config: Direct Mapped, 4 Lines, Block Size=4 bytes
# Total Cache Size = 16 bytes (Offsets 0, 4, 8, 12 map to Sets 0, 1, 2, 3)

.text
    # Setup Base Address (0x10000000)
    lui x1, 0x10000

    # --- 1. Compulsory Miss ---
    # Access Address 0x10000000 -> Maps to Set 0
    # Cache: [Valid, Tag A] (Set 0)
    lw x2, 0(x1)        # Expected: MISS

    # --- 2. Cache Hit ---
    # Access Address 0x10000000 -> Maps to Set 0
    # Data is already there.
    lw x3, 0(x1)        # Expected: HIT

    # --- 3. Conflict Miss (Eviction) ---
    # Access Address 0x10000010 (Base + 16)
    # 16 bytes / 4 bytes-per-block = Block Index 4
    # 4 % 4 sets = Set 0
    # This maps to Set 0, which is occupied by 0x10000000.
    # It must EVICT 0x10000000.
    lw x4, 16(x1)       # Expected: MISS + EVICTION

    # --- 4. Miss on Previously Cached Data ---
    # Access Address 0x10000000
    # We had this in step 1, but step 3 evicted it.
    lw x5, 0(x1)        # Expected: MISS (Reloads)

    # --- 5. Write Miss (Policy Check) ---
    # Write to 0x10000004 -> Maps to Set 1
    # If Write-Allocate:    MISS -> Allocates Set 1
    # If No-Write-Allocate: MISS -> Writes to Mem only
    sw x5, 4(x1)        # Expected: MISS