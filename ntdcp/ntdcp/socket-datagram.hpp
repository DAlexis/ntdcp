#pragma once

#include "ntdcp/transport.hpp"
#include "ntdcp/synchronization.hpp"

namespace ntdcp
{

template<class MutexType = std::mutex>
class SocketReceiverDatagram : public SocketReceiver
{
public:
    struct Incoming
    {
        uint64_t addr;
        Buffer::ptr data;
    };

    /// @todo Rewrite this with queue
    SocketReceiverDatagram(uint16_t my_port = 1) :
        SocketReceiver(my_port)
    {
    }

    void receive(Buffer::ptr data, uint64_t src_addr, uint8_t header_bits_0) override
    {
        if (m_incoming_queue.full())
            return; // Only drop is possible

        Incoming inc;
        inc.data = data;
        inc.addr = src_addr;
        m_incoming_queue.push(inc);
    }

    std::optional<Incoming> get_incoming()
    {
        return m_incoming_queue.pop();
    }

    bool has_incoming()
    {
        return !m_incoming_queue.empty();
    }


private:

    QueueLocking<Incoming, MutexType> m_incoming_queue;
};

template<class MutexType = std::mutex>
class SocketTransmitterDatagram : public SocketTransmitter
{
public:
    SocketTransmitterDatagram(uint16_t target_port, uint64_t remote_addr, uint8_t hop_limit = 10) :
        SocketTransmitter(target_port, remote_addr, hop_limit)
    {
    }

    bool busy() const
    {
        return !m_outgoing_queue.full();
    }

    bool send(Buffer::ptr buf)
    {
        return m_outgoing_queue.push(std::make_pair(0, SegmentBuffer(buf)));
    }

    std::optional<std::pair<uint8_t, SegmentBuffer>> pick_outgoing() override
    {
        return m_outgoing_queue.pop();
    }

private:
    QueueLocking<std::pair<uint8_t, SegmentBuffer>, MutexType> m_outgoing_queue;
};

}
