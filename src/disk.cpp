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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

using namespace std::chrono_literals; // for operator""min;

#include "chia_filesystem.hpp"

#include "bits.hpp"
#include <disk.hpp>
#include "util.hpp"
#include "bitfield.hpp"

constexpr uint64_t write_cache = 1024 * 1024;
constexpr uint64_t read_ahead = 1024 * 1024;

#if ENABLE_LOGGING
void disk_log(fs::path const& filename, op_t const op, uint64_t offset, uint64_t length)
{
    static std::mutex m;
    static std::unordered_map<std::string, int> file_index;
    static auto const start_time = std::chrono::steady_clock::now();
    static int next_file = 0;

    auto const timestamp = std::chrono::steady_clock::now() - start_time;

    int fd = ::open("disk.log", O_WRONLY | O_CREAT | O_APPEND, 0755);

    std::unique_lock<std::mutex> l(m);

    char buffer[512];

    int const index = [&] {
        auto it = file_index.find(filename.string());
        if (it != file_index.end()) return it->second;
        file_index[filename.string()] = next_file;

        int const len = std::snprintf(buffer, sizeof(buffer)
            , "# %d %s\n", next_file, filename.string().c_str());
        ::write(fd, buffer, len);
        return next_file++;
    }();

    // timestamp (ms), start-offset, end-offset, operation (0 = read, 1 = write), file_index
    int const len = std::snprintf(buffer, sizeof(buffer)
        , "%" PRId64 "\t%" PRIu64 "\t%" PRIu64 "\t%d\t%d\n"
        , std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count()
        , offset
        , offset + length
        , int(op)
        , index);
    ::write(fd, buffer, len);
    ::close(fd);
}
#endif

FileDisk::FileDisk(const fs::path &filename)
{
    filename_ = filename;
    Open(writeFlag);
}

void FileDisk::Open(uint8_t flags)
{
    // if the file is already open, don't do anything
    if (f_) return;

    // Opens the file for reading and writing
    do {
#ifdef _WIN32
        f_ = ::_wfopen(filename_.c_str(), (flags & writeFlag) ? L"w+b" : L"r+b");
#else
        f_ = ::fopen(filename_.c_str(), (flags & writeFlag) ? "w+b" : "r+b");
#endif
        if (f_ == nullptr) {
            std::string error_message =
                "Could not open " + filename_.string() + ": " + ::strerror(errno) + ".";
            if (flags & retryOpenFlag) {
                Util::Log(error_message + " Retrying in five minutes.\n");
                std::this_thread::sleep_for(5min);
            } else {
                throw InvalidValueException(error_message);
            }
        }
    } while (f_ == nullptr);
}

FileDisk::FileDisk(FileDisk &&fd)
{
    filename_ = std::move(fd.filename_);
    f_ = fd.f_;
    fd.f_ = nullptr;
}

void FileDisk::Close()
{
    if (f_ == nullptr) return;
    ::fclose(f_);
    f_ = nullptr;
    readPos = 0;
    writePos = 0;
}

FileDisk::~FileDisk()
{
    Close();
}

void FileDisk::Read(uint64_t begin, uint8_t *memcache, uint64_t length)
{
    Open(retryOpenFlag);
#if ENABLE_LOGGING
    disk_log(filename_, op_t::read, begin, length);
#endif
    // Seek, read, and replace into memcache
    uint64_t amtread;
    do {
        if ((!bReading) || (begin != readPos)) {
#ifdef _WIN32
            _fseeki64(f_, begin, SEEK_SET);
#else
            // fseek() takes a long as offset, make sure it's wide enough
            static_assert(sizeof(long) >= sizeof(begin));
            ::fseek(f_, begin, SEEK_SET);
#endif
            bReading = true;
        }
        amtread = ::fread(reinterpret_cast<char *>(memcache), sizeof(uint8_t), length, f_);
        readPos = begin + amtread;
        if (amtread != length) {
            Util::Log(("Only read %s of %s bytes at offset %s from %s with length %s. "
                      "Error %s. Retrying in five minutes.\n"), amtread, length, begin,
                                                                filename_, writeMax, ferror(f_));
            std::this_thread::sleep_for(5min);
        }
    } while (amtread != length);
}

