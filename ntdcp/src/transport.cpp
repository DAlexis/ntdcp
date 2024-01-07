#include "ntdcp/transport.hpp"

using namespace ntdcp;

SocketReceiver::SocketReceiver(uint16_t my_port) :
    m_my_port(my_port)
{
}

uint16_t SocketReceiver::port() const
{
    return m_my_port;
}


SocketTransmitter::SocketTransmitter(uint16_t target_port, uint64_t remote_addr, uint8_t hop_limit) :
    m_remote_port(target_port), m_remote_addr(remote_addr), m_hop_limit(hop_limit)
{
}

uint16_t SocketTransmitter::remote_port() const
{
    return m_remote_port;
}

uint64_t SocketTransmitter::remote_addr() const
{
    return m_remote_addr;
}

uint8_t SocketTransmitter::hop_limit() const
{
    return m_hop_limit;
}


TransportLayer::TransportLayer(NetworkLayer::ptr network) :
    m_network(network)
{
}

void TransportLayer::add_receiver(SocketReceiver::ptr socket)
{
    m_receivers[socket->port()] = socket;
}

void TransportLayer::add_transmitter(SocketTransmitter::ptr socket)
{
    m_transmitters.push_back(socket);
}

void TransportLayer::serve()
{
    serve_incoming();
    serve_outgoing();
}

void TransportLayer::serve_incoming()
{
    while (std::optional<NetworkLayer::Package> pkg = m_network->incoming())
    {
        uint64_t source_addr = pkg->source_addr;
        Buffer::ptr pkg_data = pkg->data;
        std::optional<std::pair<TransportLayer::TransportHeader0, Buffer::ptr>> p = decode_base_header(pkg_data->contents());
        if (!p)
            continue;

        const TransportLayer::TransportHeader0& header = p->first;
        Buffer::ptr data = p->second;

        auto it = m_receivers.find(header.target_port);
        if (it == m_receivers.end())
            continue;

        if (SocketReceiver::ptr s = it->second.lock())
        {
            s->receive(data, source_addr, header.header_byte_0);
        } else {
            m_receivers.erase(it);
            continue;
        }
    }
}

void TransportLayer::serve_outgoing()
{
    for (auto it = m_transmitters.begin(); it != m_transmitters.end();)
    {
        if (SocketTransmitter::ptr s = it->lock())
        {
            while (auto out = s->pick_outgoing())
            {
                TransportLayer::TransportHeader0 header;
                header.header_byte_0 = out->first;
                header.target_port = s->remote_port();
                SegmentBuffer& seg_buf = out->second;
                encode_base_header(seg_buf, header);
                m_network->send(seg_buf, s->remote_addr(), s->hop_limit());
            }
            ++it;
        } else {
            it = m_transmitters.erase(it);
        }
    }
}

std::optional<std::pair<TransportLayer::TransportHeader0, Buffer::ptr>> TransportLayer::decode_base_header(MemBlock mem)
{
    if (mem.size() < sizeof(uint8_t))
        return std::nullopt;

    TransportLayer::TransportHeader0 header;
    mem >> header.header_byte_0;
    uint8_t dst_port_bits = header.header_byte_0 & 0x03;
    switch(dst_port_bits)
    {
    case 0b01:
        // Default port == 1
        header.target_port = 1;
        return std::make_pair(header, Buffer::create(mem));
    case 0b10:
    {
        // Port number <= 255
        uint8_t port_number = 0;
        mem >> port_number;
        header.target_port = port_number;
        return std::make_pair(header, Buffer::create(mem));
    }
    case 0b11:
        // Port number > 255
        mem >> header.target_port;
        return std::make_pair(header, Buffer::create(mem));
    default:
        return std::nullopt;
    }
}

void TransportLayer::encode_base_header(SegmentBuffer& seg_buf, const TransportLayer::TransportHeader0& header)
{
    Buffer::ptr b = Buffer::create();
    uint8_t header_bits_0 = header.header_byte_0 & 0b11111100;

    if (header.target_port == 1)
    {
        header_bits_0 |= 0b01;
        b->raw() << header_bits_0;
    }
    else if (header.target_port <= 255)
    {
        header_bits_0 |= 0b10;
        uint8_t short_target_port = uint8_t(header.target_port);
        b->raw() << header_bits_0 << short_target_port;
    } else {
        header_bits_0 |= 0b11;
        b->raw() << header_bits_0 << header.target_port;
    }
    seg_buf.push_front(b);
}
