#include "ntdcp/network.hpp"
#include "ntdcp/virtual-device.hpp"
#include "ntdcp/transport.hpp"
#include "ntdcp/socket-datagram.hpp"
#include "ntdcp/socket-stable.hpp"
#include "test-helpers.hpp"

#include <gtest/gtest.h>

using namespace ntdcp;
using namespace std::literals::chrono_literals;

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
