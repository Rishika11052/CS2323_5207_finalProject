/**
 * @file memory_controller.h
 * @brief Contains the declaration of the MemoryController class for managing memory in the VM.
 * @author Vishank Singh, https://github.com/VishankSingh
 */

#ifndef MEMORY_CONTROLLER_H
#define MEMORY_CONTROLLER_H

#include "../config.h"
#include "main_memory.h"
#include "cache/cache.h"

#include <iostream>
#include <string>
#include <vector>


/**
 * @brief The MemoryController class is responsible for managing memory in the VM.
 */
class MemoryController {
private:
    Memory memory_; ///< The main memory object.
    cache::Cache cache_; ///< The cache object.

    void Probe(uint64_t address, bool is_write) {
        cache_.Access(address, is_write);
    }
public:
    MemoryController() = default;

    void Init(const cache::CacheConfig& config) {
      cache_.Initialize(config);
    }

    void Reset() {
        memory_.Reset();
        cache_.Reset();
    }

    cache::CacheStats GetCacheStats() const {
      return cache_.GetStats();
    }

    void PrintCacheStatus() const {
      cache::CacheStats s = cache_.GetStats();
        if (cache_.GetConfig().cache_enabled) {
            std::cout << "\n[Cache Statistics]\n"
                      << "Accesses:  " << s.accesses << "\n"
                      << "Hits:      " << s.hits << "\n"
                      << "Misses:    " << s.misses << "\n"
                      << "Evictions: " << s.evictions << "\n"
                      << "Hit Rate:  " << (s.accesses > 0 ? (double)s.hits/s.accesses : 0.0) * 100.0 << "%\n"
                      << std::endl;
        }
    }

    void WriteByte(uint64_t address, uint8_t value) {
      Probe(address, true);
      memory_.WriteByte(address, value);
    }

    void WriteHalfWord(uint64_t address, uint16_t value) {
      Probe(address, true);
      memory_.WriteHalfWord(address, value);
    }

    void WriteWord(uint64_t address, uint32_t value) {
      Probe(address, true);
      memory_.WriteWord(address, value);
    }

    void WriteDoubleWord(uint64_t address, uint64_t value) {
      Probe(address, true);
      memory_.WriteDoubleWord(address, value);
    }

    // Write functions bypassing cache
    void WriteByte_d(uint64_t address, uint8_t value) {
        memory_.WriteByte(address, value);
    }

    void WriteHalfWord_d(uint64_t address, uint16_t value) {
        memory_.WriteHalfWord(address, value);
    }

    void WriteWord_d(uint64_t address, uint32_t value) {
        memory_.WriteWord(address, value);
    }

    void WriteDoubleWord_d(uint64_t address, uint64_t value) {
        memory_.WriteDoubleWord(address, value);
    }

    [[nodiscard]] uint8_t ReadByte(uint64_t address) {
        Probe(address, false);
        return memory_.ReadByte(address);
    }

    [[nodiscard]] uint16_t ReadHalfWord(uint64_t address) {
        Probe(address, false);
        return memory_.ReadHalfWord(address);
    }

    [[nodiscard]] uint32_t ReadWord(uint64_t address) {
        Probe(address, false);
        return memory_.ReadWord(address);
    }

    [[nodiscard]] uint64_t ReadDoubleWord(uint64_t address) {
        Probe(address, false);
        return memory_.ReadDoubleWord(address);
    }

    // Functions to read memory directly with cache bypass

    [[nodiscard]] uint8_t ReadByte_d(uint64_t address) {
        return memory_.ReadByte(address);
    }

    [[nodiscard]] uint16_t ReadHalfWord_d(uint64_t address) {
        return memory_.ReadHalfWord(address);
    }

    [[nodiscard]] uint32_t ReadWord_d(uint64_t address) {
        return memory_.ReadWord(address);
    }

    [[nodiscard]] uint64_t ReadDoubleWord_d(uint64_t address) {
        return memory_.ReadDoubleWord(address);
    }

    void PrintMemory(const uint64_t address, unsigned int rows) {
      memory_.PrintMemory(address, rows);
    }

    void DumpMemory(std::vector<std::string> args) {
      memory_.DumpMemory(args);
    }

    void GetMemoryPoint(std::string address) {
      return memory_.GetMemoryPoint(address);
    }

};

#endif // MEMORY_CONTROLLER_H

