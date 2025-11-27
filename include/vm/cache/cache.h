/**
 * @file cache.h
 * @brief File containing the cache class for the virtual machine
 * @author Vishank Singh, https://github.com/VishankSingh
 */
#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <vector>
#include <list>

namespace cache {

enum class ReplacementPolicy {
  LRU,    ///< Least Recently Used
  FIFO,   ///< First In First Out
  Random  ///< Random replacement
};

enum class WriteMissPolicy {
  NoWriteAllocate, ///< Do not allocate on write miss
  WriteAllocate    ///< Allocate on write miss
};

struct CacheConfig {
  bool cache_enabled = false; ///< Flag to indicate if the cache is enabled
  unsigned long lines = 0;  ///< Number of lines in the cache
  unsigned long associativity = 1; ///< Associativity of the cache
  unsigned long block_size = 4; ///< Block size of the cache in bytes
  ReplacementPolicy replacement_policy = ReplacementPolicy::LRU; ///< Replacement policy for the cache
  WriteMissPolicy write_miss_policy = WriteMissPolicy::NoWriteAllocate; ///< Write miss policy
  unsigned long size = 0;   ///< Size of the cache in bytes
};

struct CacheLine {
  bool valid = false;      ///< Valid bit for the cache line
  uint64_t tag = 0;    ///< Tag for the cache line  
};

struct CacheStats {
  unsigned long accesses; ///< Total number of accesses to the cache
  unsigned long hits;     ///< Total number of hits in the cache
  unsigned long misses;   ///< Total number of misses in the cache
  unsigned long evictions; ///< Total number of evictions from the cache
};

struct CacheSet {
  std::list<CacheLine> lines; ///< Cache lines
};

class Cache {
  public:
    Cache() = default;
    ~Cache() = default;

    void Initialize(const CacheConfig& config);
    void Reset();
    void Access(uint64_t address, bool is_write);

    CacheStats GetStats() const {
      return stats_;
    };

    CacheConfig GetConfig() const {
      return config_;
    };

  private:
    CacheConfig config_;               ///< Configuration of the cache
    CacheStats stats_ = {0, 0, 0, 0};  ///< Statistics of the cache

    std::vector<CacheSet> sets;

    unsigned long num_sets = 0;
    unsigned long offset_bits = 0;
    unsigned long index_bits = 0;

    void UpdateLRU(CacheSet& set, std::list<CacheLine>::iterator it);
    void AllocateLine(CacheSet& set, uint64_t tag);
  
  };

} // namespace cache

#endif // CACHE_H