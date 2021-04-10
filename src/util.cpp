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

#include <cassert>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <util.hpp>

#if defined(_WIN32) || defined(_WIN64)
#include <time.h>
#define localtime_r(a, b) (localtime_s(b, a) == 0 ? b : NULL)
#endif

#ifndef _WIN32
// Allows printing of uint128_t
std::ostream &operator<<(std::ostream &strm, uint128_t const &v)
{
    strm << "uint128(" << (uint64_t)(v >> 64) << "," << (uint64_t)(v & (((uint128_t)1 << 64) - 1))
         << ")";
    return strm;
}
#endif

// compiler-specific byte swap macros.
#if defined(_MSC_VER)

// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/byteswap-uint64-byteswap-ulong-byteswap-ushort?view=msvc-160
inline uint16_t bswap_16(uint16_t x) { return _byteswap_ushort(x); }
inline uint32_t bswap_32(uint32_t x) { return _byteswap_ulong(x); }
inline uint64_t bswap_64(uint64_t x) { return _byteswap_uint64(x); }

#elif defined(__clang__) || defined(__GNUC__)

inline uint16_t bswap_16(uint16_t x) { return __builtin_bswap16(x); }
inline uint32_t bswap_32(uint32_t x) { return __builtin_bswap32(x); }
inline uint64_t bswap_64(uint64_t x) { return __builtin_bswap64(x); }

#else
#error "unknown compiler, don't know how to swap bytes"
#endif

Timer::Timer()
{
    wall_clock_time_start_ = std::chrono::steady_clock::now();
#if _WIN32
    ::GetProcessTimes(::GetCurrentProcess(), &ft_[3], &ft_[2], &ft_[1], &ft_[0]);
#else
    cpu_time_start_ = clock();
#endif
}

std::ostream& operator<< (std::ostream & out, const Timer& timer)
{
    auto end = std::chrono::steady_clock::now();
    auto wall_clock_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - timer.wall_clock_time_start_)
        .count();

#if _WIN32
    FILETIME nowft_[6];
    nowft_[0] = timer.ft_[0];
    nowft_[1] = timer.ft_[1];

    ::GetProcessTimes(::GetCurrentProcess(), &nowft_[5], &nowft_[4], &nowft_[3], &nowft_[2]);
    ULARGE_INTEGER u[4];
    for (size_t i = 0; i < 4; ++i) {
        u[i].LowPart = nowft_[i].dwLowDateTime;
        u[i].HighPart = nowft_[i].dwHighDateTime;
    }
    double user = (u[2].QuadPart - u[0].QuadPart) / 10000.0;
    double kernel = (u[3].QuadPart - u[1].QuadPart) / 10000.0;
    double cpu_time_ms = user + kernel;
#else
    double cpu_time_ms =
        1000.0 * (static_cast<double>(clock()) - timer.cpu_time_start_) / CLOCKS_PER_SEC;
#endif

    double cpu_ratio = static_cast<int>(10000 * (cpu_time_ms / wall_clock_ms)) / 100.0;

    return out << tfm::format("%s seconds. CPU (%s%%)", wall_clock_ms, cpu_ratio);
}

namespace Util {

    uint32_t ByteAlign(uint32_t num_bits) { return (num_bits + (8 - ((num_bits) % 8)) % 8); }

    std::string HexStr(const uint8_t *data, size_t len)
    {
        std::stringstream s;
        s << std::hex;
        for (size_t i = 0; i < len; ++i)
            s << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
        s << std::dec;
        return s.str();
    }

    void IntToTwoBytes(uint8_t *result, const uint16_t input)
    {
        uint16_t r = bswap_16(input);
        memcpy(result, &r, sizeof(r));
    }

    // Used to encode deltas object size
    void IntToTwoBytesLE(uint8_t *result, const uint16_t input)
    {
        result[0] = input & 0xff;
        result[1] = input >> 8;
    }

    uint16_t TwoBytesToInt(const uint8_t *bytes)
    {
        uint16_t i;
        memcpy(&i, bytes, sizeof(i));
        return bswap_16(i);
    }

    /*
     * Converts a 64 bit int to bytes.
     */
    void IntToEightBytes(uint8_t *result, const uint64_t input)
    {
        uint64_t r = bswap_64(input);
        memcpy(result, &r, sizeof(r));
    }

    /*
     * Converts a byte array to a 64 bit int.
     */
    uint64_t EightBytesToInt(const uint8_t *bytes)
    {
        uint64_t i;
        memcpy(&i, bytes, sizeof(i));
        return bswap_64(i);
    }

    void IntTo16Bytes(uint8_t *result, const uint128_t input)
    {
        uint64_t r = bswap_64(input >> 64);
        memcpy(result, &r, sizeof(r));
        r = bswap_64((uint64_t)input);
        memcpy(result + 8, &r, sizeof(r));
    }

    /*
     * Retrieves the size of an integer, in Bits.
     */
    uint8_t GetSizeBits(uint128_t value)
    {
        uint8_t count = 0;
        while (value) {
            count++;
            value >>= 1;
        }
        return count;
    }

