#pragma once

#include "ntdcp/network.hpp"

namespace ntdcp
{

/**
 * Zero header byte meaning
 *
 * | _7_ | _6_ | _5_ | _4_ | _3_ | _2_ | _1_ | _0_ |
 *                                     \ d port sz /
 *
 * 1,0: Destination port size
 *   - 0b00: target port is "1", no following port number
 *   - 0b01: target port is one following byte
 *   - 0b10: target port is two following bytes
 *   - 0b11: RESERVED
 */

class TransportLayer;

class SocketReceiver : public PtrAliases<SocketReceiver>
{
public:
    struct Incoming
    {
        uint64_t addr;
        Buffer::ptr data;
    };

    SocketReceiver(uint16_t my_port);

    uint16_t port() const;

    virtual void receive(Buffer::ptr data, uint64_t src_addr, uint8_t header_bits_0) = 0;
    virtual std::optional<Incoming> get_incoming() = 0;
    virtual bool has_incoming() = 0;

protected:
    uint16_t m_my_port;
};

class SocketTransmitter : public PtrAliases<SocketTransmitter>
{
public:
    /// @todo Rewrite this with queue

    SocketTransmitter(uint16_t target_port, uint64_t remote_addr, uint8_t hop_limit = 10);
    uint16_t remote_port() const;
    uint64_t remote_addr() const;
    uint8_t hop_limit() const;

    virtual bool busy() const = 0;
    virtual void send(Buffer::ptr buf) = 0;
    virtual std::optional<std::pair<uint8_t, SegmentBuffer>> pick_outgoing() = 0;

private:
    uint16_t m_remote_port;
    uint16_t m_remote_addr;
    uint8_t m_hop_limit;
};

class TransportLayer
{
public:
    TransportLayer(NetworkLayer::ptr network);

    void add_receiver(SocketReceiver::ptr socket);
    void add_transmitter(SocketTransmitter::ptr socket);
    void serve();

private:
    struct TransportHeader0
    {
        uint8_t header_byte_0 = 0;
        uint16_t target_port = 0;
    };

    static std::optional<std::pair<TransportHeader0, Buffer::ptr>> decode_base_header(MemBlock mem);
    static void encode_base_header(SegmentBuffer& seg_buf, const TransportHeader0& header);

    void serve_incoming();
    void serve_outgoing();

    std::map<uint16_t, SocketReceiver::wptr> m_receivers;
    std::list<SocketTransmitter::wptr> m_transmitters;
    NetworkLayer::ptr m_network;
};

}
