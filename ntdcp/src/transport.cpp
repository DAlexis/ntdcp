#include "ntdcp/transport.hpp"

using namespace ntdcp;

ConnectionId::ConnectionId(uint16_t destination_port, uint64_t source_addr, uint16_t source_port) :
    source_addr(source_addr), source_port(source_port), destination_port(destination_port)
{
}

bool ConnectionId::operator<(const ConnectionId& right) const
{
    if (source_addr != right.source_addr)
        return source_addr < right.source_addr;

    if (source_port != right.source_port)
        return source_port < right.source_port;

    return destination_port < right.destination_port;
}

// ---------------------------
// SocketBase

SocketBase::SocketBase(TransportLayer& transport_layer, uint64_t remote_address, uint16_t local_port, uint16_t remote_port, const Options& opts) :
    m_transport_layer(transport_layer), m_remote_address(remote_address), m_remote_port(remote_port), m_local_port(local_port), m_options(opts)
{
}

uint64_t SocketBase::remote_address()
{
    return m_remote_address;
}

uint16_t SocketBase::remote_port()
{
    return m_remote_port;
}

uint16_t SocketBase::local_port()
{
    return m_local_port;
}


ConnectionId SocketBase::incoming_connectiion_id()
{
    return ConnectionId(local_port(), remote_address(), remote_port());
}

SocketBase::~SocketBase()
{
}

// ---------------------------
// Acceptor

Acceptor::Acceptor(TransportLayer& transport_layer, uint16_t listening_port, OnNewConnectionCallback on_new_connection, const Options& opts) :
    SocketBase(transport_layer, 0, listening_port, 0, opts), m_on_new_connection(on_new_connection)
{
    m_transport_layer.add_acceptor(*this);
}

Acceptor::~Acceptor()
{
    m_transport_layer.remove_acceptor(*this);
}

void Acceptor::receive(Buffer::ptr data, const TransportDescription& header)
{
    if (header.type == TransportDescription::Type::connection_request)
    {
        auto already_existed = m_already_created_sockets.get_update(header.message_id);
        if (already_existed)
        {
            auto w_sock = **already_existed;
            if (auto s = w_sock.lock())
            {
                // Socket already exists, redirect to it
                s->send_connection_submit(header.message_id);
                return;
            } else {
                m_already_created_sockets.erase(header.message_id);
            }
        }

        uint16_t new_port;

        while ((new_port = m_transport_layer.system_driver()->random() & 0xFFFF) == 0);

        auto new_sock = std::make_shared<Socket>(m_transport_layer, header.source_addr, new_port, header.source_port, m_options);
        new_sock->send_connection_submit(header.message_id);
        m_already_created_sockets.put_update(header.message_id, new_sock);
        m_on_new_connection(new_sock);
        return;
    }
}

std::optional<std::pair<TransportDescription, SegmentBuffer>> Acceptor::pick_outgoing()
{
    return std::nullopt;
}

// ---------------------------
// Socket

Socket::Socket(TransportLayer& transport_layer, uint64_t remote_address, uint16_t local_port, uint16_t remote_port, const Options& opts) :
    SocketBase(transport_layer, remote_address, local_port, remote_port, opts), m_incoming(*transport_layer.system_driver())
{
    m_transport_layer.add_socket(*this);
}

Socket::~Socket()
{
    m_transport_layer.remove_socket(*this);
}

bool Socket::busy()
{
    return m_send_task.has_value();
}

bool Socket::connect()
{
    if (m_state != State::not_connected)
        return false;

    m_last_outgoing_message_id = 0;

    create_send_task(0, TransportDescription::Type::connection_request, nullptr);

    m_state = State::waiting_for_submit;

    return true;
}

bool Socket::ready_to_send()
{
    return m_state == State::connected && !m_send_task.has_value();
}

