/**
 * @file rv5s_vm.cpp
 * @brief RV5S VM (5-Stage Pipeline) implementation
 * @author Aric Maji, https://github.com/Adam-Warlock09
 */

#include "vm/rv5s/rv5s_vm.h"
#include "vm/rv5s/rv5s_control_unit.h"
#include "vm/pipeline_registers.h"
#include "vm/vm_base.h"
#include "config.h"
#include "globals.h"
#include "utils.h"
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <common/instructions.h>


// initializes the 5-stage virtual machine
RV5SVM::RV5SVM() : VmBase() {

    Reset();
    try {
        DumpRegisters(globals::registers_dump_file_path, registers_);
        DumpState(globals::vm_state_dump_file_path);
        DumpPipelineRegisters(globals::pipeline_registers_dump_file_path);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to dump initial state in RV5SVM constructor: " << e.what() << std::endl;
    }
    std::cout << "RV5SVM (5-Stage Pipeline VM) initialized." << std::endl;

}

// ~ means destructor ; is called whenever an object of some class is destroyed; default means call the default version of the destructor
//meaning destroy all member variables in normal way like no added stuff
RV5SVM::~RV5SVM() = default;


// VM is in clean/default state
// It sets all counters(program counters and cycle_s_ to 0)
// creates new empty pipeline registers and clears the undo/redo stack
void RV5SVM::Reset() {

    program_counter_ = 0;
    instructions_retired_ = 0;
    id_stall_ = false;
    stall_cycles_ = 0;
    cycle_s_ = 0;
    registers_.Reset();
    memory_controller_.Reset();
    
    if_id_reg_ = IF_ID_Register();
    id_ex_reg_ = ID_EX_Register();
    ex_mem_reg_ = EX_MEM_Register();
    mem_wb_reg_ = MEM_WB_Register();

    //Reset undo redo stacks
    while (!undo_stack_.empty()) {
        undo_stack_.pop();
    }
    while (!redo_stack_.empty()) {
        redo_stack_.pop();
    }

    // Reset Branch Prediction Table
    branch_history_table_.clear();

    // Reset Forwarding Signals
    forward_a_ = ForwardSource::kNone;
    forward_b_ = ForwardSource::kNone;
    forward_branch_a_ = ForwardSource::kNone;
    forward_branch_b_ = ForwardSource::kNone;

    instruction_sequence_counter_ = 0;
    last_retired_sequence_id_ = 0;

    control_unit_.Reset();

    DumpState(globals::vm_state_dump_file_path);
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpPipelineRegisters(globals::pipeline_registers_dump_file_path);

    std::cout << "RV5SVM has been reset." << std::endl;

}

//Simulates One clock cycle
//calls all the pipeline stages in reverse order
// why?  the next state of each stage is based on the state of the pipeline at the beginning of each clock cycle
// in real hardware , all pipeline registers update at the exact same time(when the signal changes from 0 to 1)
//In C++ only one line runs at a time
//If you run it in the right order , everything will happen one after the other like in a single cycle
// but here since you're  updating the main register in the end everything is updated simultaneously.
void RV5SVM::PipelinedStep() {

    //to snapshot the current stage 
    // this is pushed to the undo/redo stack
    CycleDelta delta;
    delta.old_pc = program_counter_;
    delta.old_if_id_reg = if_id_reg_;
    delta.old_id_ex_reg = id_ex_reg_;
    delta.old_ex_mem_reg = ex_mem_reg_;
    delta.old_mem_wb_reg = mem_wb_reg_;

    delta.old_id_stall = id_stall_;
    delta.old_stall_cycles = stall_cycles_;
    delta.old_forward_a = forward_a_;
    delta.old_forward_b = forward_b_;
    delta.old_forward_branch_a_ = forward_branch_a_;
    delta.old_forward_branch_b_ = forward_branch_b_;
    delta.old_instruction_sequence_counter_ = instruction_sequence_counter_;
    delta.old_last_retired_sequence_id_ = last_retired_sequence_id_;
    
    // flag is used to initialize the CycleDelta Object
    //when mem_wb_reg.valid is true(when WriteBack happens)
    //so that the uno/redo functions know whether they need to increment or decrement instruction_retired_ count
    delta.instruction_retired = false;

    // Run the Write Back Stage
    WbWriteInfo WBInfo = pipelineWriteBack(mem_wb_reg_);
    if (mem_wb_reg_.valid) {
        delta.instruction_retired = true;
        last_retired_sequence_id_ = mem_wb_reg_.sequence_id;
    } else {
        last_retired_sequence_id_ = 0;
    }

    //Run the Memory Stage
    std::pair<MEM_WB_Register, MemWriteInfo> MemInfo = pipelineMemory(ex_mem_reg_);
    MEM_WB_Register next_mem_wb_reg = MemInfo.first;
    //keeps track of data that was overwritten to some memory
    MemWriteInfo mem_info = MemInfo.second;

    // Get Control Hazard Signals from previous EX stage (When Branch Prediction is OFF)
    bool EX_flushSignal = ex_mem_reg_.isControlHazard;
    uint64_t EX_newPCTarget = ex_mem_reg_.targetPC;

    // Get Control Hazard Signals from previous ID stage (When Branch Prediction is ON)
    bool ID_flushSignal = id_ex_reg_.isMisPredicted;
    uint64_t ID_newPCTarget = id_ex_reg_.actualTargetPC;

    bool isHazardDetectionEnabled = vm_config::config.isHazardDetectionEnabled();

    //Run the Execute Stage
    EX_MEM_Register next_ex_mem_reg;

    if(isHazardDetectionEnabled && EX_flushSignal) {
        next_ex_mem_reg = EX_MEM_Register(); // Default to bubble (Flush)
        stall_cycles_++; // Increment stall cycles
        std::cout << "EX Stage Flush due to Control Hazard. Inserting Bubble. Branch pred off" << std::endl;
    } else {
        next_ex_mem_reg = pipelineExecute(id_ex_reg_); // Normal Execute
    }
    
    //Run the Decode Stage
    ID_EX_Register next_id_ex_reg;
    if (isHazardDetectionEnabled && EX_flushSignal) {
        next_id_ex_reg = ID_EX_Register(); // Default to bubble (Flush)
        stall_cycles_++; // Increment stall cycles
        std::cout << "ID Stage Flush due to Control Hazard. Inserting Bubble. Branch pred off" << std::endl;
    }else if (isHazardDetectionEnabled && ID_flushSignal) {
        next_id_ex_reg = ID_EX_Register(); // Default to bubble (Flush)
        stall_cycles_++; // Increment stall cycles
        std::cout << "ID Stage Flush due to Control Hazard. Inserting Bubble. Branch pred on" << std::endl;
    } else {
        next_id_ex_reg = pipelineDecode(if_id_reg_); // Normal Decode
    }

    IF_ID_Register next_if_id_reg;

    // CONTROL LOGIC FOR HAZARD DUE TO BRANCHES AND JUMPS AND STALLS
    // Handles Branch Misprediction or jump because of execute stage
    if (id_stall_) {
        // Data hazard detected: Stall the pipeline by inserting a bubble
        next_if_id_reg = if_id_reg_; // Hold the current IF/ID register (stall)
    } else if (ID_flushSignal) {
        // Branch Misprediction detected in ID stage: ReSteer the pipeline
        program_counter_ = ID_newPCTarget; // Update PC to the correct target
        next_if_id_reg = pipelineFetch(); // Fetch new instruction at updated PC
    } else if (EX_flushSignal) {
        // Branch Misprediction or Jump detected in EX stage: ReSteer the pipeline
        program_counter_ = EX_newPCTarget; // Update PC to the correct target
        next_if_id_reg = pipelineFetch(); // Fetch new instruction at updated PC
    } else {
        // Normal Operation: Fetch the next instruction
        next_if_id_reg = pipelineFetch(); // Fetch next instruction
    }
    
    // Save all the calculated values into the pipeline registers
    delta.wb_write = WBInfo;
    delta.mem_write = mem_info;    
    if_id_reg_ = next_if_id_reg;
    id_ex_reg_ = next_id_ex_reg;
    ex_mem_reg_ = next_ex_mem_reg;
    mem_wb_reg_ = next_mem_wb_reg;

    // After stage for the redo function
    delta.new_pc = program_counter_;
    delta.new_ex_mem_reg = next_ex_mem_reg;
    delta.new_id_ex_reg = next_id_ex_reg;
    delta.new_if_id_reg = next_if_id_reg;
    delta.new_mem_wb_reg = next_mem_wb_reg;

    delta.new_id_stall = id_stall_;
    delta.new_stall_cycles = stall_cycles_;
    delta.new_forward_a = forward_a_;
    delta.new_forward_b = forward_b_;
    delta.new_forward_branch_a_ = forward_branch_a_;
    delta.new_forward_branch_b_ = forward_branch_b_;
    delta.new_instruction_sequence_counter_ = instruction_sequence_counter_;
    delta.new_last_retired_sequence_id_ = last_retired_sequence_id_;

    if (delta.instruction_retired) {
        instructions_retired_++;
    }
    // Push the new cycle's delta onto the undo stack.
    undo_stack_.push(delta);
    // By executing a new step (PipelinedStep), we are creating a new,
    // divergent state history. The old "future" (the redo stack) is
    // now invalid and must be cleared to maintain a single, coherent timeline.
    while (!redo_stack_.empty()) {
        redo_stack_.pop();
    }

    cycle_s_++;

}

