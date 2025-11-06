/**
 * @file rvss_vm.h
 * @brief RVSS VM definition
 * @author Aric Maji, https://github.com/Adam-Warlock09
 */

#ifndef RV5S_VM_H
#define RV5S_VM_H

#include "vm/vm_base.h"
#include "vm/rv5s/rv5s_control_unit.h"
#include "vm/pipeline_registers.h"
#include <stack>

struct WbWriteInfo {
    bool occurred = false;
    unsigned int reg_index = 0;
    unsigned int reg_type = 0;
    uint64_t old_value = 0;
    uint64_t new_value = 0;
};

struct MemWriteInfo {
    bool occurred = false;
    uint64_t address = 0;
    std::vector<uint8_t> old_bytes;
    std::vector<uint8_t> new_bytes;
};

struct CycleDelta {
    uint64_t old_pc = 0;
    
    IF_ID_Register  old_if_id_reg;
    ID_EX_Register  old_id_ex_reg;
    EX_MEM_Register old_ex_mem_reg;
    MEM_WB_Register old_mem_wb_reg;

    IF_ID_Register  new_if_id_reg;
    ID_EX_Register  new_id_ex_reg;
    EX_MEM_Register new_ex_mem_reg;
    MEM_WB_Register new_mem_wb_reg;

    WbWriteInfo wb_write;
    MemWriteInfo mem_write;
    
    uint64_t new_pc = 0;
    bool instruction_retired = false;
};

class RV5SVM : public VmBase {  

    public:
        IF_ID_Register if_id_reg_{};
        ID_EX_Register id_ex_reg_{};
        EX_MEM_Register ex_mem_reg_{};
        MEM_WB_Register mem_wb_reg_{};

        RV5SControlUnit control_unit_;

        std::stack<CycleDelta> undo_stack_;
        std::stack<CycleDelta> redo_stack_;

        RV5SVM();
        ~RV5SVM();

        void PipelinedStep();

    private:

        // the flag that pipelineDecode will use to tell pipelineStep whether to stall or not
        bool id_stall_ = false;

        enum class ForwardSource{
            kNone, //use the value from the register file
            kFromExMem, // Forward from the EX/MEM pipeline register
            kFromMemWb // forward form the MEM/WB pipeline register
        };

        ForwardSource forward_a_ = ForwardSource::kNone;
        ForwardSource forward_b_ = ForwardSource::kNone;



        
        IF_ID_Register pipelineFetch();
        ID_EX_Register pipelineDecode(const IF_ID_Register& if_id_reg);
        EX_MEM_Register pipelineExecute(const ID_EX_Register& id_ex_reg);
        std::pair<MEM_WB_Register, MemWriteInfo> pipelineMemory(const EX_MEM_Register& ex_mem_reg);
        WbWriteInfo pipelineWriteBack(const MEM_WB_Register& mem_wb_reg);
    
    public:
        void Run() override;
        void DebugRun() override;
        void Step() override;
        void Undo() override;
        void Redo() override;
        void Reset() override;

        void PrintType() {
            std::cout << "rv5svm" << std::endl;
        }

};

#endif // RV5S_VM_H