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

class RV5SVM : public VmBase {

    public:
        IF_ID_Register if_id_reg_{};
        ID_EX_Register id_ex_reg_{};
        EX_MEM_Register ex_mem_reg_{};
        MEM_WB_Register mem_wb_reg_{};

        RV5SControlUnit control_unit_;

        RV5SVM();
        ~RV5SVM();

        void PipelinedStep();

    private:
        IF_ID_Register pipelineFetch();
        ID_EX_Register pipelineDecode(const IF_ID_Register& if_id_reg);
        EX_MEM_Register pipelineExecute(const ID_EX_Register& id_ex_reg);
        MEM_WB_Register pipelineMemory(const EX_MEM_Register& ex_mem_reg);
        void pipelineWriteBack(const MEM_WB_Register& mem_wb_reg);
    
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