void FileDisk::Write(uint64_t begin, const uint8_t *memcache, uint64_t length)
{
    Open(writeFlag | retryOpenFlag);
#if ENABLE_LOGGING
    disk_log(filename_, op_t::write, begin, length);
#endif
    // Seek and write from memcache
    uint64_t amtwritten;
    do {
        if ((bReading) || (begin != writePos)) {
#ifdef _WIN32
            _fseeki64(f_, begin, SEEK_SET);
#else
            // fseek() takes a long as offset, make sure it's wide enough
            static_assert(sizeof(long) >= sizeof(begin));
            ::fseek(f_, begin, SEEK_SET);
#endif
            bReading = false;
        }
        amtwritten =
            ::fwrite(reinterpret_cast<const char *>(memcache), sizeof(uint8_t), length, f_);
        writePos = begin + amtwritten;
        if (writePos > writeMax)
            writeMax = writePos;
        if (amtwritten != length) {
            Util::Log(("Only wrote %s of %s bytes at offset %s to %s with length %s. "
                       "Error %s. Retrying in five minutes.\n"), amtwritten, length, begin,
                      filename_, writeMax, ferror(f_));
            std::this_thread::sleep_for(5min);
        }
    } while (amtwritten != length);
}

std::string FileDisk::GetFileName()
{
    return filename_.string();
}

uint64_t FileDisk::GetWriteMax() const noexcept
{
    return writeMax;
}

void FileDisk::Truncate(uint64_t new_size)
{
    Close();
    fs::resize_file(filename_, new_size);
}

BufferedDisk::BufferedDisk(FileDisk* disk, uint64_t file_size) : disk_(disk), file_size_(file_size) {}

uint8_t const* BufferedDisk::Read(uint64_t begin, uint64_t length)
{
    assert(length < read_ahead);
    NeedReadCache();
    // all allocations need 7 bytes head-room, since
    // SliceInt64FromBytes() may overrun by 7 bytes
    if (read_buffer_start_ <= begin
        && read_buffer_start_ + read_buffer_size_ >= begin + length
        && read_buffer_start_ + read_ahead >= begin + length + 7)
    {
        // if the read is entirely inside the buffer, just return it
        return read_buffer_.get() + (begin - read_buffer_start_);
    }
    else if (begin >= read_buffer_start_ || begin == 0 || read_buffer_start_ == std::uint64_t(-1)) {

        // if the read is beyond the current buffer (i.e.
        // forward-sequential) move the buffer forward and read the next
        // buffer-capacity number of bytes.
        // this is also the case we enter the first time we perform a read,
        // where we haven't read anything into the buffer yet. Note that
        // begin == 0 won't reliably detect that case, sinec we may have
        // discarded the first entry and start at some low offset but still
        // greater than 0
        read_buffer_start_ = begin;
        uint64_t const amount_to_read = std::min(file_size_ - read_buffer_start_, read_ahead);
        disk_->Read(begin, read_buffer_.get(), amount_to_read);
        read_buffer_size_ = amount_to_read;
        return read_buffer_.get();
    }
    else {
        // ideally this won't happen
        Util::Log("Disk read position regressed. It's optimized for forward scans."
                  " Performance may suffer.\n");
        Util::Log(" read-offset: %s read-length: %s file-size: %s "
                  "read-buffer: [%s,%s] file: %s\n", begin, length, file_size_,
                                                     read_buffer_start_, read_buffer_size_,
                                                disk_->GetFileName());
        static uint8_t temp[128];
        // all allocations need 7 bytes head-room, since
        // SliceInt64FromBytes() may overrun by 7 bytes
        assert(length <= sizeof(temp) - 7);

        // if we're going backwards, don't wipe out the cache. We assume
        // forward sequential access
        disk_->Read(begin, temp, length);
        return temp;
    }
}

