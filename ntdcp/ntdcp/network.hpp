#pragma once

#include "ntdcp/channel.hpp"
#include "ntdcp/system-driver.hpp"
#include "ntdcp/caching-set.hpp"

#include <map>
#include <queue>
#include <optional>

namespace ntdcp
{

class NetworkLayer
{
public:
    struct Package
    {
        uint64_t source_addr;
        Buffer::ptr data;
    };

    NetworkLayer(ISystemDriver::ptr sys, uint64_t addr);
    void add_physical(IPhysicalInterface::ptr phys);

    void send(Buffer::ptr data, uint64_t destination_addr, uint8_t hop_limit = 10);
    std::optional<Package> incoming();

    void serve();

private:
    struct PackageDecoded
    {
        uint64_t source_addr;
        uint64_t destination_addr;
        uint16_t package_id;
        uint8_t hop_limit = 255;

        Buffer::ptr data;
    };

    void serve_incoming();
    void serve_outgoing();
    void retransmit(const PackageDecoded& pkg, IPhysicalInterface::ptr came_from);
    bool address_acceptable(uint64_t addr);

    uint16_t random_id();

    static std::optional<PackageDecoded> decode(const MemBlock& data);
    static SegmentBuffer encode(PackageDecoded package);

    ChannelLayer m_channel;
    ISystemDriver::ptr m_sys;
    uint64_t m_addr;

    std::queue<Package> m_incoming;
    std::map<IPhysicalInterface::ptr, std::queue<Buffer::ptr>> m_outgoing;
    std::list<IPhysicalInterface::ptr> m_phys_devices;
    CachingSet<uint16_t> m_packages_already_received{100};
};

}
