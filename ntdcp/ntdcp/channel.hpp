#pragma once

#include "ntdcp/utils.hpp"

#include <cstdint>


namespace ntdcp
{

#pragma pack(push, 1)
struct ChannelHeader
{
    constexpr static uint16_t magic_number_value = 0xAB;
    uint16_t magic = magic_number_value;
    uint16_t size = 0;
    uint32_t checksum = 0;
};
#pragma pack(pop)

class ChannelLayer
{
public:
    ChannelLayer();
    std::vector<Buffer::ptr> decode(SerialReadAccessor& ring_buffer);
    void encode(SegmentBuffer& frame);

private:
    enum class State
    {
        waiting_header = 0,
        waiting_buffer
    };

    State m_state = State::waiting_header;
    ChannelHeader m_header;

    struct DecodingInstance
    {
        size_t body_begin = 0;
        ChannelHeader header;
    };

    Buffer::ptr decode_single(SerialReadAccessor& ring_buffer);
    Buffer::ptr find_sucessful_instance(SerialReadAccessor& ring_buffer);
    void find_next_headers(SerialReadAccessor& ring_buffer);

    std::list<DecodingInstance> m_decoding_instances;
    size_t m_header_search_pos = 0;
};

}