void BufferedDisk::Write(uint64_t const begin, const uint8_t *memcache, uint64_t const length)
{
    NeedWriteCache();
    if (begin == write_buffer_start_ + write_buffer_size_) {
        if (write_buffer_size_ + length <= write_cache) {
            ::memcpy(write_buffer_.get() + write_buffer_size_, memcache, length);
            write_buffer_size_ += length;
            return;
        }
        FlushCache();
    }

    if (write_buffer_size_ == 0 && write_cache >= length) {
        write_buffer_start_ = begin;
        ::memcpy(write_buffer_.get() + write_buffer_size_, memcache, length);
        write_buffer_size_ = length;
        return;
    }

    disk_->Write(begin, memcache, length);
}

void BufferedDisk::Truncate(uint64_t const new_size)
{
    FlushCache();
    disk_->Truncate(new_size);
    file_size_ = new_size;
    FreeMemory();
}

std::string BufferedDisk::GetFileName()
{
    return disk_->GetFileName();
}

void BufferedDisk::FreeMemory()
{
    FlushCache();

    read_buffer_.reset();
    write_buffer_.reset();
    read_buffer_size_ = 0;
    write_buffer_size_ = 0;
}

void BufferedDisk::FlushCache()
{
    if (write_buffer_size_ == 0) return;

    disk_->Write(write_buffer_start_, write_buffer_.get(), write_buffer_size_);
    write_buffer_size_ = 0;
}

void BufferedDisk::NeedReadCache()
{
    if (read_buffer_) return;
    read_buffer_.reset(new uint8_t[read_ahead]);
    read_buffer_start_ = -1;
    read_buffer_size_ = 0;
}

void BufferedDisk::NeedWriteCache()
{
    if (write_buffer_) return;
    write_buffer_.reset(new uint8_t[write_cache]);
    write_buffer_start_ = -1;
    write_buffer_size_ = 0;
}

FilteredDisk::FilteredDisk(BufferedDisk underlying, bitfield filter, int entry_size)
    : filter_(std::move(filter))
    , underlying_(std::move(underlying))
    , entry_size_(entry_size)
{
    assert(entry_size_ > 0);
    while (!filter_.get(last_idx_)) {
        last_physical_ += entry_size_;
        ++last_idx_;
    }
    assert(filter_.get(last_idx_));
    assert(last_physical_ == last_idx_ * entry_size_);
}

uint8_t const* FilteredDisk::Read(uint64_t begin, uint64_t length)
{
    // we only support a single read-pass with no going backwards
    assert(begin >= last_logical_);
    assert((begin % entry_size_) == 0);
    assert(filter_.get(last_idx_));
    assert(last_physical_ == last_idx_ * entry_size_);

    if (begin > last_logical_) {
        // last_idx_ et.al. always points to an entry we have (i.e. the bit
        // is set). So when we advance from there, we always take at least
        // one step on all counters.
        last_logical_ += entry_size_;
        last_physical_ += entry_size_;
        ++last_idx_;

        while (begin > last_logical_)
        {
            if (filter_.get(last_idx_)) {
                last_logical_ += entry_size_;
            }
            last_physical_ += entry_size_;
            ++last_idx_;
        }

        while (!filter_.get(last_idx_)) {
            last_physical_ += entry_size_;
            ++last_idx_;
        }
    }

    assert(filter_.get(last_idx_));
    assert(last_physical_ == last_idx_ * entry_size_);
    assert(begin == last_logical_);
    return underlying_.Read(last_physical_, length);
}

void FilteredDisk::Write(uint64_t begin, const uint8_t *memcache, uint64_t length)
{
    assert(false);
    throw std::runtime_error("Write() called on read-only disk abstraction");
}

void FilteredDisk::Truncate(uint64_t new_size)
{
    underlying_.Truncate(new_size);
    if (new_size == 0) filter_.free_memory();
}

std::string FilteredDisk::GetFileName()
{
    return underlying_.GetFileName();
}

void FilteredDisk::FreeMemory()
{
    filter_.free_memory();
    underlying_.FreeMemory();
}
