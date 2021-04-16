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

#ifndef SRC_CPP_B17PHASE2_HPP_
#define SRC_CPP_B17PHASE2_HPP_

#include "disk.hpp"
#include "progress.hpp"

// Backpropagate takes in as input, a file on which forward propagation has been done.
// The purpose of backpropagate is to eliminate any dead entries that don't contribute
// to final values in f7, to minimize disk usage. A sort on disk is applied to each table,
// so that they are sorted by position.
std::vector<uint64_t> b17RunPhase2(uint8_t *memory,
                                   std::vector<FileDisk> &tmp_1_disks,
                                   const std::vector<uint64_t>& table_sizes,
                                   uint8_t k,
                                   const std::string &tmp_dirname,
                                   const std::string &filename,
                                   uint64_t memory_size,
                                   uint32_t num_buckets,
                                   uint32_t log_num_buckets,
                                   const ProgressCallbackFunc& progressCallback = progressCallbackNone);

#endif // SRC_CPP_B17PHASE2_HPP_
