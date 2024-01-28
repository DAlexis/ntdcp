#pragma once

#include "ntdcp/network.hpp"

#include <set>

namespace ntdcp
{

/**
 * Zero header byte meaning
 *
 * | _7_ | _6_ | _5_ | _4_ | _3_ | _2_ | _1_ | _0_ |
 *                         \ s port sz \ d port sz /
 *
 * 1,0: Destination port size
 *   - 0b01: target port is "1", no following port number
 *   - 0b10: target port is one following byte
 *   - 0b11: target port is two following bytes
 *   - 0b00: RESERVED
 *
 * 1,0: Source port size
 *   - 0b01: source port is "1", no following port number
 *   - 0b10: source port is one following byte
 *   - 0b11: source port is two following bytes
 *   - 0b00: RESERVED
 */

class TransportLayer;

class SocketReceiver : public PtrAliases<SocketReceiver>
{
public:
    SocketReceiver(uint16_t my_port);
    virtual ~SocketReceiver() = default;

    uint16_t port() const;

    virtual void receive(Buffer::ptr data, uint64_t src_addr, uint16_t package_id, uint8_t header_bits_0) = 0;

protected:
    uint16_t m_my_port;
};

class SocketTransmitter : public PtrAliases<SocketTransmitter>
{
public:
    /// @todo Rewrite this with queue

    SocketTransmitter(uint16_t target_port, uint64_t remote_addr, uint8_t hop_limit = 10);
    virtual ~SocketTransmitter() = default;
    uint16_t remote_port() const;
    uint64_t remote_addr() const;
    uint8_t hop_limit() const;

    virtual std::optional<std::pair<uint8_t, SegmentBuffer>> pick_outgoing() = 0;

protected:
    uint16_t m_remote_port;
    uint16_t m_remote_addr;
    uint8_t m_hop_limit;
};

class TransportLayer : public PtrAliases<TransportLayer>
{
public:
    TransportLayer(NetworkLayer::ptr network);

    void add_receiver(SocketReceiver& socket);
    void add_transmitter(SocketTransmitter& socket);

    void remove_receiver(SocketReceiver& socket);
    void remove_transmitter(SocketTransmitter& socket);

    void serve();

    NetworkLayer& network();

    static std::optional<uint16_t> decode_port(MemBlock& mem, uint8_t port_size_bits);

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

    std::map<uint16_t, SocketReceiver*> m_receivers;
    std::set<SocketTransmitter*> m_transmitters;
    NetworkLayer::ptr m_network;
};

}
