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


// initializes the 5-stage virtual machine
RV5SVM::RV5SVM() : VmBase() {

    Reset();
    try {
        DumpRegisters(globals::registers_dump_file_path, registers_);
        DumpState(globals::vm_state_dump_file_path);
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

    control_unit_.Reset();

    DumpState(globals::vm_state_dump_file_path);
    DumpRegisters(globals::registers_dump_file_path, registers_);

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
    
    // flag is used to initialize the CycleDelta Object
    //when mem_wb_reg.valid is true(when WriteBack happens)
    //so that the uno/redo functions know whether they need to increment or decrement instruction_retired_ count
    delta.instruction_retired = false;

    // Run the Write Back Stage
    WbWriteInfo WBInfo = pipelineWriteBack(mem_wb_reg_);
    if (mem_wb_reg_.valid) {
        delta.instruction_retired = true;
    }

    //Run the Memory Stage
    std::pair<MEM_WB_Register, MemWriteInfo> MemInfo = pipelineMemory(ex_mem_reg_);
    MEM_WB_Register next_mem_wb_reg = MemInfo.first;
    //keeps track of data that was overwritten to some memory
    MemWriteInfo mem_info = MemInfo.second;

    //Run the Execute Stage
    EX_MEM_Register next_ex_mem_reg = pipelineExecute(id_ex_reg_);
    
    //Run the Decode Stage
    ID_EX_Register next_id_ex_reg = pipelineDecode(if_id_reg_);    

    IF_ID_Register next_if_id_reg;

    // CONTROL LOGIC FOR HAZARD DUE TO BRANCHES AND JUMPS AND STALLS
    // Handles Branch Misprediction or jump because of execute stage
    if (PCFromEX_){

        // Misprediction detected (Modes iii, iv, v)
        // Is hazard Detection is on , then flush the pipeline
        if (vm_config::config.isHazardDetectionEnabled()) {

            program_counter_ = PCTarget_; // update PC to branch target
            next_if_id_reg = pipelineFetch(); // Fetch from the new PC

            next_id_ex_reg = ID_EX_Register(); // Flush the instruction in ID/EX stage
            stall_cycles_++; // Increment stall cycles

        } else {

            // Mode (ii) - No hazard detection, just update PC
            program_counter_ = PCTarget_; // update PC to branch target
            next_if_id_reg = pipelineFetch(); // Fetch from the new PC

        }

        delta.new_pc = next_if_id_reg.pc_plus_4; // update PC for Undo/Redo
        PCFromEX_ = false; // reset the flag

    } else if(IDPredictTaken_) {

        // Predicted Taken (Mode v)
        // This Code Path is taken when the branch was predicted taken in ID stage and Mode v is active and its not a misprediction
        program_counter_ = IDBranchTarget_; // update PC to branch target
        next_if_id_reg = pipelineFetch(); // Fetch from the new PC
        delta.new_pc = next_if_id_reg.pc_plus_4; // update PC for Undo/Redo

    } else if(id_stall_){
        // Don't Fetch . Freeze the IF_ID register by using its current value
        next_if_id_reg = if_id_reg_;
        // Don't update pc
        delta.new_pc = program_counter_;

    }else{
        //No Stall so normal fetch
        next_if_id_reg = pipelineFetch();
        //Update PC
        // this stores what the pc's value will be at the end of the current clock cycle
        delta.new_pc = next_if_id_reg.pc_plus_4;

        if (!PCFromEX_ && !IDPredictTaken_){
            //only update pc here if not changing it due to branch/jump
            program_counter_ = next_if_id_reg.pc_plus_4;        
        }

    } 
    
    // Save all the calculated values into the pipeline registers
    delta.wb_write = WBInfo;
    delta.mem_write = mem_info;    
    if_id_reg_ = next_if_id_reg;
    id_ex_reg_ = next_id_ex_reg;
    ex_mem_reg_ = next_ex_mem_reg;
    mem_wb_reg_ = next_mem_wb_reg;

    // After stage for the redo function
    delta.new_ex_mem_reg = next_ex_mem_reg;
    delta.new_id_ex_reg = next_id_ex_reg;
    delta.new_if_id_reg = next_if_id_reg;
    delta.new_mem_wb_reg = next_mem_wb_reg;

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

    return result;

}

ID_EX_Register RV5SVM::pipelineDecode(const IF_ID_Register& if_id_reg) {

    IDPredictTaken_ = false;
    IDBranchTarget_ = 0;
    
    ID_EX_Register result;

    if (!if_id_reg.valid) {
    
        // if it was set true by a previous cycle which is now invalid
        id_stall_ = false;
    
        result.valid = false;
        result.RegWrite = false;
        result.MemRead = false;
        result.MemWrite = false;
        result.MemToReg = false;
        result.AluSrc = false;
        result.AluOperation = alu::AluOp::kNone;

        return result;

    }

    uint32_t instruction = if_id_reg.instruction;
    uint8_t opcode = instruction & 0b1111111;
    uint8_t rd = (instruction >> 7) & 0b11111;
    uint8_t funct3 = (instruction >> 12) & 0b111;
    uint8_t rs1 = (instruction >> 15) & 0b11111;
    uint8_t rs2 = (instruction >> 20) & 0b11111;
    uint8_t funct7 = (instruction >> 25) & 0b1111111;

    bool usesRS1 = (opcode != 0b0110111) && (opcode != 0b0010111) && (opcode != 0b1101111); // U-type and JAL instructions do not use rs1
    bool usesRS2 = (opcode == 0b0100011) || (opcode == 0b0110011) || (opcode == 0b1100011); // S-type, R-type, B-type use rs2

    //Hazard Detection
    if(vm_config::config.isHazardDetectionEnabled()){

        //check if the instruction in EX stage is valid, if its a load instruction, if its not x0 and if rd = rs1 or rs2 of current instruction
        bool isRS1LoadUseHazard = usesRS1 && id_ex_reg_.valid && id_ex_reg_.MemRead && id_ex_reg_.rd != 0 && (id_ex_reg_.rd == rs1);
        bool isRS2LoadUseHazard = usesRS2 && id_ex_reg_.valid && id_ex_reg_.MemRead && id_ex_reg_.rd != 0 && (id_ex_reg_.rd == rs2);
        bool isLoadUseHazard = isRS1LoadUseHazard || isRS2LoadUseHazard;
        if(isLoadUseHazard){
            //stall so set flag to true
            id_stall_ = true;

            // Increment stall cycles
            stall_cycles_++;
            std::cout << "Load-Use Hazard Detected: Stalling pipeline." << std::endl;

            //return a bubble
            return ID_EX_Register();
    
        }

        if (!vm_config::config.isForwardingEnabled()) {

            // Check for hazards from EX stage (when forwarding is disabled)
            // If the instruction in EX stage is valid, writes to a register, not x0, and its rd matches rs1 or rs2 of current instruction
            bool isRS1HazardFromEXStage = usesRS1 && id_ex_reg_.valid && id_ex_reg_.RegWrite && id_ex_reg_.rd != 0 && (id_ex_reg_.rd == rs1);
            bool isRS2HazardFromEXStage = usesRS2 && id_ex_reg_.valid && id_ex_reg_.RegWrite && id_ex_reg_.rd != 0 && (id_ex_reg_.rd == rs2);
            bool isHazardFromEXStage = isRS1HazardFromEXStage || isRS2HazardFromEXStage;

            if (isHazardFromEXStage) {
                // Stall due to hazard
                id_stall_ = true;

                // Increment stall cycles
                stall_cycles_++;
                std::cout << "Hazard Detected from EX Stage: Stalling pipeline." << std::endl;

                // Return a bubble
                return ID_EX_Register();
            }

            // Check for hazards from MEM stage (when forwarding is disabled)
            // If the instruction in MEM stage is valid, writes to a register, not x0, and its rd matches rs1 or rs2 of current instruction
            bool isRS1HazardFromMemStage = usesRS1 && ex_mem_reg_.valid && ex_mem_reg_.RegWrite && ex_mem_reg_.rd != 0 && (ex_mem_reg_.rd == rs1);
            bool isRS2HazardFromMemStage = usesRS2 && ex_mem_reg_.valid && ex_mem_reg_.RegWrite && ex_mem_reg_.rd != 0 && (ex_mem_reg_.rd == rs2);
            bool isHazardFromMemStage = isRS1HazardFromMemStage || isRS2HazardFromMemStage;

            if (isHazardFromMemStage) {
                // Stall due to hazard
                id_stall_ = true;

                // Increment stall cycles
                stall_cycles_++;
                std::cout << "Hazard Detected from MEM Stage: Stalling pipeline." << std::endl;

                // Return a bubble
                return ID_EX_Register();
            }

        }

    }

    //no hazard or hazard detection is disabled
    id_stall_ = false;

    //Data Forwarding
    //default set to no forwarding
    forward_a_ = ForwardSource::kNone;
    forward_b_ = ForwardSource::kNone;

    // check if forwarding is enabled
    if(vm_config::config.isForwardingEnabled()){
        
        // --- Check for hazards from EX/MEM stage ---
        // (This data is from 2 cycles ago, so let it be overridden by newer data from ID/EX stage)
        if (ex_mem_reg_.valid && ex_mem_reg_.RegWrite && ex_mem_reg_.rd != 0) {
            if (ex_mem_reg_.rd == rs1 && usesRS1) {
                forward_a_ = ForwardSource::kFromMemWb;
            }
            if (ex_mem_reg_.rd == rs2 && usesRS2) {
                forward_b_ = ForwardSource::kFromMemWb;
            }
        }

        // --- Check for hazards from ID/EX stage ---
        // (This data is from 1 cycle ago, so it has the highest priority, so let it override previous forwarding decisions)
        if (id_ex_reg_.valid && id_ex_reg_.RegWrite && id_ex_reg_.rd != 0) {
            // Forward from ID/EX only if it's not a load instruction
            if (!id_ex_reg_.MemRead) { 
                if (id_ex_reg_.rd == rs1 && usesRS1) {
                    forward_a_ = ForwardSource::kFromExMem;
                }
                if (id_ex_reg_.rd == rs2 && usesRS2) {
                    forward_b_ = ForwardSource::kFromExMem;
                }
            }
        }
        
    }
    result.immediate = ImmGenerator(instruction);

    try {
        result.reg1_value = registers_.ReadGpr(rs1);
        result.reg2_value = registers_.ReadGpr(rs2);
    } catch (const std::out_of_range& e) {
        std::cerr << "Runtime Error: Decode failed reading registers for instruction 0x" << std::hex << instruction << " - " << e.what() << std::dec << std::endl;
        result.valid = false;
        result.RegWrite = false;
        result.MemRead = false;
        result.MemWrite = false;
        result.MemToReg = false;
        result.AluSrc = false;
        result.AluOperation = alu::AluOp::kNone;
        return result;
    }

    control_unit_.GenerateSignalForInstruction(instruction);

    // Set control signals for branch instructions
    result.currentPC = if_id_reg.pc_plus_4 - 4; // Current PC is PC + 4 - 4 = PC
    result.isBranch = control_unit_.GetBranch(); // True for branch instructions
    result.isJAL = (opcode == 0b1101111); // True for JAL, False for JALR
    result.isJump = (result.isJAL || opcode == 0b1100111); // JAL or JALR
    result.predictedTaken = false; // Default to not taken

    if (vm_config::config.getBranchPredictionType() == vm_config::BranchPredictionType::STATIC) {
        // Static Branch Prediction: Predict branches as taken
        if (result.isJAL) {
            IDPredictTaken_ = true; // JAL is always predicted taken
            IDBranchTarget_ = result.currentPC + static_cast<int64_t>(result.immediate);
            result.predictedTaken = true;
        } else if (result.isBranch && static_cast<int64_t>(result.immediate) < 0) {
            // Backward branches are predicted taken
            result.predictedTaken = true;
            IDPredictTaken_ = true;
            IDBranchTarget_ = result.currentPC + static_cast<int64_t>(result.immediate);
        }
    } else if (vm_config::config.getBranchPredictionType() == vm_config::BranchPredictionType::DYNAMIC1BIT) {

        if (result.isJAL) {
            // JAL is always predicted taken
            IDPredictTaken_ = true;
            IDBranchTarget_ = result.currentPC + static_cast<int64_t>(result.immediate);
            result.predictedTaken = true;
        } else if (result.isBranch) {
            // Check the Branch History Table for prediction
            if (branch_history_table_.count(result.currentPC) && branch_history_table_[result.currentPC] == true) {
                // Predicted taken
                result.predictedTaken = true;
                IDPredictTaken_ = true;
                IDBranchTarget_ = result.currentPC + static_cast<int64_t>(result.immediate);
            }
        }

    }

    result.RegWrite = control_unit_.GetRegWrite();
    result.MemRead = control_unit_.GetMemRead();
    result.MemWrite = control_unit_.GetMemWrite();
    result.MemToReg = control_unit_.GetMemToReg();
    result.AluSrc = control_unit_.GetAluSrc();
    result.AluOperation = control_unit_.GetAluOperation(instruction);

    result.rd = rd;
    result.rs1_idx = rs1;
    result.rs2_idx = rs2;
    result.pc_plus_4 = if_id_reg.pc_plus_4;
    result.funct3 = funct3;

    result.valid = true;

    return result;

}

EX_MEM_Register RV5SVM::pipelineExecute(const ID_EX_Register& id_ex_reg) {

    PCFromEX_ = false; // Reset control hazard signal
    PCTarget_ = 0; // Reset PC target

    EX_MEM_Register result;

    result.valid = id_ex_reg.valid;
    result.RegWrite = id_ex_reg.RegWrite;
    result.MemRead = id_ex_reg.MemRead;
    result.MemWrite = id_ex_reg.MemWrite;
    result.MemToReg = id_ex_reg.MemToReg;
    result.rd = id_ex_reg.rd;

    if (!id_ex_reg.valid) {
        return result; // Pass the Bubble
    }

    // forwarding mux for operand a
    uint64_t operand_a = 0;
    switch(forward_a_){
        case ForwardSource::kNone : operand_a = id_ex_reg.reg1_value;
            break;
        case ForwardSource::kFromExMem : operand_a = ex_mem_reg_.alu_result;
            break;
        case ForwardSource::kFromMemWb :
            if (mem_wb_reg_.MemToReg){
                operand_a = mem_wb_reg_.data_from_memory;
            } else {
                operand_a = mem_wb_reg_.alu_result;
            }
            break;
    }

    //forwarding mux for operand b
    uint64_t operand_b = 0;

    if (id_ex_reg.AluSrc) {
        operand_b = static_cast<uint64_t>(static_cast<int64_t>(id_ex_reg.immediate));
    } else {
        switch(forward_b_) {
            case ForwardSource::kNone : operand_b = id_ex_reg.reg2_value;
                break;
            case ForwardSource::kFromExMem: operand_b = ex_mem_reg_.alu_result;
                break;
            case ForwardSource::kFromMemWb: 
                if(mem_wb_reg_.MemToReg){
                    operand_b = mem_wb_reg_.data_from_memory;
                }else{
                    operand_b = mem_wb_reg_.alu_result;
                }
                break;
        }
    }

    uint64_t ALUResult = 0;
    bool overflow = false;

    try {
        std::tie(ALUResult, overflow) = alu_.execute(id_ex_reg.AluOperation, operand_a, operand_b);
    } catch (const std::exception& e) {
        std::cerr << "Runtime Error: ALU execution failed for instruction with ALU operation " << static_cast<int>(id_ex_reg.AluOperation) << " - " << e.what() << std::endl;
        result.valid = false;
        result.RegWrite = false;
        result.MemRead = false;
        result.MemWrite = false;
        result.MemToReg = false;
        return result;
    }

    // LUI Case
    if (id_ex_reg.AluOperation == alu::AluOp::kLUI) {
        ALUResult = static_cast<uint64_t>(id_ex_reg.immediate << 12);
    }

    // AUIPC Case
    if (id_ex_reg.AluOperation == alu::AluOp::kAUIPC) {
        ALUResult = id_ex_reg.currentPC + static_cast<int64_t>(id_ex_reg.immediate);
    }

    // Branch Handling

    if (id_ex_reg.isJump) {

        // Jumps ( JAL and JALR ) Always taken
        PCFromEX_ = true;

        if (id_ex_reg.isJAL) {
            // JAL
            PCTarget_ = id_ex_reg.currentPC + static_cast<int64_t>(id_ex_reg.immediate);
        } else {
            // JALR
            PCTarget_ = (operand_a + static_cast<int64_t>(id_ex_reg.immediate)) & ~1ULL; // Ensure LSB is 0
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

        if (branchTaken != id_ex_reg.predictedTaken) {

            // Misprediction
            PCFromEX_ = true;

            if (branchTaken) {
                PCTarget_ = id_ex_reg.currentPC + static_cast<int64_t>(id_ex_reg.immediate);
            } else {
                PCTarget_ = id_ex_reg.currentPC + 4;
            }

        }

        if (vm_config::config.getBranchPredictionType() == vm_config::BranchPredictionType::DYNAMIC1BIT) {
            // Update Branch History Table
            branch_history_table_[id_ex_reg.currentPC] = branchTaken;
        }

    }

    result.alu_result = ALUResult;
    result.reg2_value = id_ex_reg.reg2_value;
    result.funct3 = id_ex_reg.funct3;

    return result;

}

std::pair<MEM_WB_Register, MemWriteInfo> RV5SVM::pipelineMemory(const EX_MEM_Register& ex_mem_reg) {
    
    MEM_WB_Register result;
    MemWriteInfo writeInfo;
    writeInfo.occurred = false;

    result.valid = ex_mem_reg.valid;
    result.RegWrite = ex_mem_reg.RegWrite;
    result.MemToReg = ex_mem_reg.MemToReg;
    result.rd = ex_mem_reg.rd;
    result.alu_result = ex_mem_reg.alu_result;

    if (!ex_mem_reg.valid) {
        return {result, writeInfo};
    }

    uint64_t memoryAddress = ex_mem_reg.alu_result;

    if (ex_mem_reg.MemRead) {
        try {
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
        } catch (const std::out_of_range& e) {
            std::cerr << "Runtime Error: Memory read failed at address 0x" << std::hex << memoryAddress << " - " << e.what() << std::dec << std::endl;
            result.valid = false;
            result.RegWrite = false;
            result.MemToReg = false;
            return {result, writeInfo};
        }
    }

    if (ex_mem_reg.MemWrite) {
        writeInfo.occurred = true;
        writeInfo.address = memoryAddress;

        size_t writeSize = 0;

        switch (ex_mem_reg.funct3) {
            case 0b000: writeSize = 1; break; // SB
            case 0b001: writeSize = 2; break; // SH
            case 0b010: writeSize = 4; break; // SW
            case 0b011: writeSize = 8; break; // SD
            default:
                throw std::runtime_error("Invalid funct3 for store instruction");
        }

        try {
            writeInfo.old_bytes.resize(writeSize);
            for (size_t i = 0; i < writeSize; ++i) {
                writeInfo.old_bytes[i] = memory_controller_.ReadByte(memoryAddress + i);
            }
            switch (ex_mem_reg.funct3) {
                case 0b000: // SB
                    memory_controller_.WriteByte(memoryAddress, static_cast<uint8_t>(ex_mem_reg.reg2_value & 0xFF));
                    break;
                case 0b001: // SH
                    memory_controller_.WriteHalfWord(memoryAddress, static_cast<uint16_t>(ex_mem_reg.reg2_value & 0xFFFF));
                    break;
                case 0b010: // SW
                    memory_controller_.WriteWord(memoryAddress, static_cast<uint32_t>(ex_mem_reg.reg2_value & 0xFFFFFFFF));
                    break;
                case 0b011: // SD
                    memory_controller_.WriteDoubleWord(memoryAddress, ex_mem_reg.reg2_value);
                    break;
                default:
                    throw std::runtime_error("Invalid funct3 for store instruction");
            }

            writeInfo.new_bytes.resize(writeSize);
            for (size_t i = 0; i < writeSize; ++i) {
                writeInfo.new_bytes[i] = memory_controller_.ReadByte(memoryAddress + i);
            }

        } catch (const std::out_of_range& e) {
            std::cerr << "Runtime Error: Memory write failed at address 0x" << std::hex << memoryAddress << " - " << e.what() << std::dec << std::endl;
            result.valid = false;
            result.RegWrite = false;
            result.MemToReg = false;
            writeInfo.occurred = false;
            return {result, writeInfo};
        }
    }

    return {result, writeInfo};

}

WbWriteInfo RV5SVM::pipelineWriteBack(const MEM_WB_Register& mem_wb_reg) {
    
    WbWriteInfo WBInfo;
    WBInfo.occurred = false;

    if (!mem_wb_reg.valid) {
        return WBInfo;
    }

    if (mem_wb_reg.RegWrite) {
        
        uint64_t writeValue = 0;

        if (mem_wb_reg.MemToReg) {
            writeValue = mem_wb_reg.data_from_memory;
        } else {
            writeValue = mem_wb_reg.alu_result;
        }

        uint8_t destinationRegister = mem_wb_reg.rd;
        if (destinationRegister != 0) {
            try {
                WBInfo.reg_type = 0;
                WBInfo.reg_index = destinationRegister;
                WBInfo.old_value = registers_.ReadGpr(destinationRegister);
                registers_.WriteGpr(destinationRegister, writeValue);
                WBInfo.new_value = writeValue;
                WBInfo.occurred = true;
            } catch (const std::out_of_range& e) {
                std::cerr << "Runtime Error: WriteBack failed writing to GPR x" << static_cast<int>(destinationRegister) << " - " << e.what() << std::endl;
                WBInfo.occurred = false;
                return WBInfo;
            }            
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

    DumpState(globals::vm_state_dump_file_path);
    DumpRegisters(globals::registers_dump_file_path, registers_);

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
        if (std::find(breakpoints_.begin(), breakpoints_.end(), program_counter_) != breakpoints_.end()) {
            std::cout << "VM_BREAKPOINT_HIT: 0x" << std::hex << program_counter_ << std::dec << std::endl;
            output_status_ = "VM_BREAKPOINT_HIT";
            RequestStop(); // Stop the run loop
        }
        // --- End of Breakpoint Check ---
    }

    if (program_counter_ >= program_size_ && !if_id_reg_.valid && !id_ex_reg_.valid && !ex_mem_reg_.valid && !mem_wb_reg_.valid) {
        std::cout << "VM_PROGRAM_END" << std::endl;
        output_status_ = "VM_PROGRAM_END";
    }

    DumpState(globals::vm_state_dump_file_path);
    DumpRegisters(globals::registers_dump_file_path, registers_);

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

    //checks if the last cycle resulted in a register write
    //if true then restores the register value to the value that it stored before the cycle
    if (last.wb_write.occurred) {
        
        switch(last.wb_write.reg_type) {
            case 0: // GPR
                registers_.WriteGpr(last.wb_write.reg_index, last.wb_write.old_value);
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
    
    // check if a register write occurred in this cycle
    // if it did then update the value of the register with the new value
    
    if (next.wb_write.occurred) {
        
        switch(next.wb_write.reg_type) {
            case 0: // GPR
                registers_.WriteGpr(next.wb_write.reg_index, next.wb_write.new_value);
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

}