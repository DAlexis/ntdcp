#pragma once

#include "ntdcp/network.hpp"
#include "ntdcp/synchronization.hpp"

#include <functional>
#include <set>

namespace ntdcp {

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

struct ConnectionId
{
    ConnectionId(uint16_t destination_port = 0, uint64_t source_addr = 0, uint16_t source_port = 0);
    uint64_t source_addr = 0;
    uint16_t source_port = 0;
    uint16_t destination_port = 0;

    bool operator<(const ConnectionId& right) const;
};

struct TransportDescription
{
    enum class Type {
        broadcast,
        connection_request,
        connection_submit,
        data_transfer,
        connection_close,
        connection_close_submit
    };

    uint64_t source_addr = 0;
    uint16_t source_port = 0;

    uint64_t destination_addr = 0;
    uint16_t destination_port = 0;

    Type type = Type::data_transfer;

    uint16_t message_id = 0;
    uint16_t ack_for_message_id = 0;

    uint8_t repeat = 1;

    bool has_ack = false;
};

class Receiver
{
public:
    Receiver(uint16_t local_port);

    virtual void receive(Buffer::ptr data, const TransportDescription& header) = 0;

    uint16_t local_port();

protected:
    uint16_t m_local_port;
};

class Transmitter
{
public:
    Transmitter(uint64_t remote_address, uint16_t remote_port);

    uint64_t remote_address();
    uint16_t remote_port();

    virtual std::optional<std::pair<TransportDescription, SegmentBuffer>> pick_outgoing() = 0;

protected:
    uint64_t m_remote_address;
    uint16_t m_remote_port;
};

struct RetransmissionOptions
{
    enum class Policy {
        drop_when_timeout,
        break_when_timeout
    };

    Policy policy = Policy::break_when_timeout;

    std::chrono::milliseconds timeout{10000};
    std::chrono::milliseconds restransmission_time{1000};

    std::chrono::milliseconds force_ack_after{200};
};

class Socket : public Receiver, public Transmitter
{
public:
    enum class State {
        not_connected,
        waiting_for_submit,
        connected,
        connection_timeout,
        closed
    };

    Socket(TransportLayer& transport_layer, uint64_t remote_address, uint16_t local_port, uint16_t remote_port,
           const RetransmissionOptions& opts = RetransmissionOptions());
    ~Socket();

    bool busy();
    bool connect();
    bool ready_to_send();
    void send_connection_submit(uint16_t message_id);
    bool send(Buffer::ptr data);
    bool has_data();
    std::optional<Buffer::ptr> get_received();

    void close();

    State state();

    uint16_t unconfirmed_to_remote();
    uint16_t missed_from_remote();

    void receive(Buffer::ptr data, const TransportDescription& header) override;
    std::optional<std::pair<TransportDescription, SegmentBuffer>> pick_outgoing() override;

private:
    struct AckTask
    {
        uint16_t message_id = 0;
        bool was_sent_at_least_once = false;
        std::chrono::steady_clock::time_point time_seg_received;
        bool force_send_immediately = false;
    };

    struct SendTask
    {
        TransportDescription description;
        Buffer::ptr buf;
        int sent_count = 0;
        std::chrono::steady_clock::time_point created;
        std::chrono::steady_clock::time_point last_pick;
        std::chrono::milliseconds timeout; // TODO Add timeout support
    };

    void prepare_ack(uint16_t message_id);
    void create_send_task(uint16_t ack_for_message_id, TransportDescription::Type type, Buffer::ptr buf);
    std::optional<std::pair<TransportDescription, SegmentBuffer>> pick_force_ack();
    void drop_if_timeout(std::chrono::steady_clock::time_point now);

    TransportLayer& m_transport_layer;
    RetransmissionOptions m_options;

    std::optional<AckTask> m_ack_task;
    std::optional<SendTask> m_send_task;
    QueueLocking<Buffer::ptr> m_incoming;
    uint16_t m_last_received_message_id = 0;
    uint16_t m_last_outgoing_message_id = 0;
    uint16_t m_unconfirmed_to_remote = 0;
    uint16_t m_missed_from_remote = 0;
    State m_state = State::not_connected;
};

class BroadcastReceiver : public Receiver
{
public:
    struct Sender
    {
        uint32_t address;
    };

    BroadcastReceiver(TransportLayer& transport_layer, uint16_t local_port);
    ~BroadcastReceiver();

    void receive(Buffer::ptr data, const TransportDescription& header) override;
    bool has_data();
    std::optional<std::pair<Sender, Buffer::ptr>> get_received();

private:
    TransportLayer& m_transport_layer;
    QueueLocking<std::pair<Sender, Buffer::ptr>> m_incoming;
};

class BroadcastTransmitter : public Transmitter
{
public:
    bool busy();
    bool send(Buffer::ptr data);
    std::optional<std::pair<TransportDescription, SegmentBuffer>> pick_outgoing() override;
};

class Acceptor : public Receiver
{
public:
    using OnNewConnectionCallback = std::function<void(std::shared_ptr<Socket>)>;

    Acceptor(TransportLayer& transport_layer, uint16_t listening_port, OnNewConnectionCallback on_new_connection,
             const RetransmissionOptions& opts = RetransmissionOptions());
    ~Acceptor();

    void receive(Buffer::ptr data, const TransportDescription& header) override;

private:
    TransportLayer& m_transport_layer;
    RetransmissionOptions m_options;

    OnNewConnectionCallback m_on_new_connection;
    CachingMap<uint32_t, std::weak_ptr<Socket>> m_already_created_sockets{10};
};

class TransportLayer : public PtrAliases<TransportLayer>
{
public:
    TransportLayer(NetworkLayer::ptr network);
    void add_socket(Socket& socket);
    void remove_socket(Socket& socket);

    void add_acceptor(Acceptor& acceptor);
    void remove_acceptor(Acceptor& acceptor);

    void add_broadcast_receiver(BroadcastReceiver& receiver);
    void remove_broadcast_receiver(BroadcastReceiver& receiver);

    void serve();

    SystemDriver::ptr system_driver();

    static std::optional<std::pair<TransportDescription, Buffer::ptr>> decode(MemBlock mem);
    static void encode(SegmentBuffer& seg_buf, const TransportDescription& header);

private:
    void serve_incoming();
    void serve_outgoing();

    Receiver* find_acceptor(uint16_t port);
    Receiver* find_socket_for_data(uint64_t source_addr, uint16_t source_port, uint16_t dst_port);
    Receiver* find_socket_for_close_submit(uint64_t source_addr, uint16_t source_port, uint16_t dst_port);
    Receiver* find_socket_for_submit(uint64_t source_addr, uint16_t dst_port);
    Receiver* find_broadcast(uint16_t port);

    NetworkLayer::ptr m_network;
    std::set<Socket*> m_sockets; // temporary solution
    std::map<uint16_t, Acceptor*> m_acceptors;
    std::map<uint16_t, BroadcastReceiver*> m_broadcast_receivers;
};

} // namespace ntdcp