void Socket::send_connection_submit(uint16_t message_id)
{
    // TODO if call after already established connection go to broken state
    prepare_ack(message_id);
    // m_ack_task->force_send_immediately = true;

    m_last_outgoing_message_id = 0;

    create_send_task(0, TransportDescription::Type::connection_submit, nullptr);

    m_state = State::connected;
}

bool Socket::send(Buffer::ptr data)
{
    if (m_state != Socket::State::connected)
        return false;

    if (busy())
        return false;

    create_send_task(0, TransportDescription::Type::data_transfer, data);

    return true;
}

bool Socket::has_data()
{
    return !m_incoming.empty();
}

std::optional<Buffer::ptr> Socket::get_received()
{
    return m_incoming.pop();
}

void Socket::close()
{
    if (m_state != Socket::State::connected && m_state != Socket::State::closed)
        return;

    create_send_task(0, TransportDescription::Type::connection_close, nullptr);

    m_state = Socket::State::closed;
}

Socket::State Socket::state()
{
    return m_state;
}

uint16_t Socket::unconfirmed_to_remote()
{
    return m_unconfirmed_to_remote;
}

uint16_t Socket::missed_from_remote()
{
    return m_missed_from_remote;
}

void Socket::receive(Buffer::ptr data, const TransportDescription& header)
{
    if (m_state == State::connection_timeout)
    {
        // We should not do anything after a moment we detected a timeout
        return;
    }

    // Check ack
    if (m_send_task && header.ack_for_message_id == m_send_task->description.message_id)
    {
        // We have acknoledgement that our last transmission is OK, so remove it from outgoing
        m_send_task.reset();
        m_unconfirmed_to_remote--;
    }

    if (header.message_id <= m_last_received_message_id)
    {
        prepare_ack(header.message_id);
        return; // We already got this segment
    }

    if (m_state == Socket::State::closed)
    {
        // We received something after connetion closing
        if (header.type == TransportDescription::Type::connection_close_submit)
        {
            // This is close submit, OK, will not send anything
            m_send_task.reset();
            m_ack_task.reset();
            m_unconfirmed_to_remote--;
            return;
        }

        // And this is not a close submit, lets send close submit that should never have answer
        create_send_task(m_send_task->description.message_id, TransportDescription::Type::connection_close_submit, nullptr);
        return;
    }

    if (header.type == TransportDescription::Type::connection_submit)
    {
        if (m_state != State::waiting_for_submit)
            return;

        m_remote_port = header.source_port;
        m_state = State::connected;
        m_send_task.reset();
        prepare_ack(header.message_id);
        m_last_received_message_id = header.message_id;
        return;
    }

    if (header.type == TransportDescription::Type::connection_close)
    {
        m_state = State::closed;
        create_send_task(0, TransportDescription::Type::connection_close_submit, nullptr);
        m_last_received_message_id = header.message_id;
        return;
    }


    if (data->size() != 0)
    {
        prepare_ack(header.message_id);
        m_incoming.push(data);
    }

    m_missed_from_remote += header.message_id - (m_last_received_message_id + 1);
    m_last_received_message_id = header.message_id;
}

void Socket::prepare_ack(uint16_t message_id)
{
    if (!m_ack_task)
        m_ack_task = AckTask();

    m_ack_task->message_id = message_id;
    m_ack_task->time_seg_received = m_transport_layer.system_driver()->now();
    m_ack_task->was_sent_at_least_once = false;
}

void Socket::create_send_task(uint16_t ack_for_message_id, TransportDescription::Type type, Buffer::ptr buf)
{
    m_send_task = SendTask();
    m_send_task->created = m_transport_layer.system_driver()->now();
    m_send_task->description.message_id = type == TransportDescription::Type::connection_request ? m_transport_layer.system_driver()->random_nonzero() : ++m_last_outgoing_message_id;
    m_send_task->description.ack_for_message_id = ack_for_message_id;
    m_send_task->description.has_ack = (ack_for_message_id != 0);
    m_send_task->description.repeat = 1;
    m_send_task->description.type = type;
    m_send_task->description.destination_addr = m_remote_address;
    m_send_task->description.source_addr = 0;
    m_send_task->description.source_port = m_local_port;
    m_send_task->description.destination_port = m_remote_port;
    m_send_task->timeout = m_options.timeout;
    m_send_task->buf = buf;

    if (type != TransportDescription::Type::connection_close_submit)
        m_unconfirmed_to_remote++;
}

