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

#ifndef SRC_CPP_PHASE3_HPP_
#define SRC_CPP_PHASE3_HPP_

#include "encoding.hpp"
#include "entry_sizes.hpp"
#include "exceptions.hpp"
#include "pos_constants.hpp"
#include "sort_manager.hpp"
#include "progress.hpp"

struct Phase2Results;

// Results of phase 3. These are passed into Phase 4, so the checkpoint tables
// can be properly built.
struct Phase3Results {
    // Pointers to each table start byet in the final file
    std::vector<uint64_t> final_table_begin_pointers;
    // Number of entries written for f7
    uint64_t final_entries_written;
    uint32_t right_entry_size_bits;

    uint32_t header_size;
    std::unique_ptr<SortManager> table7_sm;
};

// This writes a number of entries into a file, in the final, optimized format. The park
// contains a checkpoint value (which is a 2k bits line point), as well as EPP (entries per
// park) entries. These entries are each divided into stub and delta section. The stub bits are
// encoded as is, but the delta bits are optimized into a variable encoding scheme. Since we
// have many entries in each park, we can approximate how much space each park with take. Format
// is: [2k bits of first_line_point]  [EPP-1 stubs] [Deltas size] [EPP-1 deltas]....
// [first_line_point] ...
void WriteParkToFile(
    FileDisk &final_disk,
    uint64_t table_start,
    uint64_t park_index,
    uint32_t park_size_bytes,
    uint128_t first_line_point,
    const std::vector<uint8_t> &park_deltas,
    const std::vector<uint64_t> &park_stubs,
    uint8_t k,
    uint8_t table_index,
    uint8_t *park_buffer,
    uint64_t const park_buffer_size);

// Compresses the plot file tables into the final file. In order to do this, entries must be
// reorganized from the (pos, offset) bucket sorting order, to a more free line_point sorting
// order. In (pos, offset ordering), we store two pointers two the previous table, (x, y) which
// are very close together, by storing  (x, y-x), or (pos, offset), which can be done in about k
// + 8 bits, since y is in the next bucket as x. In order to decrease this, We store the actual
// entries from the previous table (e1, e2), instead of pos, offset pointers, and sort the
// entire table by (e1,e2). Then, the deltas between each (e1, e2) can be stored, which require
// around k bits.

// Converting into this format requires a few passes and sorts on disk. It also assumes that the
// backpropagation step happened, so there will be no more dropped entries. See the design
// document for more details on the algorithm.
Phase3Results RunPhase3(uint8_t k,
                        FileDisk &tmp2_disk,
                        Phase2Results& res2,
                        const std::string &tmp_dirname,
                        const std::string &filename,
                        uint32_t header_size,
                        uint64_t memory_size,
                        uint32_t num_buckets,
                        uint32_t log_num_buckets,
                        const ProgressCallbackFunc& progressCallback = progressCallbackNone);

#endif  // SRC_CPP_PHASE3_HPP_
