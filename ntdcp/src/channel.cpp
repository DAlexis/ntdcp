#include "ntdcp/channel.hpp"


using namespace ntdcp;

ChannelLayer::ChannelLayer()
{
}

Buffer::ptr ChannelLayer::find_sucessful_instance(SerialReadAccessor& accessor)
{
    for (auto it = m_decoding_instances.begin(); it != m_decoding_instances.end();)
    {
        size_t size = it->header.size;
        if (size  + it->body_begin > accessor.size())
        {
            ++it;
            continue;
        }

        // Try to test
        uint32_t hash_value = 0;
        for (size_t i = 0; i != size; i++)
        {
            uint8_t byte = accessor[i + it->body_begin];
            hash_value = hash_Ly(&byte, 1, hash_value);
        }

        if (hash_value != it->header.checksum)
        {
            // Block is incorrect, bad checksum
            it = m_decoding_instances.erase(it);
            continue;
        }

        size_t this_block_begin = it->body_begin;
        size_t this_block_end = this_block_begin + size;

        // Seccess, we have correct block
        Buffer::ptr pbuf = Buffer::create(size);
        accessor.skip(this_block_begin);
        accessor.extract(pbuf->data(), size);

        // Erasing all blocks that intersect current block

        // First, removing all previously found parsing instances, that are already in list.
        // They must intersect our block, otherwise they are already checked and discarded by hash
        for (auto jt = m_decoding_instances.begin(); jt != it; )
        {
            jt = m_decoding_instances.erase(jt);
        }

        m_decoding_instances.erase(it);

        for (auto jt = m_decoding_instances.begin(); jt != m_decoding_instances.end(); )
        {
            if (jt->body_begin < this_block_end)
            {
                // We have intersection
                jt = m_decoding_instances.erase(jt);
            } else {
                jt->body_begin -= this_block_end;
                ++jt;
            }
        }

        if (m_header_search_pos > this_block_end)
            m_header_search_pos -= this_block_end;
        else
            m_header_search_pos = 0;
        return pbuf;

    }
    return nullptr;
}

void ChannelLayer::find_next_headers(SerialReadAccessor& accessor)
{
    if (accessor.size() < sizeof(ChannelHeader))
        return;

    if (m_decoding_instances.empty())
    {
        // we had not detected header before, so rewind buffer to headers begin
        // TODO add code here
        accessor.skip(m_header_search_pos);
        size_t search_limit = accessor.size() - sizeof(ChannelHeader);
        for (m_header_search_pos = 0; m_header_search_pos < search_limit; ++m_header_search_pos)
        {
            if (accessor.as<typeof(ChannelHeader::magic)>(m_header_search_pos) == ChannelHeader::magic_number_value)
                break;
        }
        accessor.skip(m_header_search_pos);
        m_header_search_pos = 0;
    }

    size_t search_limit = accessor.size() - sizeof(ChannelHeader);
    for (size_t i = m_header_search_pos; i < search_limit; ++i)
    {
        if (accessor.as<typeof(ChannelHeader::magic)>(i) != ChannelHeader::magic_number_value)
            continue;

        DecodingInstance new_instance;
        new_instance.body_begin = i + sizeof(ChannelHeader);
        new_instance.header = accessor.as<ChannelHeader>(i);

        m_decoding_instances.push_back(new_instance);
    }
    m_header_search_pos = search_limit;
}

Buffer::ptr ChannelLayer::decode_single(SerialReadAccessor& accessor)
{
    find_next_headers(accessor);
    return find_sucessful_instance(accessor);
}

std::vector<Buffer::ptr> ChannelLayer::decode(SerialReadAccessor& accessor)
{
    std::vector<Buffer::ptr> result;

    Buffer::ptr next_frame_contents = nullptr;
    for (;;)
    {
        next_frame_contents = decode_single(accessor);
        if (!next_frame_contents)
            break;
        result.push_back(next_frame_contents);
    }

    return result;
}

void ChannelLayer::encode(SegmentBuffer& frame)
{
    uint32_t hash = 0;
    for (const auto& seg : frame.segments())
    {
        hash = hash_Ly(seg->data(), seg->size(), hash);
    }
    ChannelHeader header;
    header.checksum = hash;
    header.size = frame.size();
    frame.push_front(Buffer::create(sizeof(header), &header));
}
