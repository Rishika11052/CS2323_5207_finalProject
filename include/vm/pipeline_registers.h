#ifndef PIPELINE_REGISTERS_H
#define PIPELINE_REGISTERS_H

#include <cstdint>
#include "vm/alu.h"

// --- IF/ID Register ---
// Holds the output of the Fetch stage, needed by Decode.
struct IF_ID_Register {
    uint32_t instruction = 0x00000013; // Default to nop (addi x0, x0, 0)
    uint64_t pc_plus_4 = 0;           // PC + 4 value
    bool valid = false;               // Is the data in this register valid? (For initialization/flushing)

    // Branch / Jump Prediction Signals
    bool predictedTaken = false; // Was the branch/jump predicted taken?

    // Default constructor to initialize
    IF_ID_Register() : instruction(0x00000013), pc_plus_4(0), valid(false) {}
};

// --- ID/EX Register ---
// Holds the output of Decode, needed by Execute.
struct ID_EX_Register {
      
    // Control Signals (Generated in Decode)
    bool RegWrite = false;
    bool MemRead = false;
    bool MemWrite = false;
    bool MemToReg = false; // Decides WB mux source
    bool AluSrc = false;   // Decides ALU operand B source (Reg2 or Imm)
    alu::AluOp AluOperation = alu::AluOp::kNone; // Specific ALU operation

    uint8_t rm = 0;          // Rounding Mode
    bool Rs1IsFPR = false;   // Does Reg1 come from the Float Register File?
    bool Rs2IsFPR = false;   // Does Reg2 come from the Float Register File?
    bool RdIsFPR  = false;   // Does destination register go to the Float Register File?
    bool isDouble = false;
    // Branch Control
    uint64_t currentPC = 0; // Current PC value
    bool isBranch = false; // True for BEQ, BNE, etc.
    bool isJump = false;   // True for JAL, JALR
    bool isJAL  = false;    // True for JAL, False for JALR

    // Data (Read/Generated in Decode)
    uint64_t reg1_value = 0;      // Value from rs1
    uint64_t reg2_value = 0;      // Value from rs2 (used for R-type, B-type, S-type)
    int32_t  immediate = 0;       // Sign-extended immediate value
    uint8_t  rd = 0;              // Destination register index (for R, I, U, J types)
    uint8_t  rs1_idx = 0;         // Source register 1 index (needed for forwarding later)
    uint8_t  rs2_idx = 0;         // Source register 2 index (needed for forwarding later)
    uint8_t  funct3 = 0;          // funct3 field (for branch decisions)

    // Data passed through
    uint64_t pc_plus_4 = 0;       // Passed from IF/ID (for JAL/JALR writeback)

    // Store Control Hazard Signals
    bool isMisPredicted = false;
    uint64_t actualTargetPC = 0;

    bool valid = false;           // Is the data valid?

    // Default constructor
    ID_EX_Register() = default; // Default initializes members to 0/false/kNone
};

// --- EX/MEM Register ---
// Holds the output of Execute, needed by Memory.
struct EX_MEM_Register {

    
    // Control Signals (Passed through from ID/EX)
    bool RegWrite = false;
    bool MemRead = false;
    bool MemWrite = false;
    bool MemToReg = false;

    bool RdIsFPR = false;    // Pass through: Write to F register?
    uint8_t fcsr_flags = 0;

    // Data (Calculated in Execute or passed through)
    uint64_t alu_result = 0;      // Result from ALU (used for address or WB data)
    uint64_t reg2_value = 0;      // Value from rs2 (passed through for Store instructions)
    uint8_t  rd = 0;              // Destination register index (passed through)
    uint8_t  funct3 = 0;          // funct3 field (for determining load/store size)

    // Store Control Hazard Signals
    bool isControlHazard = false;
    uint64_t targetPC = 0;

    bool valid = false;           // Is the data valid?

    // Default constructor
    EX_MEM_Register() = default;
    
};

// --- MEM/WB Register ---
// Holds the output of Memory, needed by WriteBack.
struct MEM_WB_Register {

    bool RegWrite = false;
    bool MemToReg = false;

    bool RdIsFPR = false;    // Pass through: Write to F register?
    uint8_t fcsr_flags = 0;
    // Control Signals (Passed through from EX/MEM)

    // Data (Read in Memory or passed through)
    uint64_t data_from_memory = 0; // Data read by Load instructions
    uint64_t alu_result = 0;       // Result from ALU (passed through)
    uint8_t  rd = 0;               // Destination register index (passed through)

    bool valid = false;            // Is the data valid?

    // Default constructor
    MEM_WB_Register() = default;
};

#endif // PIPELINE_REGISTERS_H