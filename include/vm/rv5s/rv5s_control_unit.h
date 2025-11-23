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

        bool IsRs1FPR(uint32_t instruction) const {
            uint8_t opcode = instruction & 0b1111111;
            uint8_t funct7 = (instruction >> 25) & 0b1111111;

            // Note: FLW/FSW use RS1 for base address calculation, which is always GPR (Integer).
            
            if (opcode == 0b1010011) { // OP-FP (Arithmetic/Convert)
                // FCVT.S.W / FCVT.D.W (Int -> Float): 0b1101000 / 0b1101001
                // FMV.W.X / FMV.D.X   (Int -> Float): 0b1111000 / 0b1111001
                // These instructions read from Integer Registers (GPR) to create a Float.
                if ((funct7 & 0b1111000) == 0b1101000) return false; // FCVT Int->Float
                if ((funct7 & 0b1111000) == 0b1111000) return false; // FMV Int->Float
                
                // All other OP-FP instructions (FADD, FCVT.W.S, FSQRT, etc.) read RS1 from FPR
                return true;
            }
            
            // Fused Multiply-Add instructions (0x43, 0x47, 0x4B, 0x4F) would return true here,
            // but are excluded per project requirements.
            
            return false;
        }

        // Determines if RS2 should be read from the Floating Point Register (FPR) file
        bool IsRs2FPR(uint32_t instruction) const {
            uint8_t opcode = instruction & 0b1111111;

            if (opcode == 0b0100111) return true; // Store-FP (FSW/FSD): RS2 is the data to store (FPR)
            if (opcode == 0b1010011) return true; // OP-FP: If RS2 is used (FADD, FSUB, etc.), it's always FPR

            return false;
        }

        // Determines if RD should be written to the Floating Point Register (FPR) file
        bool IsRdFPR(uint32_t instruction) const {
            uint8_t opcode = instruction & 0b1111111;
            uint8_t funct7 = (instruction >> 25) & 0b1111111;

            if (opcode == 0b0000111) return true; // Load-FP (FLW/FLD): Writes loaded data to FPR
            
            if (opcode == 0b1010011) { // OP-FP
                // FCVT.W.S / FCVT.L.S (Float -> Int): 0b1100000 / 0b1100001
                // FMV.X.W / FCLASS    (Float -> Int): 0b1110000 / 0b1110001
                // FCMP (FEQ/FLT/FLE)  (Float -> Int): 0b1010000 / 0b1010001
                // These instructions write the result to Integer Registers (GPR).
                
                if ((funct7 & 0b1111000) == 0b1100000) return false; // FCVT Float->Int
                if ((funct7 & 0b1111000) == 0b1110000) return false; // FMV Float->Int / Class
                if ((funct7 & 0b1111000) == 0b1010000) return false; // Comparisons (Result is 0/1 Int)
                
                return true; // FADD, FCVT Int->Float, FSQRT, etc. write to FPR
            }

            return false;
        }

};

#endif // RV5S_CONTROL_UNIT_H