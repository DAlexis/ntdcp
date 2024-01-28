#pragma once

#include "ntdcp/transport.hpp"
#include "ntdcp/synchronization.hpp"

namespace ntdcp
{

class SocketReceiverDatagram : public SocketReceiver
{
public:
    struct Incoming
    {
        uint64_t addr;
        Buffer::ptr data;
    };

    SocketReceiverDatagram(TransportLayer::ptr transport_layer, uint16_t my_port = 1);
    ~SocketReceiverDatagram() override;

    void receive(Buffer::ptr data, uint64_t src_addr, uint16_t package_id, uint8_t header_bits_0) override;
    std::optional<Incoming> get_incoming();
    bool has_incoming() const;

private:

    QueueLocking<Incoming> m_incoming_queue;
    TransportLayer::ptr m_transport;
};

class SocketTransmitterDatagram : public SocketTransmitter
{
public:
    SocketTransmitterDatagram(TransportLayer::ptr transport_layer, uint16_t target_port, uint64_t remote_addr, uint8_t hop_limit = 10);
    ~SocketTransmitterDatagram() override;

    bool busy() const;
    bool send(Buffer::ptr buf);
    std::optional<std::pair<uint8_t, SegmentBuffer>> pick_outgoing() override;

private:
    QueueLocking<std::pair<uint8_t, SegmentBuffer>> m_outgoing_queue;
    TransportLayer::ptr m_transport;
};

}
