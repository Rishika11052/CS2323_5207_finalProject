# Test: Double Precision Arithmetic
# 1. Load integers
addi x1, x0, 4          # x1 = 4
addi x2, x0, 3          # x2 = 3

# 2. Convert to Double (GPR -> FPR)
fcvt.d.w f1, x1         # f1 = 4.0 (Double)
fcvt.d.w f2, x2         # f2 = 3.0 (Double)

# 3. Arithmetic
fadd.d f3, f1, f2       # f3 = 7.0
fmul.d f4, f1, f2       # f4 = 12.0

# 4. Convert back to Int
fcvt.w.d x3, f3         # x3 = 7
fcvt.w.d x4, f4         # x4 = 12

