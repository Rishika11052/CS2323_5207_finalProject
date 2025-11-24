# Test: Single Precision Memory
# 1. Setup Base Address (Use 0x100 arbitrarily)
addi x1, x0, 256        # x1 = 256 (0x100)

# 2. Create a float value
addi x2, x0, 10         # x2 = 10
fcvt.s.w f1, x2         # f1 = 10.0

# 3. Store Float to Memory
fsw f1, 0(x1)           # Mem[0x100] = 10.0

# 4. Clear Register to ensure we actually load
fcvt.s.w f1, x0         # f1 = 0.0

# 5. Load Float from Memory
flw f2, 0(x1)           # f2 = Mem[0x100] (Should be 10.0)

# 6. Flush pipeline
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0