#pragma once

#include "ntdcp/transport.hpp"
#include "ntdcp/synchronization.hpp"

namespace ntdcp
{

class SocketStableBase : public SocketReceiver, public SocketTransmitter
{
public:
    struct StableHeader
    {
        uint16_t source_port = 0;
        uint16_t message_id = 0;
        uint16_t ack_for_message_id = 0;
    };
};

template<class MutexType = std::mutex>
class SocketStable : public SocketStableBase
{
public:
    SocketStable(uint16_t my_port, uint16_t target_port, uint64_t remote_addr, uint8_t hop_limit = 10);

    void receive(Buffer::ptr data, uint64_t src_addr, uint8_t header_bits_0) override
    {

    }

    std::optional<std::pair<uint8_t, SegmentBuffer>> pick_outgoing() override
    {
    }

    void connect();
    void send(Buffer::ptr buf);

private:
    uint16_t m_session_id;
    bool m_is_connected = false;
    bool m_waiting_ack = false;

    std::optional<std::pair<uint8_t, SegmentBuffer>> m_outgoing;
};

}
