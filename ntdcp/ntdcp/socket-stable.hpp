#pragma once

#include "ntdcp/synchronization.hpp"
#include "ntdcp/transport.hpp"

#include <functional>

namespace ntdcp
{


class SocketStable : public SocketReceiver, public SocketTransmitter
{
public:
    struct Options
    {
        enum class Policy
        {
            drop_when_timeout,
            break_when_timeout
        };

        Policy policy = Policy::break_when_timeout;

        std::chrono::milliseconds timeout{10000};
        std::chrono::milliseconds restransmission_time{1000};

        std::chrono::milliseconds force_ack_after{200};
    };


    enum class State
    {
        not_connected,
        connected,
        waiting_for_submit
    };

    enum class MessageType
    {
        conn_request,
        conn_submit,
        conn_close,
        data
    };

    struct StableHeader
    {
        uint16_t seance_id = 0;
        uint16_t message_id = 0;
        uint16_t last_received_message_id = 0;
        uint16_t src_port = 0;

        MessageType type = MessageType::data;
    };

    SocketStable(TransportLayer::ptr transport, uint16_t my_port, uint16_t target_port, uint64_t remote_addr, uint8_t hop_limit = 10);
    ~SocketStable();

    void receive(Buffer::ptr data, uint64_t src_addr, uint16_t package_id, uint8_t header_bits_0) override;
    std::optional<std::pair<uint8_t, SegmentBuffer>> pick_outgoing() override;

    void connect();

    void reset_connection();
    bool busy();
    bool send(Buffer::ptr buf);

    State state() const;

    std::optional<Buffer::ptr> get_incoming();
    bool has_incoming() const;

private:
    struct AckTask
    {
        uint16_t message_id = 0;
        bool was_sent = false;
        std::chrono::steady_clock::time_point time_seg_received;
    };


    struct SendTask
    {
        StableHeader header;
        Buffer::ptr buf;
        int sent_count = 0;
        std::chrono::steady_clock::time_point created;
        std::chrono::steady_clock::time_point last_pick;
    };

    static std::optional<std::pair<StableHeader, Buffer::ptr>> decode_base_header(MemBlock mem, uint8_t header_bits_0);
    /**
     * @brief encode_base_header
     * @param seg_buf
     * @param header
     * @return Transport header byte that will be added to other header byte's bits
     */
    static uint8_t encode_base_header(SegmentBuffer& seg_buf, const StableHeader& header);

    void lifecycle_manage();

    void on_connection_request(uint64_t src_addr, const StableHeader& header, uint8_t header_bits_0);
    void on_connection_submitted(uint64_t src_addr, const StableHeader& header, uint8_t header_bits_0);
    void on_connection_close(uint64_t src_addr, const StableHeader& header, uint8_t header_bits_0);
    void receive_data(uint64_t src_addr, const StableHeader& header, uint8_t header_bits_0, Buffer::ptr data);

    void prepare_ack(uint16_t message_id);

    uint16_t m_out_message_id = 1;
    uint16_t m_last_in_message_id = 0;
    ISystemDriver::ptr m_system;
    TransportLayer::ptr m_transport;

    Options m_options;


    AckTask m_ack_task;
    std::optional<SendTask> m_send_task;

    bool m_is_connected = false;
    bool m_waiting_ack = false;

    State m_state = State::connected;

    uint16_t m_message_id = 1;

    QueueLocking<Buffer::ptr> m_incoming;

    uint16_t m_seance_id = 0;
    uint16_t m_last_data_message_id = 0;
};

}
