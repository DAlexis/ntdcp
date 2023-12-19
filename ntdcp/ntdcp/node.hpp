#pragma once

#include "ntdcp/package.hpp"
#include "ntdcp/system-driver.hpp"
#include "ntdcp/channel.hpp"
#include "ntdcp/utils.hpp"

#include <list>
#include <map>
#include <functional>
#include <cstdint>
#include <cstdlib>

namespace ntdcp
{

class Node;
class Package;

struct NodeConfig
{
    uint64_t addr;

};

struct SoketOptions
{
    uint16_t port = 0;

    uint64_t destination_addr = 0;
    uint16_t destination_port = 0;

    TransportType type = TransportType::no_acknoledgement;

    uint8_t hop_limit = 10;
};


class Socket
{
    friend class Node;
public:
    using ReceiveCallback = std::function<void(uint64_t from, Buffer::ptr)>;

    Socket(Node& node, SoketOptions options);
    virtual ~Socket();

    const SoketOptions& options() const;

    bool busy();
    bool put_out(Buffer::ptr data);
    Buffer::ptr imcoming();
    void clear_incoming();

private:
    Buffer::ptr outgoing();
    void clear_outgoing();

    bool put_in(Buffer::ptr data);


protected:

    Node& m_node;
    Buffer::ptr m_out;
    Buffer::ptr m_in;
    SoketOptions m_options;
};


// Но RPC, когда ответ приходит асинхронно, по сути двухсторонний RPC, в одну сторону отправляется запрос на call, а в другую - на return
// Но это же то же самое, что сокет, но без очереди

//template<typename RetType, typename ArgType>
//...


class Node
{
public:
    Node(ISystemDriver::ptr sys, uint64_t addr);
    void add_physical(IPhysicalInterface::ptr phys);

    void serve();


    void add_socket(Socket& sender);

    void remove_socket(Socket& sender);

private:
    struct AckTask
    {
        uint16_t package_id = 0;
    };

    struct SendTask
    {
        IPhysicalInterface::ptr phys;
        Package p;
    };

    void serve_incoming();
    void serve_outgoung();

    void process_package(const Package& pkg);

    uint16_t random_id();

    ISystemDriver::ptr m_sys;
    uint64_t m_addr;
    std::list<IPhysicalInterface::ptr> m_phys_devices;
    ChannelLayer m_channel;

    std::map<uint16_t, Socket*> m_sockets; // senders by port

    std::map<uint64_t, std::list<AckTask>> m_acks;
    std::list<SendTask> m_send_tasks;
};


}

