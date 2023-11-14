#pragma once

#include <cstdint>
#include <cstdlib>

namespace ntdcp
{

class BitExtractor
{
public:
    BitExtractor(const uint8_t* buffer, size_t max_size);

    uint8_t get(int bits_count);

private:
    const uint8_t* m_buffer;
    size_t m_bytes_left;

    uint8_t m_bits_left = 8;
};

}
