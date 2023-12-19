#include "ntdcp/network.hpp"

using namespace ntdcp;



NetworkLayer::NetworkLayer(ISystemDriver::ptr sys, uint64_t addr) :
    m_sys(sys), m_addr(addr)
{

}

void NetworkLayer::add_physical(IPhysicalInterface::ptr phys)
{
    m_phys_devices.push_back(phys);
}


void NetworkLayer::send(Buffer::ptr data, uint64_t destination_addr, uint8_t hop_limit)
{
    if (address_acceptable(destination_addr))
    {
        Package p;
        p.source_addr = m_addr;
        p.data = data;
        m_incoming.push(p);

        if (destination_addr == m_addr) // Package is directly for me
            return;
    }

    // Do not forget send to myself (if addr is good) and put id to m_packages_already_received
    PackageDecoded package;
    package.source_addr = m_addr;
    package.destination_addr = destination_addr;
    package.package_id = random_id();
    package.hop_limit = hop_limit;
    package.data = data;

    m_packages_already_received.check_update(package.package_id);

    /// @todo Optimize using segment buffer to prevent copying
    SegmentBuffer seg_buf = encode(package);
    m_channel.encode(seg_buf);
    Buffer::ptr to_send_encoded = seg_buf.merge();

    for (const auto& dev : m_phys_devices)
    {
        m_outgoing[dev].push(to_send_encoded);
    }
}

void NetworkLayer::serve()
{
    serve_incoming();
    serve_outgoing();
}

void NetworkLayer::serve_incoming()
{
    for (auto& phys : m_phys_devices)
    {
        SerialReadAccessor& inc = phys->incoming();
        if (inc.empty())
            continue;

        std::vector<Buffer::ptr> frames = m_channel.decode(inc);
        if (frames.empty())
            continue;

        for (const auto& frame : frames)
        {
            auto pkg = decode(frame->contents());
            if (!pkg)
                continue;

            if (m_packages_already_received.check_update(pkg->package_id))
                continue;

            if (address_acceptable(pkg->destination_addr))
            {
                Package p;
                p.source_addr = pkg->source_addr;
                p.data = pkg->data;
                m_incoming.push(p);
                continue;
            }

            retransmit(*pkg, phys);
        }
    }
}

void NetworkLayer::serve_outgoing()
{
    // Sending data to physical devices
    for (auto it = m_outgoing.begin(); it != m_outgoing.end(); ++it)
    {
        IPhysicalInterface::ptr dev = it->first;
        std::queue<Buffer::ptr>& queue = it->second;

        while (!queue.empty() && !dev->busy())
        {
            dev->send(queue.front());
            queue.pop();
        }
    }
}

void NetworkLayer::retransmit(const PackageDecoded& pkg, IPhysicalInterface::ptr came_from)
{
    if (pkg.hop_limit == 0)
        return;

    PackageDecoded to_send = pkg;
    to_send.hop_limit -= 1;

    /// @todo Optimize using segment buffer to prevent copying
    SegmentBuffer seg_buf(encode(to_send));
    m_channel.encode(seg_buf);
    Buffer::ptr to_send_encoded = seg_buf.merge();

    for (auto& dev : m_phys_devices)
    {
        if (dev == came_from && !came_from->options().retransmit_back)
            continue;

        m_outgoing[dev].push(to_send_encoded);
    }
}

bool NetworkLayer::address_acceptable(uint64_t addr)
{
    if (addr == m_addr)
        return true;
    if (addr == 0xFF)
        return true;
    return false;
}


std::optional<NetworkLayer::Package> NetworkLayer::incoming()
{
    if (m_incoming.empty())
        return std::nullopt;

    NetworkLayer::Package result = m_incoming.front();
    m_incoming.pop();
    return result;
}

uint16_t NetworkLayer::random_id()
{
    for (;;)
    {
        uint16_t rnd = m_sys->random();
        if (rnd != 0)
            return rnd;
    }
}

std::optional<NetworkLayer::PackageDecoded> NetworkLayer::decode(const MemBlock& mem_block)
{
    /// Tempolral trivial implementation
    if (mem_block.size() < sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint8_t))
    {
        return std::nullopt;
    }

    MemBlock mem = mem_block;
    PackageDecoded result;
    mem >> result.source_addr >> result.destination_addr >> result.package_id >> result.hop_limit;
    result.data = Buffer::create(mem.size(), mem.begin());
    return result;
}

SegmentBuffer NetworkLayer::encode(PackageDecoded package)
{
    /// Tempolral trivial implementation
    Buffer::ptr header = Buffer::create();
    header->raw() << package.source_addr << package.destination_addr << package.package_id << package.hop_limit;
    SegmentBuffer result(header);
    result.push_back(package.data);
    return result;
}
