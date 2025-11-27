/**
 * @file cache.cpp
 * @brief File containing the implementation of the cache class for the virtual machine
 * @author Vishank Singh, https://github.com/VishankSingh
 */
#include "vm/cache/cache.h"

#include <cmath>
#include <cstdlib>
#include <iterator>

namespace cache {

    void Cache::Initialize(const CacheConfig& config) {
        config_ = config;
        if (!config_.cache_enabled || config_.block_size == 0 || config_.associativity == 0 || config_.lines == 0) {
            // Cache is disabled or improperly configured
            return;
        }

        num_sets = config_.lines / config_.associativity;
        offset_bits = static_cast<unsigned long>(std::log2(config_.block_size));
        index_bits = static_cast<unsigned long>(std::log2(num_sets));

        sets.resize(num_sets);

    }

    void Cache::Reset() {
        stats_ = {0, 0, 0, 0};
        for (auto& set : sets) {
            set.lines.clear();
        }
    }

    void Cache::Access(uint64_t address, bool is_write) {
        if (!config_.cache_enabled) {
            return ; // Cache is disabled
        }

        stats_.accesses++;

        uint64_t block_address = address >> offset_bits;
        uint64_t index_mask = (1 << index_bits) - 1;
        uint64_t set_index = block_address & index_mask;

        uint64_t tag = block_address >> index_bits;

        CacheSet& set = sets[set_index];

        bool hit = false;
        for (auto it = set.lines.begin(); it != set.lines.end(); ++it) {

            if (it->valid && it->tag == tag) {
                hit = true;
                stats_.hits++;
                UpdateLRU(set, it);
                break;
            }

        }

        if (!hit) {
            stats_.misses++;
            bool allocate = true;
            if (is_write && config_.write_miss_policy == WriteMissPolicy::NoWriteAllocate) {
                allocate = false;
            }

            if (allocate) {
                AllocateLine(set, tag);
            }
        }

    }

    void Cache::UpdateLRU(CacheSet& set, std::list<CacheLine>::iterator it) {
    
        if (config_.replacement_policy == ReplacementPolicy::LRU) {
            // Move the accessed line to the front of the list
            set.lines.splice(set.lines.begin(), set.lines, it);
        }
    
    }

    void Cache::AllocateLine(CacheSet& set, uint64_t tag) {
     
        if (set.lines.size() >= config_.associativity) {
            stats_.evictions++;
            
            if (config_.replacement_policy == ReplacementPolicy::Random) {
                int victim_index = std::rand() % set.lines.size();
                auto it = set.lines.begin();
                std::advance(it, victim_index);
                set.lines.erase(it);
            } else {
                set.lines.pop_back();
            }

        }

        CacheLine new_line;
        new_line.valid = true;
        new_line.tag = tag;

        set.lines.push_front(new_line);

    }

} // namespace cache
