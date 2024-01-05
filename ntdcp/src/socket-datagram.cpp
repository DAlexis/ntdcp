#include "ntdcp/socket-datagram.hpp"

using namespace ntdcp;

SocketReceiverDatagram::SocketReceiverDatagram(uint16_t my_port) :
    SocketReceiver(my_port)
{
}

void SocketReceiverDatagram::receive(Buffer::ptr data, uint64_t src_addr, uint8_t header_bits_0)
{
    Incoming inc;
    inc.data = data;
    inc.addr = src_addr;
    m_incoming = inc;
}

std::optional<SocketReceiver::Incoming> SocketReceiverDatagram::get_incoming()
{
    if (!m_incoming)
        return std::nullopt;

    std::optional<SocketReceiver::Incoming> result = m_incoming;
    m_incoming.reset();
    return result;
}

bool SocketReceiverDatagram::has_incoming()
{
    return m_incoming != std::nullopt;
}

SocketTransmitterDatagram::SocketTransmitterDatagram(uint16_t target_port, uint64_t remote_addr, uint8_t hop_limit) :
    SocketTransmitter(target_port, remote_addr, hop_limit)
{
}

bool SocketTransmitterDatagram::busy() const
{
    return !m_outgoing_buffer.empty();
}

void SocketTransmitterDatagram::send(Buffer::ptr buf)
{
    m_outgoing_buffer.clear();
    m_outgoing_buffer.push_back(buf);
}

std::optional<std::pair<uint8_t, SegmentBuffer>> SocketTransmitterDatagram::pick_outgoing()
{
    if (m_outgoing_buffer.empty())
    {
        return std::nullopt;
    } else {
        SegmentBuffer result = m_outgoing_buffer;
        m_outgoing_buffer.clear();
        return std::make_pair(0, result);
    }
}
