# Test: Single Precision Arithmetic (No Memory)
# 1. Load integers into GPR
addi x1, x0, 5          # x1 = 5
addi x2, x0, 2          # x2 = 2

# 2. Convert to Float (GPR -> FPR)
fcvt.s.w f1, x1         # f1 = 5.0
fcvt.s.w f2, x2         # f2 = 2.0

# 3. Arithmetic (FPR -> FPR)
fadd.s f3, f1, f2       # f3 = 5.0 + 2.0 = 7.0
fsub.s f4, f1, f2       # f4 = 5.0 - 2.0 = 3.0

# 4. Convert back to Int to verify (FPR -> GPR)
fcvt.w.s x3, f3         # x3 = (int)7.0 = 7
fcvt.w.s x4, f4         # x4 = (int)3.0 = 3

# 5. Flush pipeline (NOPs)
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0