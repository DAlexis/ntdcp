#include "ntdcp/node.hpp"
#include "ntdcp/package.hpp"

using namespace ntdcp;

Socket::Socket(Node& node, SoketOptions options) :
    m_node(node), m_options(options)
{
    m_node.add_socket(*this);
}

Socket::~Socket()
{
    m_node.remove_socket(*this);
}

const SoketOptions& Socket::options() const
{
    return m_options;
}

Node::Node(ISystemDriver::ptr sys, uint64_t addr) :
    m_sys(sys), m_addr(addr)
{
}

void Node::add_physical(IPhysicalInterface::ptr phys)
{
    m_phys_devices.push_back(phys);
}

void Node::serve()
{
    serve_incoming();
    serve_outgoung();
}


void Node::add_socket(Socket& sender)
{
    m_sockets.emplace(sender.options().port, &sender);
}

void Node::remove_socket(Socket& sender)
{
    m_sockets.erase(sender.options().port);
}

void Node::serve_incoming()
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
            Package pkg;
            parse_package(pkg, frame->contents());
            process_package(pkg);
        }

    }
}

void Node::serve_outgoung()
{
    for (auto it = m_sockets.begin(); it != m_sockets.end(); ++it)
    {
        Buffer::ptr data = it->second->outgoing();
        if (!data)
            continue;

        const SoketOptions& opts = it->second->options();

        Package p;
        p.source_addr = m_addr;
        p.destination_addr = opts.destination_addr;
        p.source_port = opts.port;
        p.destination_port = opts.destination_port;
        p.hop_limit = opts.hop_limit;
        p.transport_type = opts.type;

        p.package_id = random_id();

        auto ack_it = m_acks.find(p.destination_addr);
        if (ack_it != m_acks.end())
        {
            AckTask t = ack_it->second.front();
            ack_it->second.pop_front();
            p.acknoledgement_for_id = p.package_id;
        }

    }
}

void Node::process_package(const Package& pkg)
{
    auto it = m_sockets.find(pkg.destination_port);
    if (it == m_sockets.end())
        return;

    Socket* sock = it->second;
    bool received = sock->put_in(Buffer::create(pkg.data));
    if (received && pkg.transport_type != TransportType::no_acknoledgement)
    {
        AckTask ack;
        // ack.addr = pkg.source_addr;
        // ack.port = pkg.source_port;
        ack.package_id = pkg.package_id;
        // ack.session_id = pkg.session_id;
        m_acks[pkg.source_addr].push_back(ack);
        // todo put to acks;
    }

}

uint16_t Node::random_id()
{
    for (;;)
    {
        uint16_t rnd = m_sys->random();
        if (rnd != 0)
            return rnd;
    }
}
