// Copyright 2018 Chia Network Inc

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//    http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_CPP_B17SORTMANAGER_HPP_
#define SRC_CPP_B17SORTMANAGER_HPP_

#include <iostream>
#include <string>
#include <vector>

#include "bits.hpp"
#include "disk.hpp"

class b17SortManager {
public:
    b17SortManager(
        uint8_t *memory,
        uint64_t memory_size,
        uint32_t num_buckets,
        uint32_t log_num_buckets,
        uint16_t entry_size,
        const std::string &tmp_dirname,
        const std::string &filename,
        uint32_t begin_bits,
        uint64_t stripe_size);

    void AddToCache(const Bits &entry);
    void AddToCache(const uint8_t *entry);

    uint8_t *ReadEntry(uint64_t position, int quicksort = 0);

    bool CloseToNewBucket(uint64_t position) const;

    void TriggerNewBucket(uint64_t position, bool quicksort);

    void ChangeMemory(uint8_t *new_memory, uint64_t new_memory_size);

    void FlushCache();

    ~b17SortManager();

private:
    // Start of the whole memory array. This will be diveded into buckets
    uint8_t *memory_start;
    // Size of the whole memory array
    uint64_t memory_size;
    // One file for each bucket
    std::vector<FileDisk> bucket_files;
    // Size of each entry
    uint16_t entry_size;
    // Bucket determined by the first "log_num_buckets" bits starting at "begin_bits"
    uint32_t begin_bits;
    // Portion of memory to allocate to each bucket
    uint64_t size_per_bucket;
    // Log of the number of buckets; num bits to use to determine bucket
    uint32_t log_num_buckets;
    // One pointer to the start of each bucket memory
    std::vector<uint8_t *> mem_bucket_pointers;
    // The number of entries written to each bucket
    std::vector<uint64_t> mem_bucket_sizes;
    // The amount of data written to each disk bucket
    std::vector<uint64_t> bucket_write_pointers;
    uint64_t prev_bucket_buf_size;
    uint8_t *prev_bucket_buf;
    uint64_t prev_bucket_position_start;

    bool done;

    uint64_t final_position_start;
    uint64_t final_position_end;
    uint64_t next_bucket_to_sort;
    uint8_t *entry_buf;

    void FlushTable(uint16_t bucket_i);

    void SortBucket(int quicksort);
};

#endif  // SRC_CPP_FAST_SORT_ON_DISK_HPP_
