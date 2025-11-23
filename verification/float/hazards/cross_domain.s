# Test: Cross-Domain Forwarding (Int -> Float)
# Config: hazard=true, forwarding=true

.text
# .global main
main:
    # 1. Generate an Integer
    #    This writes to 'x1' in the EX stage.
    addi x1, x0, 10      # x1 = 10

    # 2. Immediately use it in a Float Instruction
    #    fcvt reads 'x1'. It is in Decode while addi is in Execute.
    #    Without forwarding, this would stall.
    #    With forwarding, x1 should be forwarded from EX -> EX.
    fcvt.s.w f1, x1      # f1 = 10.0

    # 3. Use the result (Standard float dependency)
    fadd.s f2, f1, f1    # f2 = 20.0

    # 4. Flush
    addi x0, x0, 0
    addi x0, x0, 0
    addi x0, x0, 0