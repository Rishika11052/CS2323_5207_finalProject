/**
 * @file rv5s_vm.cpp
 * @brief RV5S VM (5-Stage Pipeline) implementation
 * @author Aric Maji, https://github.com/Adam-Warlock09
 */

#include "vm/rv5s/rv5s_vm.h"
#include "vm/vm_base.h"
#include "config.h"
#include "globals.h"
#include "utils.h"

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

    

}