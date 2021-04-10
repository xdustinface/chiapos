//
// Created by Mariano Sorgente on 2020/09/28.
//
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

#include <entry_sizes.hpp>

uint32_t EntrySizes::GetMaxEntrySize(uint8_t k, uint8_t table_index, bool phase_1_size)
{
    // This represents the largest entry size that each table will have, throughout the
    // entire plotting process. This is useful because it allows us to rewrite tables
    // on top of themselves without running out of space.
    switch (table_index) {
        case 1:
            // Represents f1, x
            if (phase_1_size) {
                return Util::ByteAlign(k + kExtraBits + k) / 8;
            } else {
                // After computing matches, table 1 is rewritten without the f1, which
                // is useless after phase1.
                return Util::ByteAlign(k) / 8;
            }
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            if (phase_1_size)
                // If we are in phase 1, use the max size, with metadata.
                // Represents f, pos, offset, and metadata
                return Util::ByteAlign(
                           k + kExtraBits + (k) + kOffsetSize +
                           k * kVectorLens[table_index + 1]) /
                       8;
            else
                // If we are past phase 1, we can use a smaller size, the smaller between
                // phases 2 and 3. Represents either:
                //    a:  sort_key, pos, offset        or
                //    b:  line_point, sort_key
                return Util::ByteAlign(
                           std::max(static_cast<uint32_t>(2 * k + kOffsetSize),
                               static_cast<uint32_t>(3 * k - 1))) /
                       8;
        case 7:
        default:
            // Represents line_point, f7
            return Util::ByteAlign(3 * k - 1) / 8;
    }
}

uint32_t EntrySizes::GetKeyPosOffsetSize(uint8_t k)
{
    return cdiv(2 * k + kOffsetSize, 8);
}

uint32_t EntrySizes::CalculateC3Size(uint8_t k)
{
    if (k < 20) {
        return Util::ByteAlign(8 * kCheckpoint1Interval) / 8;
    } else {
        return Util::ByteAlign(kC3BitsPerEntry * kCheckpoint1Interval) / 8;
    }
}

uint32_t EntrySizes::CalculateLinePointSize(uint8_t k)
{
    return Util::ByteAlign(2 * k) / 8;
}

uint32_t EntrySizes::CalculateMaxDeltasSize(uint8_t k, uint8_t table_index)
{
    if (table_index == 1) {
        return Util::ByteAlign((kEntriesPerPark - 1) * kMaxAverageDeltaTable1) / 8;
    }
    return Util::ByteAlign((kEntriesPerPark - 1) * kMaxAverageDelta) / 8;
}

uint32_t EntrySizes::CalculateStubsSize(uint32_t k)
{
    return Util::ByteAlign((kEntriesPerPark - 1) * (k - kStubMinusBits)) / 8;
}

uint32_t EntrySizes::CalculateParkSize(uint8_t k, uint8_t table_index)
{
    return CalculateLinePointSize(k) + CalculateStubsSize(k) +
           CalculateMaxDeltasSize(k, table_index);
}
