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

void TransportLayer::add_receiver(SocketReceiver& socket)
{
    m_receivers[socket.port()] = &socket;
}

void TransportLayer::add_transmitter(SocketTransmitter& socket)
{
    m_transmitters.insert(&socket);
}

void TransportLayer::remove_receiver(SocketReceiver& socket)
{
    m_receivers.erase(socket.port());
}

void TransportLayer::remove_transmitter(SocketTransmitter& socket)
{
    m_transmitters.erase(&socket);
}

void TransportLayer::serve()
{
    serve_incoming();
    serve_outgoing();
}

NetworkLayer& TransportLayer::network()
{
    return *m_network;
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

        SocketReceiver& s = *it->second;
        s.receive(data, source_addr, pkg->package_id, header.header_byte_0);

    }
}

void TransportLayer::serve_outgoing()
{
    for (auto it = m_transmitters.begin(); it != m_transmitters.end();)
    {
        SocketTransmitter& s = **it;

        while (auto out = s.pick_outgoing())
        {
            TransportLayer::TransportHeader0 header;
            header.header_byte_0 = out->first;
            header.target_port = s.remote_port();
            SegmentBuffer& seg_buf = out->second;
            encode_base_header(seg_buf, header);
            m_network->send(seg_buf, s.remote_addr(), s.hop_limit());
        }
        ++it;

    }
}

std::optional<uint16_t> TransportLayer::decode_port(MemBlock& mem, uint8_t port_size_bits)
{
    uint16_t port;
    switch(port_size_bits)
    {
    case 0b01: // Default port == 1
        port = 1;
        break;
    case 0b10: // Port number <= 255
    {
        if (mem.size() < sizeof(uint8_t))
            return std::nullopt;

        uint8_t port_number = 0;
        mem >> port_number;
        port = port_number;
        break;
    }
    case 0b11: // Port number > 255
        if (mem.size() < sizeof(uint16_t))
            return std::nullopt;

        mem >> port;
        break;
    default:
        return std::nullopt;
    }
    return port;
}


std::optional<std::pair<TransportLayer::TransportHeader0, Buffer::ptr>> TransportLayer::decode_base_header(MemBlock mem)
{
    if (mem.size() < sizeof(uint8_t))
        return std::nullopt;

    TransportLayer::TransportHeader0 header;
    mem >> header.header_byte_0;

    uint8_t dst_port_bits = header.header_byte_0 & 0x03;
    auto port = decode_port(mem, dst_port_bits);
    if (!port)
        return std::nullopt;

    header.target_port = *port;
    return std::make_pair(header, Buffer::create(mem));
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
