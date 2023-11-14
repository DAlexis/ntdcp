#include "ntdcp/utils.hpp"

using namespace ntdcp;

BitExtractor::BitExtractor(const uint8_t* buffer, size_t max_size) :
    m_buffer(buffer), m_bytes_left(max_size)
{
}

uint8_t BitExtractor::get(int bits_count)
{
    uint8_t result = 0;
    if (bits_count >= m_bits_left)
    {
        m_bits_left -= bits_count;
        result = *m_buffer & ((1 << bits_count) - 1);
    }
    /// @todo Here
    return result;
}
