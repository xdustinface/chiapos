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

#ifndef SRC_CPP_PHASE1_HPP_
#define SRC_CPP_PHASE1_HPP_

#include <vector>

#include "disk.hpp"
#include "progress.hpp"

typedef std::vector<uint64_t> Phase1Results;

// This is Phase 1, or forward propagation. During this phase, all of the 7 tables,
// and f functions, are evaluated. The result is an intermediate plot file, that is
// several times larger than what the final file will be, but that has all of the
// proofs of space in it. First, F1 is computed, which is special since it uses
// ChaCha8, and each encryption provides multiple output values. Then, the rest of the
// f functions are computed, and a sort on disk happens for each table.
Phase1Results RunPhase1(
    std::vector<FileDisk>& tmp_1_disks,
    uint8_t const k,
    const uint8_t* const id,
    std::string const tmp_dirname,
    std::string const filename,
    uint64_t const memory_size,
    uint32_t const num_buckets,
    uint32_t const log_num_buckets,
    uint32_t const stripe_size,
    uint8_t const num_threads,
    bool const enable_bitfield,
    const ProgressCallbackFunc& progressCallback = progressCallbackNone);

#endif  // SRC_CPP_PHASE1_HPP_
