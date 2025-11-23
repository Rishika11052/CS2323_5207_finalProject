#ifndef FP_REGISTERS_H
#define FP_REGISTERS_H

#include <cstdint>
#include <array>
#include <stdexcept>
#include <cstring>

class FpRegisters {
public:
    FpRegisters() { Reset(); }

    void Reset() { fpr_.fill(0); }

    uint64_t ReadFpr(uint8_t index) const {
        if (index >= 32) throw std::out_of_range("FP Register index out of bounds");
        return fpr_[index];
    }

    void WriteFpr(uint8_t index, uint64_t value) {
        if (index >= 32) throw std::out_of_range("FP Register index out of bounds");
        fpr_[index] = value;
    }

private:
    std::array<uint64_t, 32> fpr_;
};

#endif