std::optional<std::pair<TransportDescription, SegmentBuffer>> Socket::pick_force_ack()
{
    TransportDescription hdr;
    hdr.ack_for_message_id = m_ack_task->message_id;
    hdr.message_id = 0;
    hdr.has_ack = true;
    hdr.repeat = 1;
    hdr.type = TransportDescription::Type::data_transfer;
    hdr.destination_addr = m_remote_address;
    hdr.destination_port = m_remote_port;
    hdr.source_port = m_local_port;
    hdr.source_addr = 0;

    m_ack_task->was_sent_at_least_once = true;

    return std::make_pair(hdr, SegmentBuffer());
}

void Socket::drop_if_timeout(std::chrono::steady_clock::time_point now)
{
    if (!m_send_task)
        return;

    if (now - m_send_task->created > m_send_task->timeout)
    {
        if (m_send_task->description.type == TransportDescription::Type::connection_request)
        {
            m_state = State::connection_timeout;
        }

        m_send_task.reset();
    }
}

std::optional<std::pair<TransportDescription, SegmentBuffer>> Socket::pick_outgoing()
{
    if (m_state == State::connection_timeout)
    {
        // We should not do anything after a moment we detected a timeout
        return std::nullopt;
    }

    auto now = m_transport_layer.system_driver()->now();
    drop_if_timeout(now);

    if (!m_send_task)
    {
        // Nothing to send, but may be ack is waiting
        if (!m_ack_task->was_sent_at_least_once
            && ((now - m_ack_task->time_seg_received > m_options.force_ack_after)
                || m_ack_task->force_send_immediately))
        {
            // Need to force ack
            return pick_force_ack();
        }

        return std::nullopt;
    }

    // If now a time to transmit
    if (m_send_task->sent_count != 0 && now - m_send_task->last_pick < m_options.restransmission_time)
        return std::nullopt;

    m_send_task->last_pick = now;
    m_send_task->sent_count++;

    SegmentBuffer sg(m_send_task->buf);

    if (m_ack_task)
    {
        m_send_task->description.ack_for_message_id = m_ack_task->message_id;
        m_ack_task->was_sent_at_least_once = true;
    }

    return std::make_pair(m_send_task->description, sg);
}


// ---------------------------
// TransportLayer

TransportLayer::TransportLayer(NetworkLayer::ptr network) :
    m_network(network)
{

}

void TransportLayer::add_socket(Socket& socket)
{
    m_sockets.insert(&socket);
}

void TransportLayer::remove_socket(Socket& socket)
{
    m_sockets.erase(&socket);
}

void TransportLayer::add_acceptor(Acceptor& acceptor)
{
    m_acceptors[acceptor.local_port()] = &acceptor;
}

void TransportLayer::remove_acceptor(Acceptor& acceptor)
{
    m_acceptors.erase(acceptor.local_port());
}

void TransportLayer::serve()
{
    serve_incoming();
    serve_outgoing();
}

SystemDriver::ptr TransportLayer::system_driver()
{
    return m_network->system_driver();
}

void TransportLayer::serve_incoming()
{
    while (std::optional<NetworkLayer::Package> pkg = m_network->incoming())
    {
        uint64_t source_addr = pkg->source_addr;
        Buffer::ptr pkg_data = pkg->data;
        std::optional<std::pair<TransportDescription, Buffer::ptr>> p = decode(pkg_data->contents());
        if (!p)
            continue;

        TransportDescription& header = p->first;
        header.source_addr = source_addr;

        Buffer::ptr data = p->second;

        SocketBase* s = nullptr;

        switch(header.type)
        {
        case TransportDescription::Type::connection_request:
            s = find_acceptor(header.destination_port);
            break;
        case TransportDescription::Type::connection_submit:
            s = find_socket_for_submit(header.source_addr, header.destination_port);
            break;
        case TransportDescription::Type::connection_close_submit:
            s = find_socket_for_close_submit(header.source_addr, header.source_port, header.destination_port);
            break;
        default:
            s = find_socket_for_data(header.source_addr, header.source_port, header.destination_port);
        }

        if (!s)
            continue;

        s->receive(data, header);
    }
}