IF_ID_Register RV5SVM::pipelineFetch() {
    
    IF_ID_Register result;
    
    if (program_counter_ >= program_size_) {
        result.instruction = 0x00000013; // NOP
        result.pc_plus_4 = program_counter_;
        result.valid = false;

        return result;
    }

    result.instruction = 0x00000013; // Default to NOP

    try {
        result.instruction = memory_controller_.ReadWord(program_counter_);
        result.pc_plus_4 = program_counter_ + 4;
    } catch (const std::out_of_range& e) {
        std::cerr << "Error during instruction fetch at PC = 0x" << std::hex << program_counter_ << " - " << e.what() << std::dec << std::endl;
        result.instruction = 0x00000013; // NOP
        result.pc_plus_4 = program_counter_;
        result.valid = false;

        return result;
    }

    result.pc_plus_4 = program_counter_ + 4;
    result.valid = true;

    // Branch Prediction Logic
    bool predictedTaken = false;
    uint64_t predictedTarget = 0;

    // Get the branch prediction type from config
    vm_config::BranchPredictionType bp_type = vm_config::config.getBranchPredictionType();

    if (bp_type != vm_config::BranchPredictionType::NONE) {

        // Decode opcode to determine if it's a branch or jump
        uint8_t opcode = result.instruction & 0b1111111;
        bool isJump = (opcode == 0b1101111) || (opcode == 0b1100111);
        bool isBranch = (opcode == 0b1100011);
        bool isJAL = (opcode == 0b1101111);

        if (isJump) {

            // For JAL, we can predict the target; for JALR, we cannot
            if (isJAL) {
                predictedTaken = true;
                int32_t imm = ImmGenerator(result.instruction);
                predictedTarget = program_counter_ + imm;
            } else {
                predictedTaken = false; // For JALR, we cannot predict the target
            }

        } else if (isBranch) {

            if (bp_type == vm_config::BranchPredictionType::STATIC) {

                // Static Prediction: Backward branches taken, forward branches not taken
                int32_t imm = ImmGenerator(result.instruction);
                if (static_cast<int64_t>(imm) < 0) {
                    predictedTaken = true;
                    predictedTarget = program_counter_ + imm;
                } else {
                    predictedTaken = false;
                }

            } else if (bp_type == vm_config::BranchPredictionType::DYNAMIC1BIT) {

                if (branch_history_table_.count(program_counter_) && branch_history_table_[program_counter_] == true) {
                    // Predict taken
                    predictedTaken = true;
                    int32_t imm = ImmGenerator(result.instruction);
                    predictedTarget = program_counter_ + static_cast<int64_t>(imm);
                } else {
                    // Follow Static Prediction: Backward branches taken, forward branches not taken
                    int32_t imm = ImmGenerator(result.instruction);
                    if (static_cast<int64_t>(imm) < 0) {
                        predictedTaken = true;
                        predictedTarget = program_counter_ + static_cast<int64_t>(imm);
                    } else {
                        predictedTaken = false;
                    }
                }

            }

        }

    }

    // Update PC based on prediction
    if (predictedTaken) {
        program_counter_ = predictedTarget;
    } else {
        program_counter_ = result.pc_plus_4; // Increment PC normally
    }

    // Store prediction info in IF/ID register for use in Decode stage
    result.predictedTaken = predictedTaken;

    result.sequence_id = ++instruction_sequence_counter_;

    return result;

}

