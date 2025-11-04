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

RV5SVM::~RV5SVM() = default;

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

    control_unit_.Reset();

    std::cout << "RV5SVM has been reset." << std::endl;

}

void RV5SVM::PipelinedStep() {

    pipelineWriteBack(mem_wb_reg_);

    MEM_WB_Register next_mem_wb_reg = pipelineMemory(ex_mem_reg_);

    EX_MEM_Register next_ex_mem_reg = pipelineExecute(id_ex_reg_);

    ID_EX_Register next_id_ex_reg = pipelineDecode(if_id_reg_);

    IF_ID_Register next_if_id_reg = pipelineFetch();

    if_id_reg_ = next_if_id_reg;
    id_ex_reg_ = next_id_ex_reg;
    ex_mem_reg_ = next_ex_mem_reg;
    mem_wb_reg_ = next_mem_wb_reg;

    program_counter_ = next_if_id_reg.pc_plus_4;

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
    
    ID_EX_Register result;

    if (!if_id_reg.valid) {
    
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
    
    EX_MEM_Register result;

    result.valid = id_ex_reg.valid;
    result.RegWrite = id_ex_reg.RegWrite;
    result.MemRead = id_ex_reg.MemRead;
    result.MemWrite = id_ex_reg.MemWrite;
    result.MemToReg = id_ex_reg.MemToReg;
    result.rd = id_ex_reg.rd;

    if (!id_ex_reg.valid) {
        return result;
    }

    uint64_t operand_a = id_ex_reg.reg1_value;
    uint64_t operand_b = 0;

    if (id_ex_reg.AluSrc) {
        operand_b = static_cast<uint64_t>(static_cast<int64_t>(id_ex_reg.immediate));
    } else {
        operand_b = id_ex_reg.reg2_value;
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

    result.alu_result = ALUResult;
    result.reg2_value = id_ex_reg.reg2_value;
    result.funct3 = id_ex_reg.funct3;

    return result;

}

MEM_WB_Register RV5SVM::pipelineMemory(const EX_MEM_Register& ex_mem_reg) {
    
    MEM_WB_Register result;

    result.valid = ex_mem_reg.valid;
    result.RegWrite = ex_mem_reg.RegWrite;
    result.MemToReg = ex_mem_reg.MemToReg;
    result.rd = ex_mem_reg.rd;
    result.alu_result = ex_mem_reg.alu_result;

    if (!ex_mem_reg.valid) {
        return result;
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
            return result;
        }
    }

    if (ex_mem_reg.MemWrite) {
        try {
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
        } catch (const std::out_of_range& e) {
            std::cerr << "Runtime Error: Memory write failed at address 0x" << std::hex << memoryAddress << " - " << e.what() << std::dec << std::endl;
            result.valid = false;
            result.RegWrite = false;
            result.MemToReg = false;
            return result;
        }
    }

    return result;

}

void RV5SVM::pipelineWriteBack(const MEM_WB_Register& mem_wb_reg) {
    
    if (!mem_wb_reg.valid) {
        return;
    }

    if (mem_wb_reg.RegWrite) {
        
        uint64_t writeValue = 0;

        if (mem_wb_reg.MemToReg) {
            writeValue = mem_wb_reg.data_from_memory;
        } else {
            writeValue = mem_wb_reg.alu_result;
        }

        uint8_t destinationRegister = mem_wb_reg.rd;
        try {
            registers_.WriteGpr(destinationRegister, writeValue);
        } catch (const std::out_of_range& e) {
            std::cerr << "Runtime Error: WriteBack failed writing to GPR x" << static_cast<int>(destinationRegister) << " - " << e.what() << std::endl;
            return;
        }

    }

    instructions_retired_++;

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
    std::cout << "VM_ERROR: DebugRun() is not supported in multi_stage mode." << std::endl;
    output_status_ = "VM_ERROR";
    RequestStop();
}

void RV5SVM::Undo() {
    std::cout << "VM_ERROR: Undo() is not yet implemented for multi_stage mode." << std::endl;
    output_status_ = "VM_ERROR";
}

void RV5SVM::Redo() {
    std::cout << "VM_ERROR: Redo() is not yet implemented for multi_stage mode." << std::endl;
    output_status_ = "VM_ERROR";
}