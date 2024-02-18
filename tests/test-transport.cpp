#include "ntdcp/network.hpp"
#include "ntdcp/virtual-device.hpp"
#include "ntdcp/transport.hpp"
#include "test-helpers.hpp"

#include <gtest/gtest.h>

using namespace ntdcp;
using namespace std::literals::chrono_literals;
/*
class ExchangeSimulation
{
public:

    TransmissionMedium::ptr medium{std::make_shared<TransmissionMedium>()};
    std::shared_ptr<SystemDriverDeterministic> sys{std::make_shared<SystemDriverDeterministic>()};

    struct Client
    {
        Client(TransmissionMedium::ptr medium, ISystemDriver::ptr sys, uint64_t addr) :
            phys(VirtualPhysicalInterface::create(PhysicalInterfaceOptions(), sys, medium)),
            net(std::make_shared<NetworkLayer>(sys, addr))
        {
        }

        void add_acceptor(uint16_t listening_port)
        {
            acceptors[listening_port] = Acceptor();
        }

        std::shared_ptr<VirtualPhysicalInterface> phys;
        NetworkLayer::ptr net;

        std::map<uint16_t, std::shared_ptr<Socket>> accepted_sockets;
        std::map<uint16_t, std::shared_ptr<Socket>> initial_sockets;
        std::map<uint16_t, Acceptor> acceptors;
    };
};*/

TEST(TransoportLevel, ConnectionLifecycle)
{
    TransmissionMedium::ptr medium = std::make_shared<TransmissionMedium>();
    std::shared_ptr<SystemDriverDeterministic> sys = std::make_shared<SystemDriverDeterministic>();

    PhysicalInterfaceOptions opts;

    std::shared_ptr<VirtualPhysicalInterface> phys1 = VirtualPhysicalInterface::create(opts, sys, medium);
    std::shared_ptr<VirtualPhysicalInterface> phys2 = VirtualPhysicalInterface::create(opts, sys, medium);

    NetworkLayer::ptr net1 = std::make_shared<NetworkLayer>(sys, 123);
    net1->add_physical(phys1);

    NetworkLayer::ptr net2 = std::make_shared<NetworkLayer>(sys, 321);
    net2->add_physical(phys2);


    auto tr1 = std::make_shared<TransportLayer>(net1);
    auto tr2 = std::make_shared<TransportLayer>(net2);


    Socket initial_socket(*tr1, 321, 300, 10);
    ASSERT_TRUE(initial_socket.state() == Socket::State::not_connected);

    std::shared_ptr<Socket> accepted_socket;

    Acceptor acc(*tr2, 10, [&accepted_socket](std::shared_ptr<Socket> sock) { accepted_socket = sock; });

    initial_socket.connect();
    ASSERT_TRUE(initial_socket.busy());

    // Connection request 123:300-->321:10
    tr1->serve();
    net1->serve();
    net2->serve();
    tr2->serve(); // Create new socker 321:rnd. Connection submit 321:rnd-->123:300

    ASSERT_TRUE(accepted_socket);
    ASSERT_TRUE(accepted_socket->busy());

    net2->serve();
    net1->serve();
    tr1->serve(); // 123:300 received connection submit

    ASSERT_FALSE(initial_socket.busy());

    Socket::Options default_socket_options;
    sys->increment_time(default_socket_options.force_ack_after + 1ms);

    tr1->serve(); // 123:300 force ack --> 321:rnd
    net1->serve();
    net2->serve();
    tr2->serve(); // 321:rnd receive ack
    ASSERT_FALSE(accepted_socket->busy());

    ASSERT_TRUE(initial_socket.state() == Socket::State::connected);


    initial_socket.send(Buffer::create_from_string(test_string_1));
    tr1->serve(); // 123:300 send data --> 321:rnd
    net1->serve();
    net2->serve();
    tr2->serve(); // 321:rnd receive data

    ASSERT_TRUE(accepted_socket->has_data());
    ASSERT_TRUE(initial_socket.busy());

    auto incoming = accepted_socket->get_received();

    ASSERT_TRUE(incoming.has_value());
    EXPECT_EQ(strcmp((const char*) incoming.value()->data(), test_string_1), 0);
    ASSERT_FALSE(accepted_socket->has_data());

    sys->increment_time(default_socket_options.force_ack_after + 1ms);

    tr2->serve(); // 321:rnd force ack --> 123:300
    net2->serve();
    net1->serve();
    tr1->serve(); // 123:300 receive ack

    ASSERT_FALSE(initial_socket.busy());

    // Without ack forcing:
    initial_socket.send(Buffer::create_from_string(test_string_2));
    tr1->serve(); // 123:300 send data --> 321:rnd
    net1->serve();
    net2->serve();
    tr2->serve(); // 321:rnd receive data

    accepted_socket->send(Buffer::create_from_string(test_string_3));

    tr2->serve(); // 321:rnd send data + ack --> 123:300
    net2->serve();
    net1->serve();
    tr1->serve(); // 123:300 receive data

    ASSERT_FALSE(initial_socket.busy());
    ASSERT_TRUE(initial_socket.has_data());
    incoming = initial_socket.get_received();
    ASSERT_TRUE(incoming.has_value());
    EXPECT_EQ(strcmp((const char*) incoming.value()->data(), test_string_3), 0);

    ASSERT_TRUE(accepted_socket->busy());
    ASSERT_TRUE(accepted_socket->has_data());
    incoming = accepted_socket->get_received();
    ASSERT_TRUE(incoming.has_value());
    EXPECT_EQ(strcmp((const char*) incoming.value()->data(), test_string_2), 0);

    sys->increment_time(default_socket_options.force_ack_after + 1ms);
    tr1->serve(); // 123:300 send ack --> 321:rnd
    net1->serve();
    net2->serve();
    tr2->serve(); // 321:rnd receive ack

    ASSERT_FALSE(accepted_socket->busy());

    ASSERT_TRUE(accepted_socket->state() == Socket::State::connected);
    ASSERT_TRUE(initial_socket.state() == Socket::State::connected);

    accepted_socket->close();

    tr2->serve(); // 321:rnd send close message --> 123:300
    net2->serve();
    net1->serve();
    tr1->serve(); // 123:300 receive data close message

    ASSERT_TRUE(initial_socket.state() == Socket::State::closed);

    tr1->serve(); // 123:300 close submit --> 321:rnd
    net1->serve();
    net2->serve();
    tr2->serve(); // 321:rnd receive ack

    sys->increment_time(default_socket_options.restransmission_time + 1ms);

    ASSERT_TRUE(accepted_socket->pick_outgoing() == std::nullopt); // Nothing should be sent because close submit received
}

