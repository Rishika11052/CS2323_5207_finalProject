/**
 * @file rvss_control_unit.h
 * @brief RVSS Control Unit
 * @author Aric Maji, https://github.com/Adam-Warlock09
 */

#ifndef RV5S_CONTROL_UNIT_H
#define RV5S_CONTROL_UNIT_H

#include "vm/rvss/rvss_control_unit.h"
#include "vm/alu.h"

#include <cstdint>

class RV5SControlUnit {

    private:
        RVSSControlUnit baseControlLogic;
    
    public:
        RV5SControlUnit() = default;

        void GenerateSignalForInstruction(uint32_t instruction) {
            baseControlLogic.SetControlSignals(instruction);
        }

        alu::AluOp GetAluOperation(uint32_t instruction) {
            return baseControlLogic.GetAluSignal(instruction, baseControlLogic.GetAluOp());
        }

        bool GetRegWrite() const {
            return baseControlLogic.GetRegWrite();
        }

        bool GetMemRead() const {
            return baseControlLogic.GetMemRead();
        }

        bool GetMemWrite() const {
            return baseControlLogic.GetMemWrite();
        }

        bool GetAluSrc() const {
            return baseControlLogic.GetAluSrc();
        }

        bool GetMemToReg() const {
            return baseControlLogic.GetMemToReg();
        }

        bool GetBranch() const {
            return baseControlLogic.GetBranch();
        }

        uint8_t GetAluOp() const {
            return baseControlLogic.GetAluOp();
        }

        void Reset() {
            baseControlLogic.Reset();
        }

};

#endif // RV5S_CONTROL_UNIT_H