#include "ntdcp/socket-stable.hpp"


using namespace ntdcp;

SocketStable::SocketStable(TransportLayer::ptr transport, uint16_t my_port, uint16_t target_port, uint64_t target_addr, uint8_t hop_limit) :
    SocketReceiver(my_port), SocketTransmitter(target_port, target_addr, hop_limit),
    m_system(transport->network().system_driver()), m_transport(transport), m_incoming(*transport->network().system_driver())
{
    m_transport->add_receiver(*this);
    m_transport->add_transmitter(*this);
}

SocketStable::~SocketStable()
{
    m_transport->remove_receiver(*this);
    m_transport->remove_transmitter(*this);
}

std::optional<std::pair<SocketStable::StableHeader, Buffer::ptr>> SocketStable::decode_base_header(MemBlock mem, uint8_t header_bits_0)
{
    // Temporal implementation
    if (mem.size() < sizeof(StableHeader))
        return std::nullopt;

    StableHeader header;
    mem >> header;
    return std::make_pair(header, Buffer::create(mem));
}

uint8_t SocketStable::encode_base_header(SegmentBuffer& seg_buf, const SocketStable::StableHeader& header)
{
    // Temporal implementation
    auto buf = Buffer::create();
    buf->raw() << header;
    seg_buf.push_front(buf);
    return 0;
}

void SocketStable::receive(Buffer::ptr data, uint64_t src_addr, uint16_t package_id, uint8_t header_bits_0)
{
    std::optional<std::pair<StableHeader, Buffer::ptr>> decoded = decode_base_header(data->contents(), header_bits_0);
    if (!decoded)
        return;

    const StableHeader& header = decoded->first;
    Buffer::ptr buf = decoded->second;

    // Check ack
    if (m_send_task && header.last_received_message_id == m_send_task->header.message_id)
    {
        // We have acknoledgement that our last transmission is OK, so remove it from outgoing
        m_send_task.reset();
    }

    switch(header.type)
    {
    case MessageType::conn_request:
        on_connection_request(src_addr, header, header_bits_0);
        return;

    case MessageType::conn_submit:
        on_connection_submitted(src_addr, header, header_bits_0);
        return;

    case MessageType::conn_close:
        on_connection_close(src_addr, header, header_bits_0);
        return;

    case MessageType::data:
        receive_data(src_addr, header, header_bits_0, buf);
        return;
    };
}

std::optional<std::pair<uint8_t, SegmentBuffer>> SocketStable::pick_outgoing()
{
    // drop_if_timeout();
    auto now = m_transport->network().system_driver()->now();

    if (!m_send_task)
    {
        // Nothing to send
        if (!m_ack_task.was_sent && now - m_ack_task.time_seg_received > m_options.force_ack_after)
        {
            // But need to force ack
            StableHeader hdr;
            hdr.seance_id = m_my_port;
            hdr.message_id = 0;
            hdr.last_received_message_id = m_ack_task.message_id;

            SegmentBuffer sg;
            uint8_t flags = encode_base_header(sg, hdr);

            m_ack_task.was_sent = true;
            return std::make_pair(flags, sg);
        }
        return std::nullopt;   
    }

    if (m_send_task->sent_count != 0 && now - m_send_task->last_pick < m_options.restransmission_time)
        return std::nullopt;

    m_send_task->last_pick = now;
    m_send_task->sent_count++;

    SegmentBuffer sg(m_send_task->buf);

    m_send_task->header.last_received_message_id = m_ack_task.message_id;
    m_ack_task.was_sent = false;

    uint8_t flags = encode_base_header(sg, m_send_task->header);
    return std::make_pair(flags, sg);
}

void SocketStable::reset_connection()
{
    m_ack_task = AckTask();
    m_send_task.reset();
    m_state = State::not_connected;
    // m_message_id = 1;
}

bool SocketStable::send(Buffer::ptr buf)
{
    if (busy())
        return false;

    SendTask task;
    task.header.seance_id = m_seance_id;
    task.header.message_id = m_message_id++;
    task.header.type = MessageType::data;

    task.buf = buf;
    task.created = m_system->now();

    m_send_task = task;

    return true;
}

bool SocketStable::busy()
{
    if (m_state != State::connected)
        return true;

    return m_send_task.has_value();
}

SocketStable::State SocketStable::state() const
{
    return m_state;
}

std::optional<Buffer::ptr> SocketStable::get_incoming()
{
    return m_incoming.pop();
}

bool SocketStable::has_incoming() const
{
    return !m_incoming.empty();
}

void SocketStable::lifecycle_manage()
{
    if (!m_send_task)
        return;

    auto now = m_transport->network().system_driver()->now();

    if (now - m_send_task->created > m_options.timeout)
    {
        m_send_task.reset();
    }
}

void SocketStable::on_connection_request(uint64_t src_addr, const StableHeader& header, uint8_t header_bits_0)
{
    if (m_state != State::not_connected)
        return;

    // Submit a connection
    m_remote_port = header.src_port;
    m_seance_id = header.seance_id;

    SendTask task;
    task.header.seance_id = m_seance_id;
    task.header.message_id = 0;
    task.header.src_port = m_my_port;
    task.header.type = MessageType::conn_submit;

    task.buf = nullptr;
    task.created = m_system->now();

    m_send_task = task;

    m_state = State::connected;

    prepare_ack(header.message_id);
}

void SocketStable::on_connection_submitted(uint64_t src_addr, const StableHeader& header, uint8_t header_bits_0)
{
    if (m_state != State::waiting_for_submit)
        return;

    if (header.seance_id != m_seance_id)
        return;

    m_state = State::connected;
    prepare_ack(header.message_id);
}

void SocketStable::on_connection_close(uint64_t src_addr, const StableHeader& header, uint8_t header_bits_0)
{
    if (header.seance_id != m_seance_id)
        return;

    m_state = State::not_connected;

    prepare_ack(header.message_id);
}

void SocketStable::receive_data(uint64_t src_addr, const StableHeader& header, uint8_t header_bits_0, Buffer::ptr buf)
{
    if (buf->size() == 0)
        return; // Segment is empty, this is only ack

    prepare_ack(header.message_id);

    if (header.message_id <= m_last_data_message_id)
        return; // We already got this segment

    m_incoming.push(buf);
}

void SocketStable::prepare_ack(uint16_t message_id)
{
    m_ack_task.message_id = message_id;
    m_ack_task.time_seg_received = m_system->now();
    m_ack_task.was_sent = false;
}
