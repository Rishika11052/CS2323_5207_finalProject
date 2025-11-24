# Test: Double Precision Memory
# 1. Setup Base Address (0x200)
addi x1, x0, 512        # x1 = 512 (0x200)

# 2. Create Double value
addi x2, x0, 20         # x2 = 20
fcvt.d.w f1, x2         # f1 = 20.0

# 3. Store Double (8 bytes)
fsd f1, 0(x1)           # Mem[0x200] = 20.0

# 4. Clear Register
fcvt.d.w f1, x0         # f1 = 0.0

# 5. Load Double back
fld f2, 0(x1)           # f2 = 20.0