void TransportLayer::serve_outgoing()
{
    for (auto it = m_sockets.begin(); it != m_sockets.end(); ++it)
    {
        Socket* s = *it;

        while (auto out = s->pick_outgoing())
        {
            const TransportDescription& header = out->first;
            SegmentBuffer& seg_buf = out->second;

            encode(seg_buf, header);
            m_network->send(seg_buf, s->remote_address());
        }
    }
}

Acceptor* TransportLayer::find_acceptor(uint16_t port)
{
    auto it = m_acceptors.find(port);
    if (it != m_acceptors.end())
        return it->second;
    else
        return nullptr;
}

Socket* TransportLayer::find_socket_for_data(uint64_t source_addr, uint16_t source_port, uint16_t dst_port)
{
    for (auto s : m_sockets)
    {
        if (s->state() != Socket::State::connected)
            continue;
        if (s->remote_address() == source_addr && s->remote_port() == source_port && s->local_port() == dst_port)
            return s;
    }
    return nullptr;
}

Socket* TransportLayer::find_socket_for_close_submit(uint64_t source_addr, uint16_t source_port, uint16_t dst_port)
{
    for (auto s : m_sockets)
    {
        if (s->state() != Socket::State::closed)
            continue;
        if (s->remote_address() == source_addr && s->remote_port() == source_port && s->local_port() == dst_port)
            return s;
    }
    return nullptr;
}

Socket* TransportLayer::find_socket_for_submit(uint64_t source_addr, uint16_t dst_port)
{
    for (auto s : m_sockets)
    {
        // Connection submit may be directed to socket waiting for submit or socket that has already received a copy
        // of connection submit, sent ack, but ack was failed, so we should repeat ack
        if (s->state() != Socket::State::waiting_for_submit && s->state() != Socket::State::connected)
            continue;

        if (s->remote_address() == source_addr && s->local_port() == dst_port)
            return s;
    }
    return nullptr;
}

std::optional<std::pair<TransportDescription, Buffer::ptr>> TransportLayer::decode(MemBlock mem)
{
    // Trivial impl
    if (mem.size() < sizeof(TransportDescription))
        return std::nullopt;

    TransportDescription header;
    mem >> header;
    return std::make_pair(header, Buffer::create(mem));
}

void TransportLayer::encode(SegmentBuffer& seg_buf, const TransportDescription& header)
{
    // Trivial impl
    auto b = Buffer::create();
    b->raw() << header;
    seg_buf.push_front(b);
}


