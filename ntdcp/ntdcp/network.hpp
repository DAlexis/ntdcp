#pragma once

#include "ntdcp/channel.hpp"
#include "ntdcp/system-driver.hpp"
#include "ntdcp/caching-set.hpp"

#include <map>
#include <queue>
#include <optional>

namespace ntdcp
{


/**
 * Zero header byte meaning
 *
 * | _7_ | _6_ | _5_ | _4_ | _3_ | _2_ | _1_ | _0_ |
 * \      hop limit        \ dst_size  \  src size /
 *
 * 1,0: Source address size
 *   - 0b00: 1 byte
 *   - 0b01: 2 bytes
 *   - 0b10: 3 bytes
 *   - 0b11: 4 bytes
 *
 * 3,2: Destination address size
 *   - 0b00: 1 byte
 *   - 0b01: 2 bytes
 *   - 0b10: 3 bytes
 *   - 0b11: 4 bytes
 *
 * 7,6,5,4: hop limit
 *   - if from 0b000 to 0b110 it is a value of hop limit
 *   - if 0b111 then next byte after adresses is hop limit
 */

class NetworkLayer : public PtrAliases<NetworkLayer>
{
public:
    struct Package
    {
        uint64_t source_addr;
        uint16_t package_id;
        Buffer::ptr data;
    };

    NetworkLayer(ISystemDriver::ptr sys, uint64_t addr);
    void add_physical(IPhysicalInterface::ptr phys);

    void send(Buffer::ptr data, uint64_t destination_addr, uint8_t hop_limit = 10);
    void send(SegmentBuffer data, uint64_t destination_addr, uint8_t hop_limit = 10);
    std::optional<Package> incoming();

    void serve();

    ISystemDriver::ptr system_driver();

private:
    struct PackageHeader
    {
        uint64_t source_addr;
        uint64_t destination_addr;
        uint16_t package_id;
        uint8_t hop_limit = 255;
    };

    void serve_incoming();
    void serve_outgoing();
    void retransmit(const PackageHeader& pkg, Buffer::ptr data, IPhysicalInterface::ptr came_from);
    bool address_acceptable(uint64_t addr);

    uint16_t random_id();

    struct addr_size
    {
        constexpr static uint8_t bytes_1 = 0b00;
        constexpr static uint8_t bytes_2 = 0b01;
        constexpr static uint8_t bytes_3 = 0b10;
        constexpr static uint8_t bytes_4 = 0b11;
    };

    static std::optional<std::pair<PackageHeader, Buffer::ptr>> decode(const MemBlock& data);
    static void encode(PackageHeader package, SegmentBuffer& buf);

    static uint8_t get_addr_size_bits(uint64_t addr);
    static void put_address_to_buffer(Buffer::ptr buf, uint64_t addr);
    static std::optional<uint64_t> read_addr_from_mem(MemBlock& data, uint8_t address_size_bits);

    ChannelLayer m_channel;
    ISystemDriver::ptr m_sys;
    uint64_t m_addr;

    std::queue<Package> m_incoming;
    std::map<IPhysicalInterface::ptr, std::queue<Buffer::ptr>> m_outgoing;
    std::list<IPhysicalInterface::ptr> m_phys_devices;
    CachingSet<uint16_t> m_packages_already_received{100};
};

}