    // 'bytes' points to a big-endian 64 bit value (possibly truncated, if
    // (start_bit % 8 + num_bits > 64)). Returns the integer that starts at
    // 'start_bit' that is 'num_bits' long (as a native-endian integer).
    //
    // Note: requires that 8 bytes after the first sliced byte are addressable
    // (regardless of 'num_bits'). In practice it can be ensured by allocating
    // extra 7 bytes to all memory buffers passed to this function.
    uint64_t SliceInt64FromBytes(
        const uint8_t *bytes,
        uint32_t start_bit,
        const uint32_t num_bits)
    {
        uint64_t tmp;

        if (start_bit + num_bits > 64) {
            bytes += start_bit / 8;
            start_bit %= 8;
        }

        tmp = Util::EightBytesToInt(bytes);
        tmp <<= start_bit;
        tmp >>= 64 - num_bits;
        return tmp;
    }

    uint64_t SliceInt64FromBytesFull(
        const uint8_t *bytes,
        uint32_t start_bit,
        uint32_t num_bits)
    {
        uint32_t last_bit = start_bit + num_bits;
        uint64_t r = SliceInt64FromBytes(bytes, start_bit, num_bits);
        if (start_bit % 8 + num_bits > 64)
            r |= bytes[last_bit / 8] >> (8 - last_bit % 8);
        return r;
    }

    uint128_t SliceInt128FromBytes(
        const uint8_t *bytes,
        const uint32_t start_bit,
        const uint32_t num_bits)
    {
        if (num_bits <= 64)
            return SliceInt64FromBytesFull(bytes, start_bit, num_bits);

        uint32_t num_bits_high = num_bits - 64;
        uint64_t high = SliceInt64FromBytesFull(bytes, start_bit, num_bits_high);
        uint64_t low = SliceInt64FromBytesFull(bytes, start_bit + num_bits_high, 64);
        return ((uint128_t)high << 64) | low;
    }

    void GetRandomBytes(uint8_t *buf, uint32_t num_bytes)
    {
        std::random_device rd;
        std::mt19937 mt(rd());
        std::uniform_int_distribution<int> dist(0, 255);
        for (uint32_t i = 0; i < num_bytes; i++) {
            buf[i] = dist(mt);
        }
    }

    std::string GetLocalTimeString()
    {
        char buffer[100];
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm;
        localtime_r(&tt, &now_tm);
        if (strftime(buffer, sizeof buffer, "%d-%m-%Y %H:%M:%S", &now_tm) == 0) {
            return "GetLocalTimeString failed.";
        }
        return buffer;
    }

    uint64_t ExtractNum(
        const uint8_t *bytes,
        uint32_t len_bytes,
        uint32_t begin_bits,
        uint32_t take_bits)
    {
        if ((begin_bits + take_bits) / 8 > len_bytes - 1) {
            take_bits = len_bytes * 8 - begin_bits;
        }
        return Util::SliceInt64FromBytes(bytes, begin_bits, take_bits);
    }

    // The number of memory entries required to do the custom SortInMemory algorithm, given the
    // total number of entries to be sorted.
    uint64_t RoundSize(uint64_t size)
    {
        size *= 2;
        uint64_t result = 1;
        while (result < size) result *= 2;
        return result + 50;
    }

    /*
     * Like memcmp, but only compares starting at a certain bit.
     */
    int MemCmpBits(
        uint8_t *left_arr,
        uint8_t *right_arr,
        uint32_t len,
        uint32_t bits_begin)
    {
        uint32_t start_byte = bits_begin / 8;
        uint8_t mask = ((1 << (8 - (bits_begin % 8))) - 1);
        if ((left_arr[start_byte] & mask) != (right_arr[start_byte] & mask)) {
            return (left_arr[start_byte] & mask) - (right_arr[start_byte] & mask);
        }

        for (uint32_t i = start_byte + 1; i < len; i++) {
            if (left_arr[i] != right_arr[i])
                return left_arr[i] - right_arr[i];
        }
        return 0;
    }

    double RoundPow2(double a)
    {
        // https://stackoverflow.com/questions/54611562/truncate-float-to-nearest-power-of-2-in-c-performance
        int exp;
        double frac = frexp(a, &exp);
        if (frac > 0.0)
            frac = 0.5;
        else if (frac < 0.0)
            frac = -0.5;
        double b = ldexp(frac, exp);
        return b;
    }

#if defined(_WIN32) || defined(__x86_64__)
    void CpuID(uint32_t leaf, uint32_t *regs)
    {
#if defined(_WIN32)
        __cpuid((int *)regs, (int)leaf);
#else
        __get_cpuid(leaf, &regs[0], &regs[1], &regs[2], &regs[3]);
#endif /* defined(_WIN32) */
    }

    bool HavePopcnt(void)
    {
        // EAX, EBX, ECX, EDX
        uint32_t regs[4] = {0};

        CpuID(1, regs);
        // Bit 23 of ECX indicates POPCNT instruction support
        return (regs[2] >> 23) & 1;
    }
#endif /* defined(_WIN32) || defined(__x86_64__) */

    uint64_t PopCount(uint64_t n)
    {
#if defined(_WIN32)
        return __popcnt64(n);
#elif defined(__x86_64__)
        uint64_t r;
        __asm__("popcnt %1, %0" : "=r"(r) : "r"(n));
        return r;
#else
        return __builtin_popcountl(n);
#endif /* defined(_WIN32) ... defined(__x86_64__) */
    }

    std::ostream& LogStream(std::ostream* pStreamIn)
    {
        static std::ostream* pStream = &std::cout;
        if (pStreamIn != nullptr) {
            pStream = pStreamIn;
        }
        return *pStream;
    }

    void LogElapsed(const std::string& strEvent, const Timer& timer)
    {
#ifdef _UTIL_LOGS_ENABLED_
        Log("%s - time: %s %s\n", strEvent, timer, GetLocalTimeString());
#endif
    }
}