ID_EX_Register RV5SVM::pipelineDecode(const IF_ID_Register& if_id_reg) {
    ID_EX_Register result;

    if (!if_id_reg.valid) {
        id_stall_ = false;
        result.valid = false;
        return result;
    }

    uint32_t instruction = if_id_reg.instruction;
    uint8_t opcode = instruction & 0b1111111;
    uint8_t rd = (instruction >> 7) & 0b11111;
    uint8_t funct3 = (instruction >> 12) & 0b111;
    uint8_t rs1 = (instruction >> 15) & 0b11111;
    uint8_t rs2 = (instruction >> 20) & 0b11111;

    // --- 1. Identify Operand Types [NEW] ---
    bool Rs1IsFPR = control_unit_.IsRs1FPR(instruction);
    bool Rs2IsFPR = control_unit_.IsRs2FPR(instruction);
    bool RdIsFPR  = control_unit_.IsRdFPR(instruction);
    bool isDouble = instruction_set::isDInstruction(instruction);

    bool usesRS1 = (opcode != 0b0110111) && (opcode != 0b0010111) && (opcode != 0b1101111); 
    bool usesRS2 = (opcode == 0b0100011) || (opcode == 0b0110011) || (opcode == 0b1100011) 
                   || (opcode == 0b0100111) || (opcode == 0b1010011); // Added Store-FP & Op-FP

    // --- 2. Hazard Detection ---
    if(vm_config::config.isHazardDetectionEnabled() && (vm_config::config.branch_prediction_type == vm_config::BranchPredictionType::NONE || (opcode != 0b1101111 && opcode != 0b1100111 && opcode != 0b1100011))) {
        
        // Generic Hazard Check (Works for both Int and Float because we check Register Index AND Type)
        // Check ID/EX (1 cycle ahead)
        if (id_ex_reg_.valid && id_ex_reg_.rd != 0) {
            bool hazard1 = usesRS1 && (id_ex_reg_.rd == rs1) && (id_ex_reg_.RdIsFPR == Rs1IsFPR); // Match Index AND Type
            bool hazard2 = usesRS2 && (id_ex_reg_.rd == rs2) && (id_ex_reg_.RdIsFPR == Rs2IsFPR);
            
            if (id_ex_reg_.MemRead && (hazard1 || hazard2)) {
                id_stall_ = true;
                stall_cycles_++;
                return ID_EX_Register(); // Stall for Load-Use
            }

            if (!vm_config::config.isForwardingEnabled()) {
                if (id_ex_reg_.RegWrite && (hazard1 || hazard2)) {
                    id_stall_ = true;
                    stall_cycles_++;
                    return ID_EX_Register(); // Stall for RAW
                }
            }
        }
        
        // Check EX/MEM (2 cycles ahead) - Only needed if forwarding is disabled
        if (!vm_config::config.isForwardingEnabled() && ex_mem_reg_.valid && ex_mem_reg_.RegWrite && ex_mem_reg_.rd != 0) {
            bool hazard1 = usesRS1 && (ex_mem_reg_.rd == rs1) && (ex_mem_reg_.RdIsFPR == Rs1IsFPR);
            bool hazard2 = usesRS2 && (ex_mem_reg_.rd == rs2) && (ex_mem_reg_.RdIsFPR == Rs2IsFPR);
            if (hazard1 || hazard2) {
                id_stall_ = true;
                stall_cycles_++;
                return ID_EX_Register();
            }
        }
    }

    id_stall_ = false;
    forward_a_ = ForwardSource::kNone;
    forward_b_ = ForwardSource::kNone;

    // --- 3. Forwarding Logic [UPDATED] ---
    if(vm_config::config.isForwardingEnabled()) {
        // EX/MEM Hazard (2 cycles ago)
        if (ex_mem_reg_.valid && ex_mem_reg_.RegWrite && ex_mem_reg_.rd != 0) {
            // Forward only if types match (Int->Int or Float->Float)
            if (usesRS1 && ex_mem_reg_.rd == rs1 && (ex_mem_reg_.RdIsFPR == Rs1IsFPR)) {
                forward_a_ = ForwardSource::kFromMemWb;
            }
            if (usesRS2 && ex_mem_reg_.rd == rs2 && (ex_mem_reg_.RdIsFPR == Rs2IsFPR)) {
                forward_b_ = ForwardSource::kFromMemWb;
            }
        }
        // ID/EX Hazard (1 cycle ago) - Higher Priority
        if (id_ex_reg_.valid && id_ex_reg_.RegWrite && id_ex_reg_.rd != 0 && !id_ex_reg_.MemRead) {
            if (usesRS1 && id_ex_reg_.rd == rs1 && (id_ex_reg_.RdIsFPR == Rs1IsFPR)) {
                forward_a_ = ForwardSource::kFromExMem;
            }
            if (usesRS2 && id_ex_reg_.rd == rs2 && (id_ex_reg_.RdIsFPR == Rs2IsFPR)) {
                forward_b_ = ForwardSource::kFromExMem;
            }
        }
    }

    // --- 4. Read Registers (GPR vs FPR) [NEW] ---
    result.immediate = ImmGenerator(instruction);
    try {
        if (Rs1IsFPR) result.reg1_value = registers_.ReadFpr(rs1);
        else          result.reg1_value = registers_.ReadGpr(rs1);

        if (Rs2IsFPR) result.reg2_value = registers_.ReadFpr(rs2);
        else          result.reg2_value = registers_.ReadGpr(rs2);
    } catch (const std::exception& e) {
        result.valid = false;
        return result;
    }

    // --- 5. Populate Result ---
    control_unit_.GenerateSignalForInstruction(instruction);
    
    result.RegWrite = control_unit_.GetRegWrite();
    result.MemRead = control_unit_.GetMemRead();
    result.MemWrite = control_unit_.GetMemWrite();
    result.MemToReg = control_unit_.GetMemToReg();
    result.AluSrc = control_unit_.GetAluSrc();
    result.AluOperation = control_unit_.GetAluOperation(instruction);
    
    // Control Flow (Existing logic)
    result.currentPC = if_id_reg.pc_plus_4 - 4;
    result.isBranch = control_unit_.GetBranch(); 
    result.isJAL = (opcode == 0b1101111);
    result.isJump = (result.isJAL || opcode == 0b1100111);

    result.rd = rd;
    result.rs1_idx = rs1;
    result.rs2_idx = rs2;
    result.pc_plus_4 = if_id_reg.pc_plus_4;
    result.funct3 = funct3;

    result.instruction = if_id_reg.instruction;
    result.sequence_id = if_id_reg.sequence_id;

    forward_branch_a_ = ForwardSource::kNone;
    forward_branch_b_ = ForwardSource::kNone;

    // Branch Prediction Handling
    if (vm_config::config.getBranchPredictionType() != vm_config::BranchPredictionType::NONE) {
        
        uint64_t reg1_value = result.reg1_value;
        uint64_t reg2_value = result.reg2_value;

        if (result.isBranch || result.isJump) {

            if (vm_config::config.isHazardDetectionEnabled()) {

                bool hazardFromEX = false;
                // Check for hazards from EX stage
                if (id_ex_reg_.valid && id_ex_reg_.RegWrite && id_ex_reg_.rd != 0) {
                    if (usesRS1 && id_ex_reg_.rd == rs1 && (id_ex_reg_.RdIsFPR == Rs1IsFPR)) hazardFromEX = true;
                    if (usesRS2 && id_ex_reg_.rd == rs2 && (id_ex_reg_.RdIsFPR == Rs2IsFPR)) hazardFromEX = true;
                }

                if (hazardFromEX) {
                    // Stall due to hazard since branch target can't be determined yet
                    id_stall_ = true;
                    stall_cycles_++;
                    std::cout << "Branch Hazard Detected from EX Stage: Stalling pipeline for branch resolution." << std::endl;
                    return ID_EX_Register(); // Return bubble
                }

                bool loadHazardFromMem = false;
                if (ex_mem_reg_.valid && ex_mem_reg_.MemRead && ex_mem_reg_.RegWrite && ex_mem_reg_.rd != 0) {
                    if (usesRS1 && ex_mem_reg_.rd == rs1 && (ex_mem_reg_.RdIsFPR == Rs1IsFPR)) loadHazardFromMem = true;
                    if (usesRS2 && ex_mem_reg_.rd == rs2 && (ex_mem_reg_.RdIsFPR == Rs2IsFPR)) loadHazardFromMem = true;
                }

                if (loadHazardFromMem) {
                    // Stall due to load-use hazard since branch target can't be determined yet
                    id_stall_ = true;
                    stall_cycles_++;
                    std::cout << "Load-Use Hazard Detected from MEM Stage (Load): Stalling pipeline for branch resolution." << std::endl;
                    return ID_EX_Register(); // Return bubble
                }

                if (!vm_config::config.isForwardingEnabled()) {

                    bool ALUHazardFromMem = false;
                    if (ex_mem_reg_.valid && ex_mem_reg_.RegWrite && !ex_mem_reg_.MemRead && ex_mem_reg_.rd != 0) {
                        if (usesRS1 && ex_mem_reg_.rd == rs1 && (ex_mem_reg_.RdIsFPR == Rs1IsFPR)) ALUHazardFromMem = true;
                        if (usesRS2 && ex_mem_reg_.rd == rs2 && (ex_mem_reg_.RdIsFPR == Rs2IsFPR)) ALUHazardFromMem = true;
                    }

                    if (ALUHazardFromMem) {
                        // Stall due to hazard since branch target can't be determined yet
                        id_stall_ = true;
                        stall_cycles_++;
                        std::cout << "ALU (Forwarding Disabled) Branch Hazard Detected from MEM Stage: Stalling pipeline for branch resolution." << std::endl;
                        return ID_EX_Register(); // Return bubble
                    }

                }

            }

            if (vm_config::config.isForwardingEnabled()) {

                // Forwarded values for branch resolution
                if (ex_mem_reg_.valid && ex_mem_reg_.RegWrite && !ex_mem_reg_.MemRead && ex_mem_reg_.rd != 0) {
                    if (ex_mem_reg_.rd == rs1 && usesRS1 && (ex_mem_reg_.RdIsFPR == Rs1IsFPR)) {
                        reg1_value = ex_mem_reg_.alu_result;
                        forward_branch_a_ = ForwardSource::kFromExMem;
                        // std::cout << "Forwarding for Branch Resolution: RS1 from EX/MEM Stage." << std::endl;
                    }
                    if (ex_mem_reg_.rd == rs2 && usesRS2 && (ex_mem_reg_.RdIsFPR == Rs2IsFPR)) {
                        reg2_value = ex_mem_reg_.alu_result;
                        forward_branch_b_ = ForwardSource::kFromExMem;
                        // std::cout << "Forwarding for Branch Resolution: RS2 from EX/MEM Stage." << std::endl;
                    }
                }

            }

        }

        bool actualTaken = false;
        uint64_t actualTargetPC = 0;

        if (result.isJump) {

            actualTaken = true;
            if (result.isJAL) {
                // JAL Target
                actualTargetPC = result.currentPC + static_cast<int64_t>(result.immediate);
            } else {
                // JALR Target
                actualTargetPC = (reg1_value + static_cast<int64_t>(result.immediate)) & ~1ULL;
            }

        } else if (result.isBranch) {

            uint64_t aluResult = 0;
            bool overflow = false;
            
            if (result.AluOperation != alu::AluOp::kNone) {
                try {
                    std::tie(aluResult, overflow) = alu_.execute(result.AluOperation, reg1_value, reg2_value);
                } catch (const std::exception& e) {
                    std::cerr << "Runtime Error: ALU execution failed during branch resolution for instruction 0x" << std::hex << instruction << " - " << e.what() << std::dec << std::endl;
                    result.valid = false;
                    result.RegWrite = false;
                    result.MemRead = false;
                    result.MemWrite = false;
                    result.MemToReg = false;
                    return result;
                }
            }

            switch (result.funct3) {
                case 0b000: // BEQ
                    actualTaken = (aluResult == 0);
                    break;
                case 0b001: // BNE
                    actualTaken = (aluResult != 0);
                    break;
                case 0b100: // BLT
                    actualTaken = (aluResult == 1);
                    break;
                case 0b101: // BGE
                    actualTaken = (aluResult == 0);
                    break;
                case 0b110: // BLTU
                    actualTaken = (aluResult == 1);
                    break;
                case 0b111: // BGEU
                    actualTaken = (aluResult == 0);
                    break;
                default:
                    throw std::runtime_error("Invalid funct3 for branch instruction");
            }

            if (actualTaken) {
                actualTargetPC = result.currentPC + static_cast<int64_t>(result.immediate);
            } else {
                actualTargetPC = result.currentPC + 4;
            }

        }

        bool predictedTaken = if_id_reg.predictedTaken;

        if (actualTaken != predictedTaken) {
            // Misprediction
            result.isMisPredicted = true;
            result.actualTargetPC = actualTargetPC;
        }

        if (vm_config::config.getBranchPredictionType() == vm_config::BranchPredictionType::DYNAMIC1BIT) {
            // Update the Branch History Table
            if (result.isBranch) {
                branch_history_table_[result.currentPC] = actualTaken;
            }
        }

    }


    // Pass Float Signals
    result.rm = funct3; // Rounding mode usually in funct3
    result.Rs1IsFPR = Rs1IsFPR;
    result.Rs2IsFPR = Rs2IsFPR;
    result.RdIsFPR  = RdIsFPR;
    result.isDouble = isDouble;

    result.valid = true;

    return result;
}

