#pragma once

#include "ntdcp/transport.hpp"

namespace ntdcp
{

class SocketReceiverDatagram : public SocketReceiver
{
public:
    /// @todo Rewrite this with queue
    SocketReceiverDatagram(uint16_t my_port = 1);

    void receive(Buffer::ptr data, uint64_t src_addr, uint8_t header_bits_0) override;
    std::optional<Incoming> get_incoming() override;
    bool has_incoming() override;

private:
    std::optional<Incoming> m_incoming;
};

class SocketTransmitterDatagram : public SocketTransmitter
{
public:
    SocketTransmitterDatagram(uint16_t target_port, uint64_t remote_addr, uint8_t hop_limit = 10);
    bool busy() const override;
    void send(Buffer::ptr buf) override;
    std::optional<std::pair<uint8_t, SegmentBuffer>> pick_outgoing() override;

private:
    SegmentBuffer m_outgoing_buffer;
};

}
