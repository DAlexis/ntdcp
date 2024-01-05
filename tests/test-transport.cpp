#include "ntdcp/network.hpp"
#include "ntdcp/virtual-device.hpp"
#include "ntdcp/transport.hpp"
#include "ntdcp/socket-datagram.hpp"
#include "test-helpers.hpp"

#include <gtest/gtest.h>

using namespace ntdcp;
using namespace std::literals::chrono_literals;

TEST(ChannelTest, SimpleTransmitReceive)
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


    TransportLayer tr1(net1);
    TransportLayer tr2(net2);

    // Transmitters
    auto sock_trans_1_p10 = std::make_shared<SocketTransmitterDatagram>(10, 321);
    tr1.add_transmitter(sock_trans_1_p10);

    auto sock_trans_1_p1 = std::make_shared<SocketTransmitterDatagram>(1, 321);
    tr1.add_transmitter(sock_trans_1_p1);

    auto sock_trans_1_p9999 = std::make_shared<SocketTransmitterDatagram>(9999, 321);
    tr1.add_transmitter(sock_trans_1_p9999);

    // Receivers
    auto sock_recv_2_p10 = std::make_shared<SocketReceiverDatagram>(10);
    tr2.add_receiver(sock_recv_2_p10);

    auto sock_recv_2_p1 = std::make_shared<SocketReceiverDatagram>();
    tr2.add_receiver(sock_recv_2_p1);

    auto sock_recv_2_p9999 = std::make_shared<SocketReceiverDatagram>(9999);
    tr2.add_receiver(sock_recv_2_p9999);

    {
        sock_trans_1_p10->send(Buffer::create_from_string(test_string_1));
        tr1.serve();
        net1->serve();
        net2->serve();
        tr2.serve();
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

        tr1.serve();
        net1->serve();
        net2->serve();
        tr2.serve();

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
}
