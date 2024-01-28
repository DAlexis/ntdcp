#include "ntdcp/socket-datagram.hpp"

using namespace ntdcp;

SocketReceiverDatagram::SocketReceiverDatagram(TransportLayer::ptr transport_layer, uint16_t my_port) :
    SocketReceiver(my_port), m_incoming_queue(*transport_layer->network().system_driver()), m_transport(transport_layer)
{
    m_transport->add_receiver(*this);
}

SocketReceiverDatagram::~SocketReceiverDatagram()
{
    m_transport->remove_receiver(*this);
}

void SocketReceiverDatagram::receive(Buffer::ptr data, uint64_t src_addr, uint16_t package_id, uint8_t header_bits_0)
{
    if (m_incoming_queue.full())
        return; // Only drop is possible

    Incoming inc;
    inc.data = data;
    inc.addr = src_addr;
    m_incoming_queue.push(inc);
}

std::optional<SocketReceiverDatagram::Incoming> SocketReceiverDatagram::get_incoming()
{
    return m_incoming_queue.pop();
}

bool SocketReceiverDatagram::has_incoming() const
{
    return !m_incoming_queue.empty();
}


SocketTransmitterDatagram::SocketTransmitterDatagram(TransportLayer::ptr transport_layer, uint16_t target_port, uint64_t remote_addr, uint8_t hop_limit) :
    SocketTransmitter(target_port, remote_addr, hop_limit), m_outgoing_queue(*transport_layer->network().system_driver()), m_transport(transport_layer)
{
    m_transport->add_transmitter(*this);
}

SocketTransmitterDatagram::~SocketTransmitterDatagram()
{
    m_transport->remove_transmitter(*this);
}

bool SocketTransmitterDatagram::busy() const
{
    return !m_outgoing_queue.full();
}

bool SocketTransmitterDatagram::send(Buffer::ptr buf)
{
    return m_outgoing_queue.push(std::make_pair(0, SegmentBuffer(buf)));
}

std::optional<std::pair<uint8_t, SegmentBuffer>> SocketTransmitterDatagram::pick_outgoing()
{
    return m_outgoing_queue.pop();
}
