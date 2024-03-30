#include "ntdcp/network.hpp"

using namespace ntdcp;



NetworkLayer::NetworkLayer(SystemDriver::ptr sys, uint64_t addr) :
    m_sys(sys), m_addr(addr)
{

}

void NetworkLayer::add_physical(IPhysicalInterface::ptr phys)
{
    m_phys_devices.push_back(phys);
}

void NetworkLayer::send(Buffer::ptr data, uint64_t destination_addr, uint8_t hop_limit)
{
    send(SegmentBuffer(data), destination_addr, hop_limit);
}

void NetworkLayer::send(SegmentBuffer data, uint64_t destination_addr, uint8_t hop_limit)
{
    uint16_t package_id = random_id();
    if (address_acceptable(destination_addr))
    {
        Package p;
        p.source_addr = m_addr;
        p.data = data.merge();
        p.package_id = package_id;
        m_incoming.push(p);

        if (destination_addr == m_addr) // Package is directly for me
            return;
    }

    // Do not forget send to myself (if addr is good) and put id to m_packages_already_received
    PackageHeader package;
    package.source_addr = m_addr;
    package.destination_addr = destination_addr;
    package.package_id = package_id;
    package.hop_limit = hop_limit;

    m_packages_already_received.check_update(package.package_id);

    /// @todo Optimize using segment buffer to prevent copying

    encode(package, data);
    m_channel.encode(data);
    Buffer::ptr to_send_encoded = data.merge();

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

SystemDriver::ptr NetworkLayer::system_driver()
{
    return m_sys;
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

            const PackageHeader& header = pkg->first;

            if (m_packages_already_received.check_update(header.package_id))
                continue;

            if (address_acceptable(header.destination_addr))
            {
                Package p;
                p.source_addr = header.source_addr;
                p.data = pkg->second;
                p.package_id = header.package_id;
                m_incoming.push(p);
                continue;
            }

            retransmit(header, pkg->second, phys);
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

void NetworkLayer::retransmit(const PackageHeader& pkg, Buffer::ptr data, IPhysicalInterface::ptr came_from)
{
    if (pkg.hop_limit == 0)
        return;

    PackageHeader to_send = pkg;
    to_send.hop_limit -= 1;

    SegmentBuffer seg_buf(data);
    encode(to_send, seg_buf);
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

std::optional<std::pair<NetworkLayer::PackageHeader, Buffer::ptr>> NetworkLayer::decode(const MemBlock& mem_block)
{
    // TODO add here size checks
    MemBlock m(mem_block);
    if (mem_block.size() < sizeof(uint8_t))
        return std::nullopt;

    uint8_t flag_byte;
    m >> flag_byte;

    PackageHeader package;
    package.hop_limit = flag_byte >> 4;
    if (package.hop_limit == 0xF)
    {
        if (m.size() < sizeof(package.hop_limit))
            return std::nullopt;
        m >> package.hop_limit;
    }

    if (m.size() < sizeof(package.package_id))
        return std::nullopt;
    m >> package.package_id;

    uint8_t src_size_bits = flag_byte & 0b11;
    uint8_t dst_size_bits = (flag_byte >> 2) & 0b11;

    auto source_addr = read_addr_from_mem(m, src_size_bits);
    if (!source_addr)
        return std::nullopt;
    package.source_addr = *source_addr;

    auto dst_addr = read_addr_from_mem(m, dst_size_bits);
    if (!dst_addr)
        return std::nullopt;
    package.destination_addr = *dst_addr;

    return std::make_pair(package, Buffer::create(m.size(), m.begin()));
}

void NetworkLayer::encode(PackageHeader package, SegmentBuffer& buf)
{
    uint8_t flag_byte = 0;
    uint8_t src_addr_size_bits = get_addr_size_bits(package.source_addr);
    uint8_t dst_addr_size_bits = get_addr_size_bits(package.destination_addr);

    uint8_t hop_limit_bits = 0;
    if (package.hop_limit < 0xF)
    {
        hop_limit_bits = package.hop_limit;
    } else {
        hop_limit_bits = 0xF;
    }

    flag_byte |= src_addr_size_bits;
    flag_byte |= dst_addr_size_bits << 2;
    flag_byte |= hop_limit_bits << 4;

    Buffer::ptr header = Buffer::create();
    auto raw = header->raw();
    raw << flag_byte;
    raw << package.package_id;
    if (hop_limit_bits == 0xF)
        header->raw() << package.hop_limit;
    put_address_to_buffer(header, package.source_addr);
    put_address_to_buffer(header, package.destination_addr);


    buf.push_front(header);
}

uint8_t NetworkLayer::get_addr_size_bits(uint64_t addr)
{
    if (addr <= 0xFF)
    {
        return addr_size::bytes_1;
    } else if (addr <= 0xFFFF)
    {
        return addr_size::bytes_2;
    } else if (addr <= 0xFFFFFF)
    {
        return addr_size::bytes_3;
    } else {
        return addr_size::bytes_4;
    }
}

void NetworkLayer::put_address_to_buffer(Buffer::ptr buf, uint64_t addr)
{
    if (addr <= 0xFF)
    {
        uint8_t x = uint8_t(addr);
        buf->raw() << x;
    } else if (addr <= 0xFFFF)
    {
        uint8_t x1 = addr & 0xFF;
        addr >>= 8;
        uint8_t x2 = addr & 0xFF;
        buf->raw() << x2 << x1;
    } else if (addr <= 0xFFFFFF)
    {
        uint8_t x1 = addr & 0xFF;
        addr >>= 8;
        uint8_t x2 = addr & 0xFF;
        addr >>= 8;
        uint8_t x3 = addr & 0xFF;
        buf->raw() << x3 << x2 << x1;
    } else {
        uint8_t x1 = addr & 0xFF;
        addr >>= 8;
        uint8_t x2 = addr & 0xFF;
        addr >>= 8;
        uint8_t x3 = addr & 0xFF;
        addr >>= 8;
        uint8_t x4 = addr & 0xFF;
        buf->raw() << x4 << x3 << x2 << x1;
    }
}

std::optional<uint64_t> NetworkLayer::read_addr_from_mem(MemBlock& data, uint8_t address_size_bits)
{
    uint64_t result = 0;
    uint8_t x = 0;
    switch(address_size_bits)
    {
    case addr_size::bytes_1:
        if (data.size() < 1)
            return std::nullopt;
        uint8_t x;
        data >> x;
        result = x;
        break;

    case addr_size::bytes_2:
        if (data.size() < 2)
            return std::nullopt;
        data >> x;
        result = x << 8;
        data >> x;
        result += x;
        break;

    case addr_size::bytes_3:
        if (data.size() < 3)
            return std::nullopt;
        data >> x;
        result = x << 16;
        data >> x;
        result += x << 8;
        data >> x;
        result += x;
        break;

    case addr_size::bytes_4:
        if (data.size() < 4)
            return std::nullopt;
        data >> x;
        result = x << 24;
        data >> x;
        result += x << 16;
        data >> x;
        result += x << 8;
        data >> x;
        result += x;
        break;
    }
    return result;
}
