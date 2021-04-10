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

#ifndef SRC_CPP_FAST_SORT_ON_DISK_HPP_
#define SRC_CPP_FAST_SORT_ON_DISK_HPP_

#include <string>
#include <vector>

#include <disk.hpp>

enum class strategy_t : uint8_t
{
    uniform,
    quicksort,

    // the quicksort_last strategy is important because uniform sort performs
    // really poorly on data that isn't actually uniformly distributed. The last
    // buckets are often not uniformly distributed.
    quicksort_last,
};

class SortManager : public Disk {
public:
    SortManager(
        uint64_t const memory_size,
        uint32_t const num_buckets,
        uint32_t const log_num_buckets,
        uint16_t const entry_size,
        const std::string &tmp_dirname,
        const std::string &filename,
        uint32_t begin_bits,
        uint64_t const stripe_size,
        strategy_t const sort_strategy = strategy_t::uniform);

    void AddToCache(const Bits &entry);

    void AddToCache(const uint8_t *entry);

    uint8_t const* Read(uint64_t begin, uint64_t length) override;

    void Write(uint64_t, uint8_t const*, uint64_t) override;

    void Truncate(uint64_t new_size) override;

    std::string GetFileName() override;

    void FreeMemory() override;

    uint8_t *ReadEntry(uint64_t position);

    bool CloseToNewBucket(uint64_t position) const;

    void TriggerNewBucket(uint64_t position);

    void FlushCache();

    ~SortManager();

private:

    struct bucket_t
    {
        bucket_t(FileDisk f) : underlying_file(std::move(f)), file(&underlying_file, 0) {}

        // The amount of data written to the disk bucket
        uint64_t write_pointer = 0;

        // The file for the bucket
        FileDisk underlying_file;
        BufferedDisk file;
    };

    // The buffer we use to sort buckets in-memory
    std::unique_ptr<uint8_t[]> memory_start_;
    // Size of the whole memory array
    uint64_t memory_size_;
    // Size of each entry
    uint16_t entry_size_;
    // Bucket determined by the first "log_num_buckets" bits starting at "begin_bits"
    uint32_t begin_bits_;
    // Log of the number of buckets; num bits to use to determine bucket
    uint32_t log_num_buckets_;

    std::vector<bucket_t> buckets_;

    uint64_t prev_bucket_buf_size;
    std::unique_ptr<uint8_t[]> prev_bucket_buf_;
    uint64_t prev_bucket_position_start = 0;

    bool done = false;

    uint64_t final_position_start = 0;
    uint64_t final_position_end = 0;
    uint64_t next_bucket_to_sort = 0;
    std::unique_ptr<uint8_t[]> entry_buf_;
    strategy_t strategy_;

    void SortBucket();
};

#endif  // SRC_CPP_FAST_SORT_ON_DISK_HPP_
