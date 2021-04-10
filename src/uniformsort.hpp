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

#ifndef SRC_CPP_UNIFORMSORT_HPP_
#define SRC_CPP_UNIFORMSORT_HPP_

#include <disk.hpp>

namespace UniformSort {

    int64_t const BUF_SIZE = 262144;

    bool IsPositionEmpty(const uint8_t *memory, uint32_t const entry_len);

    void SortToMemory(
                FileDisk &input_disk,
                uint64_t const input_disk_begin,
                uint8_t *const memory,
                uint32_t const entry_len,
                uint64_t const num_entries,
                uint32_t const bits_begin);
}

#endif  // SRC_CPP_UNIFORMSORT_HPP_
