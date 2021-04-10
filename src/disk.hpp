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

#ifndef SRC_CPP_DISK_HPP_
#define SRC_CPP_DISK_HPP_

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

// enables disk I/O logging to disk.log
// use tools/disk.gnuplot to generate a plot
#define ENABLE_LOGGING 0

#include "chia_filesystem.hpp"

#include "bits.hpp"
#include "util.hpp"
#include "bitfield.hpp"

struct Disk {
    virtual uint8_t const* Read(uint64_t begin, uint64_t length) = 0;
    virtual void Write(uint64_t begin, const uint8_t *memcache, uint64_t length) = 0;
    virtual void Truncate(uint64_t new_size) = 0;
    virtual std::string GetFileName() = 0;
    virtual void FreeMemory() = 0;
    virtual ~Disk() = default;
};

#if ENABLE_LOGGING
#include <mutex>
#include <unordered_map>
#include <cinttypes>

enum class op_t : int { read, write};

void disk_log(fs::path const& filename, op_t const op, uint64_t offset, uint64_t length);
#endif

struct FileDisk {
    explicit FileDisk(const fs::path &filename);

    void Open(uint8_t flags = 0);

    FileDisk(FileDisk &&fd);

    FileDisk(const FileDisk &) = delete;
    FileDisk &operator=(const FileDisk &) = delete;

    void Close();

    ~FileDisk();

    void Read(uint64_t begin, uint8_t *memcache, uint64_t length);

    void Write(uint64_t begin, const uint8_t *memcache, uint64_t length);

    std::string GetFileName();

    uint64_t GetWriteMax() const noexcept;

    void Truncate(uint64_t new_size);

private:

    uint64_t readPos = 0;
    uint64_t writePos = 0;
    uint64_t writeMax = 0;
    bool bReading = true;

    fs::path filename_;
    FILE *f_ = nullptr;

    static const uint8_t writeFlag = 0b01;
    static const uint8_t retryOpenFlag = 0b10;
};

struct BufferedDisk : Disk
{
    BufferedDisk(FileDisk* disk, uint64_t file_size);

    uint8_t const* Read(uint64_t begin, uint64_t length) override;

    void Write(uint64_t const begin, const uint8_t *memcache, uint64_t const length) override;

    void Truncate(uint64_t const new_size) override;

    std::string GetFileName() override;

    void FreeMemory() override;

    void FlushCache();

private:

    void NeedReadCache();

    void NeedWriteCache();

    FileDisk* disk_;

    uint64_t file_size_;

    // the file offset the read buffer was read from
    uint64_t read_buffer_start_ = -1;
    std::unique_ptr<uint8_t[]> read_buffer_;
    uint64_t read_buffer_size_ = 0;

    // the file offset the write buffer should be written back to
    // the write buffer is *only* for contiguous and sequential writes
    uint64_t write_buffer_start_ = -1;
    std::unique_ptr<uint8_t[]> write_buffer_;
    uint64_t write_buffer_size_ = 0;
};

struct FilteredDisk : Disk
{
    FilteredDisk(BufferedDisk underlying, bitfield filter, int entry_size);

    uint8_t const* Read(uint64_t begin, uint64_t length) override;

    void Write(uint64_t begin, const uint8_t *memcache, uint64_t length) override;
    void Truncate(uint64_t new_size) override;
    std::string GetFileName() override;
    void FreeMemory() override;

private:

    // only entries whose bit is set should be read
    bitfield filter_;
    BufferedDisk underlying_;
    int entry_size_;

    // the "physical" disk offset of the last read
    uint64_t last_physical_ = 0;
    // the "logical" disk offset of the last read. i.e. the offset as if the
    // file would have been compacted based on filter_
    uint64_t last_logical_ = 0;

    // the index of the last read. This is also the index into the bitfield. It
    // could be computed as last_physical_ / entry_size_, but we want to avoid
    // the division.
    uint64_t last_idx_ = 0;
};

#endif  // SRC_CPP_DISK_HPP_