EX_MEM_Register RV5SVM::pipelineExecute(const ID_EX_Register& id_ex_reg) {

    EX_MEM_Register result;

    result.valid = id_ex_reg.valid;
    result.RegWrite = id_ex_reg.RegWrite;
    result.MemRead = id_ex_reg.MemRead;
    result.MemWrite = id_ex_reg.MemWrite;
    result.MemToReg = id_ex_reg.MemToReg;
    result.rd = id_ex_reg.rd;

    result.currentPC = id_ex_reg.currentPC;
    result.instruction = id_ex_reg.instruction;
    result.sequence_id = id_ex_reg.sequence_id;

    result.RdIsFPR = id_ex_reg.RdIsFPR;
    result.fcsr_flags = 0;

    // Set Default Control Hazard Signals
    result.isControlHazard = false;
    result.targetPC = 0;

    if (!id_ex_reg.valid) {
        return result; // Pass the Bubble
    }

    // forwarding mux for operand a
    uint64_t operand_a = 0;
    switch(forward_a_){
        case ForwardSource::kNone : operand_a = id_ex_reg.reg1_value;
            break;
        case ForwardSource::kFromExMem : 
            operand_a = ex_mem_reg_.alu_result;
            // std::cout << "Forwarding operand A from EX/MEM Stage: Value = 0x" << std::hex << operand_a << std::dec << std::endl;
            break;
        case ForwardSource::kFromMemWb :
            if (mem_wb_reg_.MemToReg){
                operand_a = mem_wb_reg_.data_from_memory;
                // std::cout << "Forwarding operand A from MEM/WB Stage (Memory): Value = 0x" << std::hex << operand_a << std::dec << std::endl;
            } else {
                operand_a = mem_wb_reg_.alu_result;
                // std::cout << "Forwarding operand A from MEM/WB Stage (ALU): Value = 0x" << std::hex << operand_a << std::dec << std::endl;
            }
            num_forwards_ ++ ;
            break;
    }

    //forwarding mux for operand b
    uint64_t operand_b = 0;

    switch(forward_b_) {
        case ForwardSource::kNone : result.reg2_value = id_ex_reg.reg2_value;
            break;
        case ForwardSource::kFromExMem: 
            result.reg2_value = ex_mem_reg_.alu_result;
            // std::cout << "Forwarding operand B from EX/MEM Stage: Value = 0x" << std::hex << result.reg2_value << std::dec << std::endl;
            break;
        case ForwardSource::kFromMemWb: 
            if(mem_wb_reg_.MemToReg){
                // std::cout << "Forwarding operand B from MEM/WB Stage (Memory): Value = 0x" << std::hex << mem_wb_reg_.data_from_memory << std::dec << std::endl;
                result.reg2_value = mem_wb_reg_.data_from_memory;
            }else{
                // std::cout << "Forwarding operand B from MEM/WB Stage (ALU): Value = 0x" << std::hex << mem_wb_reg_.alu_result << std::dec << std::endl;
                result.reg2_value = mem_wb_reg_.alu_result;
            }
            break;
    }

    if (id_ex_reg.AluSrc) {
        operand_b = static_cast<uint64_t>(static_cast<int64_t>(id_ex_reg.immediate));
        // std::cout << "Using immediate for operand B: Value = 0x" << std::hex << operand_b << std::dec << std::endl;
    } else {
        operand_b = result.reg2_value;
        // std::cout << "Using register value for operand B: Value = 0x" << std::hex << operand_b << std::dec << std::endl;
    }

    bool isFloatOp = (id_ex_reg.Rs1IsFPR || id_ex_reg.Rs2IsFPR || id_ex_reg.RdIsFPR) && !id_ex_reg.MemRead && !id_ex_reg.MemWrite; 

    uint64_t ALUResult = 0;

    try{
        if(isFloatOp){
            uint8_t flags = 0;

            if(id_ex_reg.isDouble){
                std::tie(ALUResult, flags) = alu_.dfpexecute(id_ex_reg.AluOperation, operand_a, operand_b, 0, id_ex_reg.rm);
            } else {
                std::tie(ALUResult, flags) = alu_.fpexecute(id_ex_reg.AluOperation, operand_a, operand_b, 0, id_ex_reg.rm);
            }
            result.fcsr_flags = flags;
        }else{
            bool overflow = false;
            std::tie(ALUResult, overflow) = alu_.execute(id_ex_reg.AluOperation, operand_a, operand_b);

            // JAL and JALR Case
            if (id_ex_reg.isJump) {
                ALUResult = id_ex_reg.currentPC + 4; // Return Address
            }
        
            // LUI Case
            if (id_ex_reg.AluOperation == alu::AluOp::kLUI) {
                ALUResult = static_cast<uint64_t>(id_ex_reg.immediate << 12);
            }
        
            // AUIPC Case
            if (id_ex_reg.AluOperation == alu::AluOp::kAUIPC) {
                ALUResult = id_ex_reg.currentPC + static_cast<int64_t>(id_ex_reg.immediate);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Runtime Error: ALU execution failed for instruction with ALU operation " << static_cast<int>(id_ex_reg.AluOperation) << " - " << e.what() << std::endl;
        result.valid = false;
        result.RegWrite = false;
        result.MemRead = false;
        result.MemWrite = false;
        result.MemToReg = false;
        return result;
    }

    // Branch Handling

    if (vm_config::config.getBranchPredictionType() == vm_config::BranchPredictionType::NONE) {

        if (id_ex_reg.isJump) {

            // Jumps ( JAL and JALR ) Always taken
            result.isControlHazard = true;

            if (id_ex_reg.isJAL) {
                // JAL
                result.targetPC = id_ex_reg.currentPC + static_cast<int64_t>(id_ex_reg.immediate);
            } else {
                // JALR
                result.targetPC = (operand_a + static_cast<int64_t>(id_ex_reg.immediate)) & ~1ULL; // Ensure LSB is 0
            }

        } else if (id_ex_reg.isBranch) {

            bool branchTaken = false; // Determine if branch is taken based on funct3

            switch (id_ex_reg.funct3) {
                case 0b000: // BEQ
                    branchTaken = (ALUResult == 0);
                    break;
                case 0b001: // BNE
                    branchTaken = (ALUResult != 0);
                    break;
                case 0b100: // BLT
                    branchTaken = (ALUResult == 1);
                    break;
                case 0b101: // BGE
                    branchTaken = (ALUResult == 0);
                    break;
                case 0b110: // BLTU
                    branchTaken = (ALUResult == 1);
                    break;
                case 0b111: // BGEU
                    branchTaken = (ALUResult == 0);
                    break;
                default:
                    throw std::runtime_error("Invalid funct3 for branch instruction");
            }

            if (branchTaken) {

                result.isControlHazard = true;
                result.targetPC = id_ex_reg.currentPC + static_cast<int64_t>(id_ex_reg.immediate);

            }

        }

    }

    result.alu_result = ALUResult;
    result.funct3 = id_ex_reg.funct3;

    return result;

}

std::pair<MEM_WB_Register, MemWriteInfo> RV5SVM::pipelineMemory(const EX_MEM_Register& ex_mem_reg) {
    
    MEM_WB_Register result;
    MemWriteInfo writeInfo;
    writeInfo.occurred = false;

    // Pass through control signals
    result.valid = ex_mem_reg.valid;
    result.RegWrite = ex_mem_reg.RegWrite;
    result.MemToReg = ex_mem_reg.MemToReg;
    result.rd = ex_mem_reg.rd;
    result.alu_result = ex_mem_reg.alu_result;

    result.currentPC = ex_mem_reg.currentPC;
    result.instruction = ex_mem_reg.instruction;
    result.sequence_id = ex_mem_reg.sequence_id;
    
    // NEW: Pass through Float Signals
    result.RdIsFPR = ex_mem_reg.RdIsFPR; 
    result.fcsr_flags = ex_mem_reg.fcsr_flags;

    if (!ex_mem_reg.valid) return {result, writeInfo};

    uint64_t memoryAddress = ex_mem_reg.alu_result;

    // --- READ LOGIC ---
    if (ex_mem_reg.MemRead) {
        try {
            // Check for Float Loads first (based on RdIsFPR, as destination is Float)
            if (ex_mem_reg.RdIsFPR) {
                // Determine width based on funct3 or instruction type
                // Usually FLW=0x2 (Word), FLD=0x3 (Double) in funct3
                switch (ex_mem_reg.funct3) {
                    case 0b010: // FLW
                        result.data_from_memory = static_cast<uint64_t>(memory_controller_.ReadWord(memoryAddress));
                        break;
                    case 0b011: // FLD
                        result.data_from_memory = memory_controller_.ReadDoubleWord(memoryAddress);
                        break;
                    default:
                        // Fallback for safety
                        result.data_from_memory = memory_controller_.ReadDoubleWord(memoryAddress); 
                        break;
                }
            } else {
                // Existing Integer Loads (LB, LH, LW, etc.)
                switch (ex_mem_reg.funct3) {
                    case 0b000: // LB
                        result.data_from_memory = static_cast<int64_t>(static_cast<int8_t>(memory_controller_.ReadByte(memoryAddress)));
                        break;
                    case 0b001: // LH
                        result.data_from_memory = static_cast<int64_t>(static_cast<int16_t>(memory_controller_.ReadHalfWord(memoryAddress)));
                        break;
                    case 0b010: // LW
                        result.data_from_memory = static_cast<int64_t>(static_cast<int32_t>(memory_controller_.ReadWord(memoryAddress)));
                        break;
                    case 0b011: // LD
                        result.data_from_memory = memory_controller_.ReadDoubleWord(memoryAddress);
                        break;
                    case 0b100: // LBU
                        result.data_from_memory = static_cast<uint64_t>(memory_controller_.ReadByte(memoryAddress));
                        break;
                    case 0b101: // LHU
                        result.data_from_memory = static_cast<uint64_t>(memory_controller_.ReadHalfWord(memoryAddress));
                        break;
                    case 0b110: // LWU
                        result.data_from_memory = static_cast<uint64_t>(memory_controller_.ReadWord(memoryAddress));
                        break;
                    default:
                        throw std::runtime_error("Invalid funct3 for load instruction");
                }
            }
        } catch (const std::out_of_range& e) {
            std::cerr << "Runtime Error: Memory read failed at address 0x" << std::hex << memoryAddress << " - " << e.what() << std::dec << std::endl;
            result.valid = false;
            return {result, writeInfo};
        }
    }

    // --- WRITE LOGIC ---
    if (ex_mem_reg.MemWrite) {
        writeInfo.occurred = true;
        writeInfo.address = memoryAddress;
        size_t writeSize = 0;

        // Determine size. For Floats, FSW is usually 0b010 (Word), FSD is 0b011 (Double)
        // This overlaps with SW and SD, which is fine because the size is the same.
        switch (ex_mem_reg.funct3) {
            case 0b000: writeSize = 1; break; // SB
            case 0b001: writeSize = 2; break; // SH
            case 0b010: writeSize = 4; break; // SW / FSW
            case 0b011: writeSize = 8; break; // SD / FSD
            default: throw std::runtime_error("Invalid funct3 for store instruction");
        }

        try {
            // Save old bytes for UNDO
            writeInfo.old_bytes.resize(writeSize);
            for (size_t i = 0; i < writeSize; ++i) {
                writeInfo.old_bytes[i] = memory_controller_.ReadByte(memoryAddress + i);
            }

            // Perform Write (Data comes from reg2_value, which holds float bits if it was a float store)
            switch (ex_mem_reg.funct3) {
                case 0b000: // SB
                    memory_controller_.WriteByte(memoryAddress, static_cast<uint8_t>(ex_mem_reg.reg2_value & 0xFF));
                    break;
                case 0b001: // SH
                    memory_controller_.WriteHalfWord(memoryAddress, static_cast<uint16_t>(ex_mem_reg.reg2_value & 0xFFFF));
                    break;
                case 0b010: // SW / FSW
                    memory_controller_.WriteWord(memoryAddress, static_cast<uint32_t>(ex_mem_reg.reg2_value & 0xFFFFFFFF));
                    break;
                case 0b011: // SD / FSD
                    memory_controller_.WriteDoubleWord(memoryAddress, ex_mem_reg.reg2_value);
                    break;
            }

            // Save new bytes for REDO
            writeInfo.new_bytes.resize(writeSize);
            for (size_t i = 0; i < writeSize; ++i) {
                writeInfo.new_bytes[i] = memory_controller_.ReadByte(memoryAddress + i);
            }

        } catch (const std::out_of_range& e) {
            result.valid = false;
            writeInfo.occurred = false;
            return {result, writeInfo};
        }
    }

    return {result, writeInfo};
}

WbWriteInfo RV5SVM::pipelineWriteBack(const MEM_WB_Register& mem_wb_reg) {
    
    WbWriteInfo WBInfo;
    WBInfo.occurred = false;

    // 1. SAFETY: Don't process bubbles!
    if (!mem_wb_reg.valid) {
        return WBInfo;
    }

    if(mem_wb_reg.fcsr_flags != 0){
        uint64_t current_fcsr = registers_.ReadCsr(0x003);
        registers_.WriteCsr(0x003, current_fcsr | mem_wb_reg.fcsr_flags);
    }

    if(mem_wb_reg.RegWrite){
        uint64_t writeValue = mem_wb_reg.MemToReg ? mem_wb_reg.data_from_memory : mem_wb_reg.alu_result;
        uint8_t dest = mem_wb_reg.rd;

        if(dest != 0 || mem_wb_reg.RdIsFPR){
            
            // 2. UNDO SUPPORT: Save index
            WBInfo.reg_index = dest;
            WBInfo.new_value = writeValue;

            if(mem_wb_reg.RdIsFPR){
                // 3. UNDO SUPPORT: Save old value
                try { WBInfo.old_value = registers_.ReadFpr(dest); } catch (...) { WBInfo.old_value = 0; }
                
                registers_.WriteFpr(dest, writeValue);
                WBInfo.reg_type = 2;
            }else{
                // 3. UNDO SUPPORT: Save old value
                try { WBInfo.old_value = registers_.ReadGpr(dest); } catch (...) { WBInfo.old_value = 0; }
                
                registers_.WriteGpr(dest, writeValue);
                WBInfo.reg_type = 0;
            }
            WBInfo.occurred = true;
        }
    }  

    return WBInfo;
}

void RV5SVM::Run() {

    ClearStop();
    
    while(!stop_requested_ && (program_counter_ < program_size_ || if_id_reg_.valid || id_ex_reg_.valid || ex_mem_reg_.valid || mem_wb_reg_.valid)) {
        PipelinedStep();
        std::cout << "Program Counter: " << program_counter_ << std::endl;
    }

    if (program_counter_ >= program_size_) {
        std::cout << "VM_PROGRAM_END" << std::endl;
        output_status_ = "VM_PROGRAM_END";
    }

    std::cout << "--- Simulation Stats ---" << std::endl;
    std::cout << "Total Cycles: " << cycle_s_ << std::endl;
    std::cout << "Instructions Retired: " << instructions_retired_ << std::endl;
    std::cout << "Stall Cycles: " << stall_cycles_ << std::endl; // You already have this!
    std::cout << "Forwarding Events: " << num_forwards_ << std::endl;
    std::cout << "Pipeline Flushes: " << num_flushes_ << std::endl;

    DumpState(globals::vm_state_dump_file_path);
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpPipelineRegisters(globals::pipeline_registers_dump_file_path);

}

void RV5SVM::Step() {
 
    if (program_counter_ >= program_size_ && !if_id_reg_.valid && !id_ex_reg_.valid && !ex_mem_reg_.valid && !mem_wb_reg_.valid){
        std::cout << "VM_PROGRAM_END" << std::endl;
        output_status_ = "VM_PROGRAM_END";
        return;
    }

    PipelinedStep();

    if (program_counter_ < program_size_ || if_id_reg_.valid || id_ex_reg_.valid || ex_mem_reg_.valid || mem_wb_reg_.valid) {
        std::cout << "VM_STEP_COMPLETED" << std::endl;
        output_status_ = "VM_STEP_COMPLETED";
    } else {
        std::cout << "VM_PROGRAM_END" << std::endl;
        output_status_ = "VM_PROGRAM_END";
    }

    DumpState(globals::vm_state_dump_file_path);
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpPipelineRegisters(globals::pipeline_registers_dump_file_path);
    
}

void RV5SVM::DebugRun() {

    ClearStop();
    output_status_ = "VM_DEBUG_RUN_STARTED";
    
    // Main debug run loop
    // This condition is the same as Run(), it stops when the pipeline is empty.
    while(!stop_requested_ && (program_counter_ < program_size_ || if_id_reg_.valid || id_ex_reg_.valid || ex_mem_reg_.valid || mem_wb_reg_.valid)) {
        
        // PipelinedStep() automatically saves the undo/redo history
        PipelinedStep(); 

        // --- Breakpoint Check ---
        // This is the only part that's different from Run()
        // We assume 'breakpoints_' is a std::set<uint64_t> inherited from VmBase
        if (program_counter_ < program_size_) {
            if (std::find(breakpoints_.begin(), breakpoints_.end(), program_counter_) != breakpoints_.end()) {
                std::cout << "VM_BREAKPOINT_HIT: 0x" << std::hex << program_counter_ << std::dec << std::endl;
                output_status_ = "VM_BREAKPOINT_HIT";
                RequestStop(); // Stop the run loop
            }
        }
        // --- End of Breakpoint Check ---
    }

    if (program_counter_ >= program_size_ && !if_id_reg_.valid && !id_ex_reg_.valid && !ex_mem_reg_.valid && !mem_wb_reg_.valid) {
        std::cout << "VM_PROGRAM_END" << std::endl;
        output_status_ = "VM_PROGRAM_END";
    }

    DumpState(globals::vm_state_dump_file_path);
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpPipelineRegisters(globals::pipeline_registers_dump_file_path);

}

void RV5SVM::Undo() {
    
    // If stack is empty then nothing is left to undo
    if (undo_stack_.empty()) {
        std::cout << "VM_NO_MORE_UNDO" << std::endl;
        output_status_ = "VM_NO_MORE_UNDO";
        return;
    }
    
    // snapshot of everything in the last cycle then pops it from the stack
    CycleDelta last = undo_stack_.top();
    undo_stack_.pop();

    //restores pc to old pc
    program_counter_ = last.old_pc;

    //restores the state of all pipeline registers to the state before the last cycle started
    if_id_reg_  = last.old_if_id_reg;
    id_ex_reg_  = last.old_id_ex_reg;
    ex_mem_reg_ = last.old_ex_mem_reg;
    mem_wb_reg_ = last.old_mem_wb_reg;

    id_stall_ = last.old_id_stall;
    stall_cycles_ = last.old_stall_cycles;
    forward_a_ = last.old_forward_a;
    forward_b_ = last.old_forward_b;
    forward_branch_a_ = last.old_forward_branch_a_;
    forward_branch_b_ = last.old_forward_branch_b_;
    instruction_sequence_counter_ = last.old_instruction_sequence_counter_;
    last_retired_sequence_id_ = last.old_last_retired_sequence_id_;

    //checks if the last cycle resulted in a register write
    //if true then restores the register value to the value that it stored before the cycle
    if (last.wb_write.occurred) {
        
        switch(last.wb_write.reg_type) {
            case 0: // GPR
                registers_.WriteGpr(last.wb_write.reg_index, last.wb_write.old_value);
                break;
            case 2: // FPR
                registers_.WriteFpr(last.wb_write.reg_index, last.wb_write.old_value);
                break;
            default:
                std::cerr << "Runtime Error: Invalid register type in Undo WB writeback." << std::endl;
                break;
        }

    }


    // checks if the last cycle resulted in mem write
    //reverse that change
    if (last.mem_write.occurred) {
        for (size_t i = 0; i < last.mem_write.old_bytes.size(); ++i) {
            memory_controller_.WriteByte(last.mem_write.address + i, last.mem_write.old_bytes[i]);
        }
    }

    //decrements the clock_cycle
    cycle_s_--;
    if (last.instruction_retired && instructions_retired_ > 0) {
        instructions_retired_--;
    }
    // pushes the snapshot to the redo stack
    redo_stack_.push(last);

    std::cout << "VM_UNDO_COMPLETED" << std::endl;
    output_status_ = "VM_UNDO_COMPLETED";
    DumpState(globals::vm_state_dump_file_path);
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpPipelineRegisters(globals::pipeline_registers_dump_file_path);

}

void RV5SVM::DumpPipelineRegisters(const std::filesystem::path &filename) {

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file for dumping pipeline registers: " << filename.string() << std::endl;
        return;
    }

    auto format_hex = [](uint64_t value) {
        std::stringstream ss;
        ss << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
        return ss.str();
    };

    auto format_hex32 = [](uint32_t value) {
        std::stringstream ss;
        ss << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
        return ss.str();
    };

    auto format_hex8 = [](uint8_t value) {
        std::stringstream ss;
        ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<uint32_t>(value);
        return ss.str();
    };

    auto format_bool = [](bool value) {
        return value ? "true" : "false";
    };

    auto format_alu_op = [](alu::AluOp op) {
        std::stringstream ss;
        ss << op; // This uses the operator << 
        return ss.str();
    };

    auto get_line_num = [&](uint64_t pc) -> int {
        // PC is in bytes, map uses instruction index (PC / 4)
        uint32_t instr_index = static_cast<uint32_t>(pc / 4);
        if (program_.instruction_number_line_number_mapping.count(instr_index)) {
            return program_.instruction_number_line_number_mapping[instr_index];
        }
        return -1; // Unknown line
    };

    auto format_fwd = [](ForwardSource src) {
        switch (src) {
            case ForwardSource::kNone: return "None";
            case ForwardSource::kFromExMem: return "ExMem";
            case ForwardSource::kFromMemWb: return "MemWb";
            default: return "None";
        }
    };

    file << "{\n";

    // --- IF/ID Stage ---
    uint64_t IF_PC = if_id_reg_.pc_plus_4 - 4;
    file << "  \"IF_ID\": {\n";
    file << "    \"pc\": \"" << format_hex(IF_PC) << "\",\n";
    file << "    \"line\": " << get_line_num(IF_PC) << ",\n";
    file << "    \"instr\": \"" << format_hex32(if_id_reg_.instruction) << "\",\n";
    file << "    \"predictedTaken\": " << format_bool(if_id_reg_.predictedTaken) << ",\n";
    file << "    \"isStalled\": " << format_bool(id_stall_) << ",\n";
    file << "    \"seq_id\": \"" << if_id_reg_.sequence_id << "\",\n";
    file << "    \"valid\": " << format_bool(if_id_reg_.valid) << "\n";
    file << "  },\n";

    // --- ID/EX Stage ---
    file << "  \"ID_EX\": {\n";
    file << "    \"CurrentPC\": \"" << format_hex(id_ex_reg_.currentPC) << "\",\n";
    file << "    \"line\": " << get_line_num(id_ex_reg_.currentPC) << ",\n";
    file << "    \"rd\": \"" << std::dec << (int)id_ex_reg_.rd << "\",\n";
    file << "    \"rs1\": \"" << std::dec << (int)id_ex_reg_.rs1_idx << "\",\n";
    file << "    \"rs2\": \"" << std::dec << (int)id_ex_reg_.rs2_idx << "\",\n";
    file << "    \"reg1_value\": \"" << format_hex(id_ex_reg_.reg1_value) << "\",\n";
    file << "    \"reg2_value\": \"" << format_hex(id_ex_reg_.reg2_value) << "\",\n";
    file << "    \"imm\": \"" << std::dec << id_ex_reg_.immediate << "\",\n";
    file << "    \"funct3\": \"" << format_hex8(id_ex_reg_.funct3) << "\",\n";
    file << "    \"instr\": \"" << format_hex32(id_ex_reg_.instruction) << "\",\n";
    file << "    \"seq_id\": \"" << id_ex_reg_.sequence_id << "\",\n";

    // Forwarding Sources
    file << "    \"forward_a\": \"" << format_fwd(forward_a_) << "\",\n";
    file << "    \"forward_b\": \"" << format_fwd(forward_b_) << "\",\n";
    file << "    \"forward_branch_a\": \"" << format_fwd(forward_branch_a_) << "\",\n";
    file << "    \"forward_branch_b\": \"" << format_fwd(forward_branch_b_) << "\",\n";
    
    // Control Signals
    file << "    \"RegWrite\": " << format_bool(id_ex_reg_.RegWrite) << ",\n";
    file << "    \"MemRead\": " << format_bool(id_ex_reg_.MemRead) << ",\n";
    file << "    \"MemWrite\": " << format_bool(id_ex_reg_.MemWrite) << ",\n";
    file << "    \"MemToReg\": " << format_bool(id_ex_reg_.MemToReg) << ",\n";
    file << "    \"AluSrc\": " << format_bool(id_ex_reg_.AluSrc) << ",\n";
    file << "    \"AluOperation\": \"" << format_alu_op(id_ex_reg_.AluOperation) << "\",\n";
    file << "    \"isBranch\": " << format_bool(id_ex_reg_.isBranch) << ",\n";
    file << "    \"isJAL\": " << format_bool(id_ex_reg_.isJAL) << ",\n";
    file << "    \"isJump\": " << format_bool(id_ex_reg_.isJump) << ",\n";
    
    // New Prediction Signals
    file << "    \"isMisPredicted\": " << format_bool(id_ex_reg_.isMisPredicted) << ",\n"; // NEW
    file << "    \"actualTargetPC\": \"" << format_hex(id_ex_reg_.actualTargetPC) << "\",\n"; // NEW
    
    file << "    \"valid\": " << format_bool(id_ex_reg_.valid) << "\n";
    file << "  },\n";

    // --- EX/MEM Stage ---
    file << "  \"EX_MEM\": {\n";

    // Control Signals
    file << "    \"RegWrite\": " << format_bool(ex_mem_reg_.RegWrite) << ",\n";
    file << "    \"mem_write\": " << format_bool(ex_mem_reg_.MemWrite) << ",\n";
    file << "    \"mem_read\": " << format_bool(ex_mem_reg_.MemRead) << ",\n";
    file << "    \"MemToReg\": " << format_bool(ex_mem_reg_.MemToReg) << ",\n";

    // Data Signals
    file << "    \"CurrentPC\": \"" << format_hex(ex_mem_reg_.currentPC) << "\",\n";
    file << "    \"line\": " << get_line_num(ex_mem_reg_.currentPC) << ",\n";
    file << "    \"alu_result\": \"" << format_hex(ex_mem_reg_.alu_result) << "\",\n";
    file << "    \"rd\": \"" << std::dec << (int)ex_mem_reg_.rd << "\",\n";
    file << "    \"reg2_value\": \"" << format_hex(ex_mem_reg_.reg2_value) << "\",\n";
    file << "    \"funct3\": \"" << format_hex8(ex_mem_reg_.funct3) << "\",\n";
    file << "    \"instr\": \"" << format_hex32(ex_mem_reg_.instruction) << "\",\n";
    file << "    \"seq_id\": \"" << ex_mem_reg_.sequence_id << "\",\n";

    // Control Hazard Signals
    file << "    \"isControlHazard\": " << format_bool(ex_mem_reg_.isControlHazard) << ",\n";
    file << "    \"targetPC\": \"" << format_hex(ex_mem_reg_.targetPC) << "\",\n";

    file << "    \"valid\": " << format_bool(ex_mem_reg_.valid) << "\n";
    file << "  },\n";

    // --- MEM/WB Stage ---
    file << "  \"MEM_WB\": {\n";

    // Control Signals
    file << "    \"RegWrite\": " << format_bool(mem_wb_reg_.RegWrite) << ",\n";
    file << "    \"MemToReg\": " << format_bool(mem_wb_reg_.MemToReg) << ",\n";

    // Data Signals
    file << "    \"CurrentPC\": \"" << format_hex(mem_wb_reg_.currentPC) << "\",\n";
    file << "    \"line\": " << get_line_num(mem_wb_reg_.currentPC) << ",\n";
    file << "    \"alu_result\": \"" << format_hex(mem_wb_reg_.alu_result) << "\",\n";
    file << "    \"mem_data\": \"" << format_hex(mem_wb_reg_.data_from_memory) << "\",\n";
    file << "    \"rd\": \"" << std::dec << (int)mem_wb_reg_.rd << "\",\n";
    file << "    \"instr\": \"" << format_hex32(mem_wb_reg_.instruction) << "\",\n";
    file << "    \"seq_id\": \"" << mem_wb_reg_.sequence_id << "\",\n";

    file << "    \"valid\": " << format_bool(mem_wb_reg_.valid) << "\n";
    file << "  },\n";

    file << "  \"Retired\": {\n";
    file << "    \"seq_id\": " << last_retired_sequence_id_ << "\n";
    file << "  }\n";

    file << "}\n";
    file.close();

}

void RV5SVM::Redo() {

    // If stack is empty nothing to redo so
    if (redo_stack_.empty()) {
        std::cout << "VM_NO_MORE_REDO" << std::endl;
        output_status_ = "VM_NO_MORE_REDO";
        return;
    }

    // snapshot of the cycle that just got undoed(meanig you want to redo this cycle)
    CycleDelta next = redo_stack_.top();
    // pop it
    redo_stack_.pop();
    //update the pc
    program_counter_ = next.new_pc;

    //update the states of the pipeline registers
    if_id_reg_  = next.new_if_id_reg;
    id_ex_reg_  = next.new_id_ex_reg;
    ex_mem_reg_ = next.new_ex_mem_reg;
    mem_wb_reg_ = next.new_mem_wb_reg;

    id_stall_ = next.new_id_stall;
    stall_cycles_ = next.new_stall_cycles;
    forward_a_ = next.new_forward_a;
    forward_b_ = next.new_forward_b;
    forward_branch_a_ = next.new_forward_branch_a_;
    forward_branch_b_ = next.new_forward_branch_b_;
    instruction_sequence_counter_ = next.new_instruction_sequence_counter_;
    
    // check if a register write occurred in this cycle
    // if it did then update the value of the register with the new value
    
    if (next.wb_write.occurred) {
        
        switch(next.wb_write.reg_type) {
            case 0: // GPR
                registers_.WriteGpr(next.wb_write.reg_index, next.wb_write.new_value);
                break;
            case 2: // FPR
                registers_.WriteFpr(next.wb_write.reg_index, next.wb_write.new_value);
                break;
            default:
                std::cerr << "Runtime Error: Invalid register type in Redo WB writeback." << std::endl;
                break;
        }

    }
    //if mem write occured then update the memory 
    if (next.mem_write.occurred) {
        for (size_t i = 0; i < next.mem_write.new_bytes.size(); ++i) {
            memory_controller_.WriteByte(next.mem_write.address + i, next.mem_write.new_bytes[i]);
        }
    }
    //update the cycle count
    cycle_s_++;
    if (next.instruction_retired) {
        instructions_retired_++;
    }

    // push it onto the undo stack
    undo_stack_.push(next);

    std::cout << "VM_REDO_COMPLETED" << std::endl;
    output_status_ = "VM_REDO_COMPLETED";
    DumpState(globals::vm_state_dump_file_path);
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpPipelineRegisters(globals::pipeline_registers_dump_file_path);

}