TEST(TransoportLevel, NotStableTransmission)
{

}

/*
TEST(ChannelTest, TransmitReceiveDatagram)
{
    TransmissionMedium::ptr medium = std::make_shared<TransmissionMedium>();
    ISystemDriver::ptr sys = std::make_shared<SystemDriverDeterministic>();

    PhysicalInterfaceOptions opts;

    std::shared_ptr<VirtualPhysicalInterface> phys1 = VirtualPhysicalInterface::create(opts, sys, medium);
    std::shared_ptr<VirtualPhysicalInterface> phys2 = VirtualPhysicalInterface::create(opts, sys, medium);

    NetworkLayer::ptr net1 = std::make_shared<NetworkLayer>(sys, 123);
    net1->add_physical(phys1);

    NetworkLayer::ptr net2 = std::make_shared<NetworkLayer>(sys, 321);
    net2->add_physical(phys2);


    auto tr1 = std::make_shared<TransportLayer>(net1);
    auto tr2 = std::make_shared<TransportLayer>(net2);

    // Transmitters
    auto sock_trans_1_p10 = std::make_shared<SocketTransmitterDatagram>(tr1, 10, 321);
    auto sock_trans_1_p1 = std::make_shared<SocketTransmitterDatagram>(tr1, 1, 321);
    auto sock_trans_1_p9999 = std::make_shared<SocketTransmitterDatagram>(tr1, 9999, 321);


    // Receivers
    auto sock_recv_2_p10 = std::make_shared<SocketReceiverDatagram>(tr2, 10);
    auto sock_recv_2_p1 = std::make_shared<SocketReceiverDatagram>(tr2);
    auto sock_recv_2_p9999 = std::make_shared<SocketReceiverDatagram>(tr2, 9999);

    {
        sock_trans_1_p10->send(Buffer::create_from_string(test_string_1));
        tr1->serve();
        net1->serve();
        net2->serve();
        tr2->serve();
        ASSERT_TRUE(sock_recv_2_p10->has_incoming());
        ASSERT_FALSE(sock_recv_2_p1->has_incoming());
        ASSERT_FALSE(sock_recv_2_p9999->has_incoming());
        auto in2 = sock_recv_2_p10->get_incoming();
        EXPECT_EQ(strcmp((const char*) in2->data->data(), test_string_1), 0);
    }


    {
        sock_trans_1_p10->send(Buffer::create_from_string(test_string_2));
        sock_trans_1_p1->send(Buffer::create_from_string(test_string_1));
        sock_trans_1_p9999->send(Buffer::create_from_string(test_string_3));

        tr1->serve();
        net1->serve();
        net2->serve();
        tr2->serve();

        ASSERT_TRUE(sock_recv_2_p10->has_incoming());
        ASSERT_TRUE(sock_recv_2_p1->has_incoming());
        ASSERT_TRUE(sock_recv_2_p9999->has_incoming());
        auto in2 = sock_recv_2_p10->get_incoming();
        EXPECT_EQ(strcmp((const char*) in2->data->data(), test_string_2), 0);
        in2 = sock_recv_2_p1->get_incoming();
        EXPECT_EQ(strcmp((const char*) in2->data->data(), test_string_1), 0);
        in2 = sock_recv_2_p9999->get_incoming();
        EXPECT_EQ(strcmp((const char*) in2->data->data(), test_string_3), 0);
    }

    {
        sock_trans_1_p9999->send(Buffer::create_from_string(test_string_1));
        sock_trans_1_p9999->send(Buffer::create_from_string(test_string_2));
        sock_trans_1_p9999->send(Buffer::create_from_string(test_string_3));

        tr1->serve();
        net1->serve();
        net2->serve();
        tr2->serve();
        ASSERT_TRUE(sock_recv_2_p9999->has_incoming());
        auto in2 = sock_recv_2_p9999->get_incoming();
        EXPECT_EQ(strcmp((const char*) in2->data->data(), test_string_1), 0);

        ASSERT_TRUE(sock_recv_2_p9999->has_incoming());
        in2 = sock_recv_2_p9999->get_incoming();
        EXPECT_EQ(strcmp((const char*) in2->data->data(), test_string_2), 0);

        ASSERT_TRUE(sock_recv_2_p9999->has_incoming());
        in2 = sock_recv_2_p9999->get_incoming();
        EXPECT_EQ(strcmp((const char*) in2->data->data(), test_string_3), 0);
    }
}

TEST(ChannelTest, TransmitReceiveStable)
{
    TransmissionMedium::ptr medium = std::make_shared<TransmissionMedium>();
    std::shared_ptr<SystemDriverDeterministic> sys = std::make_shared<SystemDriverDeterministic>();

    PhysicalInterfaceOptions opts;

    std::shared_ptr<VirtualPhysicalInterface> phys1 = VirtualPhysicalInterface::create(opts, sys, medium);
    std::shared_ptr<VirtualPhysicalInterface> phys2 = VirtualPhysicalInterface::create(opts, sys, medium);

    NetworkLayer::ptr net1 = std::make_shared<NetworkLayer>(sys, 123);
    net1->add_physical(phys1);

    NetworkLayer::ptr net2 = std::make_shared<NetworkLayer>(sys, 321);
    net2->add_physical(phys2);


    auto tr1 = std::make_shared<TransportLayer>(net1);
    auto tr2 = std::make_shared<TransportLayer>(net2);

    // Transmitters
    auto sock1_p1 = std::make_shared<SocketStable>(tr1, 1, 2, 321);

    // Receivers
    auto sock2_p2 = std::make_shared<SocketStable>(tr2, 2, 1, 123);

    {
        ASSERT_TRUE(sock1_p1->send(Buffer::create_from_string(test_string_1)));

        tr1->serve();
        net1->serve();
        net2->serve();
        tr2->serve();

        ASSERT_TRUE(sock1_p1->busy());

        ASSERT_TRUE(sock2_p2->send(Buffer::create_from_string(test_string_2)));

        for (int i = 0; i < 4; i++)
        {
            tr1->serve();
            net1->serve();
            net2->serve();
            tr2->serve();
        }

        ASSERT_FALSE(sock1_p1->busy());

        sys->increment_time(500ms);
        // After this time ack from 1 to 2 should be sent automatically, without real message

        tr1->serve();
        net1->serve();
        net2->serve();
        tr2->serve();

        ASSERT_FALSE(sock2_p2->busy());

        ASSERT_TRUE(sock2_p2->has_incoming());
        auto in2 = sock2_p2->get_incoming();
        EXPECT_EQ(strcmp((const char*) (*in2)->data(), test_string_1), 0);
    }
}
*/