/*
SocketBase::SocketBase(const ConnectionId& conn_id) :
    m_connection_id(conn_id)
{
}

const ConnectionId& SocketBase::connection_id()
{
    return m_connection_id;
}

SocketReceiver::SocketReceiver(const ConnectionId& conn_id) :
    SocketBase(conn_id)
{
}


SocketTransmitter::SocketTransmitter(const ConnectionId& conn_id, uint8_t hop_limit) :
    m_remote_port(target_port), m_remote_addr(remote_addr), m_hop_limit(hop_limit)
{
}

uint16_t SocketTransmitter::remote_port() const
{
    return m_remote_port;
}

uint64_t SocketTransmitter::remote_addr() const
{
    return m_remote_addr;
}

uint8_t SocketTransmitter::hop_limit() const
{
    return m_hop_limit;
}


TransportLayer::TransportLayer(NetworkLayer::ptr network) :
    m_network(network)
{
}

void TransportLayer::add_receiver(SocketReceiver& socket)
{
    m_receivers[socket.port()] = &socket;
}

void TransportLayer::add_transmitter(SocketTransmitter& socket)
{
    m_transmitters.insert(&socket);
}

void TransportLayer::remove_receiver(SocketReceiver& socket)
{
    m_receivers.erase(socket.port());
}

void TransportLayer::remove_transmitter(SocketTransmitter& socket)
{
    m_transmitters.erase(&socket);
}

void TransportLayer::serve()
{
    serve_incoming();
    serve_outgoing();
}

NetworkLayer& TransportLayer::network()
{
    return *m_network;
}


void TransportLayer::serve_incoming()
{
    while (std::optional<NetworkLayer::Package> pkg = m_network->incoming())
    {
        uint64_t source_addr = pkg->source_addr;
        Buffer::ptr pkg_data = pkg->data;
        std::optional<std::pair<TransportLayer::TransportHeader0, Buffer::ptr>> p = decode_base_header(pkg_data->contents());
        if (!p)
            continue;

        const TransportLayer::TransportHeader0& header = p->first;
        Buffer::ptr data = p->second;

        auto it = m_receivers.find(header.target_port);
        if (it == m_receivers.end())
            continue;

        SocketReceiver& s = *it->second;
        s.receive(data, source_addr, pkg->package_id, header.header_byte_0);

    }
}

void TransportLayer::serve_outgoing()
{
    for (auto it = m_transmitters.begin(); it != m_transmitters.end();)
    {
        SocketTransmitter& s = **it;

        while (auto out = s.pick_outgoing())
        {
            TransportLayer::TransportHeader0 header;
            header.header_byte_0 = out->first;
            header.target_port = s.remote_port();
            SegmentBuffer& seg_buf = out->second;
            encode_base_header(seg_buf, header);
            m_network->send(seg_buf, s.remote_addr(), s.hop_limit());
        }
        ++it;

    }
}

std::optional<uint16_t> TransportLayer::decode_port(MemBlock& mem, uint8_t port_size_bits)
{
    uint16_t port;
    switch(port_size_bits)
    {
    case 0b01: // Default port == 1
        port = 1;
        break;
    case 0b10: // Port number <= 255
    {
        if (mem.size() < sizeof(uint8_t))
            return std::nullopt;

        uint8_t port_number = 0;
        mem >> port_number;
        port = port_number;
        break;
    }
    case 0b11: // Port number > 255
        if (mem.size() < sizeof(uint16_t))
            return std::nullopt;

        mem >> port;
        break;
    default:
        return std::nullopt;
    }
    return port;
}


std::optional<std::pair<TransportLayer::TransportHeader0, Buffer::ptr>> TransportLayer::decode_base_header(MemBlock mem)
{
    if (mem.size() < sizeof(uint8_t))
        return std::nullopt;

    TransportLayer::TransportHeader0 header;
    mem >> header.header_byte_0;

    uint8_t dst_port_bits = header.header_byte_0 & 0x03;
    auto port = decode_port(mem, dst_port_bits);
    if (!port)
        return std::nullopt;

    header.target_port = *port;
    return std::make_pair(header, Buffer::create(mem));
}

void TransportLayer::encode_base_header(SegmentBuffer& seg_buf, const TransportLayer::TransportHeader0& header)
{
    Buffer::ptr b = Buffer::create();
    uint8_t header_bits_0 = header.header_byte_0 & 0b11111100;

    if (header.target_port == 1)
    {
        header_bits_0 |= 0b01;
        b->raw() << header_bits_0;
    }
    else if (header.target_port <= 255)
    {
        header_bits_0 |= 0b10;
        uint8_t short_target_port = uint8_t(header.target_port);
        b->raw() << header_bits_0 << short_target_port;
    } else {
        header_bits_0 |= 0b11;
        b->raw() << header_bits_0 << header.target_port;
    }
    seg_buf.push_front(b);
}
*/
