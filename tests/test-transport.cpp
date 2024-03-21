#include "ntdcp/network.hpp"
#include "ntdcp/virtual-device.hpp"
#include "ntdcp/transport.hpp"
#include "test-helpers.hpp"

#include <gtest/gtest.h>
#include <cstdlib>


using namespace ntdcp;
using namespace std::literals::chrono_literals;

class ExchangeSimulation
{
public:
    struct Client
    {
        Client(TransmissionMedium::ptr medium, ISystemDriver::ptr sys, uint64_t addr) :
            phys(VirtualPhysicalInterface::create(PhysicalInterfaceOptions(), sys, medium)),
            net(std::make_shared<NetworkLayer>(sys, addr)),
            transport(std::make_shared<TransportLayer>(net))
        {
            net->add_physical(phys);
        }

        void add_acceptor(uint16_t listening_port)
        {
            auto emp = acceptors.emplace(listening_port, Acceptor(
                *transport,
                listening_port,
                [this](std::shared_ptr<Socket> sock)
                {
                    accepted_sockets[sock->local_port()] = sock;
                }
            ));
            transport->add_acceptor(emp.first->second);
        }

        void add_initial_socket(uint64_t remote_addr, uint16_t local_port, uint16_t remote_port)
        {
            initial_sockets.emplace(local_port, std::make_shared<Socket>(*transport, remote_addr, local_port, remote_port));
        }

        void serve()
        {
            transport->serve();
            net->serve();
        }

        std::shared_ptr<VirtualPhysicalInterface> phys;
        NetworkLayer::ptr net;
        TransportLayer::ptr transport;

        std::map<uint16_t, std::shared_ptr<Socket>> accepted_sockets;
        std::map<uint16_t, std::shared_ptr<Socket>> initial_sockets;
        std::map<uint16_t, Acceptor> acceptors;
    };

    Client& add_client(uint64_t addr)
    {
        return clients.emplace(addr, Client(medium, sys, addr)).first->second;
    }

    void serve_all()
    {
        for (auto it = clients.begin(); it != clients.end(); ++it)
        {
            it->second.serve();
        }
    }

    TransmissionMedium::ptr medium{std::make_shared<TransmissionMedium>()};
    std::shared_ptr<SystemDriverDeterministic> sys{std::make_shared<SystemDriverDeterministic>()};
    std::map<uint64_t, Client> clients;
};

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
    EXPECT_EQ(initial_socket.unconfirmed_to_remote(), 1);
    EXPECT_EQ(initial_socket.missed_from_remote(), 0);
    ASSERT_TRUE(initial_socket.busy());

    // Connection request 123:300-->321:10
    tr1->serve();
    net1->serve();
    net2->serve();
    tr2->serve(); // Create new socker 321:rnd. Connection submit 321:rnd-->123:300

    ASSERT_TRUE(accepted_socket);
    ASSERT_TRUE(accepted_socket->busy());
    EXPECT_EQ(accepted_socket->unconfirmed_to_remote(), 1);

    net2->serve();
    net1->serve();
    tr1->serve(); // 123:300 received connection submit

    ASSERT_FALSE(initial_socket.busy());
    EXPECT_EQ(initial_socket.unconfirmed_to_remote(), 0);
    EXPECT_EQ(initial_socket.missed_from_remote(), 0);

    Socket::Options default_socket_options;
    sys->increment_time(default_socket_options.force_ack_after + 1ms);

    tr1->serve(); // 123:300 force ack --> 321:rnd
    net1->serve();
    net2->serve();
    tr2->serve(); // 321:rnd receive ack
    ASSERT_FALSE(accepted_socket->busy());
    EXPECT_EQ(accepted_socket->unconfirmed_to_remote(), 0);
    EXPECT_EQ(initial_socket.missed_from_remote(), 0);
    EXPECT_EQ(accepted_socket->missed_from_remote(), 0);

    ASSERT_TRUE(initial_socket.state() == Socket::State::connected);


    initial_socket.send(Buffer::create_from_string(test_string_1));
    tr1->serve(); // 123:300 send data --> 321:rnd
    net1->serve();
    net2->serve();
    tr2->serve(); // 321:rnd receive data

    ASSERT_TRUE(accepted_socket->has_data());
    ASSERT_TRUE(initial_socket.busy());
    EXPECT_EQ(accepted_socket->missed_from_remote(), 0);

    auto incoming = accepted_socket->get_received();

    ASSERT_TRUE(incoming.has_value());
    EXPECT_EQ(strcmp((const char*) incoming.value()->data(), test_string_1), 0);
    ASSERT_FALSE(accepted_socket->has_data());

    sys->increment_time(default_socket_options.force_ack_after + 1ms);

    tr2->serve(); // 321:rnd force ack --> 123:300
    net2->serve();
    net1->serve();
    tr1->serve(); // 123:300 receive ack

    EXPECT_EQ(initial_socket.missed_from_remote(), 0);
    EXPECT_EQ(accepted_socket->missed_from_remote(), 0);
    EXPECT_EQ(accepted_socket->unconfirmed_to_remote(), 0);

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

    EXPECT_EQ(initial_socket.missed_from_remote(), 0);
    EXPECT_EQ(accepted_socket->missed_from_remote(), 0);
    EXPECT_EQ(accepted_socket->unconfirmed_to_remote(), 1);

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

    EXPECT_EQ(accepted_socket->missed_from_remote(), 0);
    EXPECT_EQ(accepted_socket->unconfirmed_to_remote(), 0);

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

    EXPECT_EQ(initial_socket.missed_from_remote(), 0);
    EXPECT_EQ(initial_socket.unconfirmed_to_remote(), 0);

    // These fail
    EXPECT_EQ(accepted_socket->missed_from_remote(), 0);
    EXPECT_EQ(accepted_socket->unconfirmed_to_remote(), 0);

    ASSERT_TRUE(accepted_socket->pick_outgoing() == std::nullopt); // Nothing should be sent because close submit received
}

TEST(TransoportLevel, NotStableTransmission)
{
    ExchangeSimulation sim;
    const int clients_count = 2;
    const int sockets_count = 1;
    const int cycles_count = 1000;

    for (int i = 1; i <= clients_count; i++)
    {
        auto& c = sim.add_client(i);
        for (int j = 1; j <= sockets_count; j++)
        {
            c.add_acceptor(j);
            uint64_t target_address = (i-1 + j) % clients_count + 1;
            c.add_initial_socket(target_address, 100 + j, j);
        }
    }

    std::srand(0);
    int connections_count = 0;

    for (int i = 0; i < cycles_count; i++)
    {
        sim.sys->increment_time(100ms);
        sim.serve_all();
        if (rand() % 5 == 0)
        {
            int client_addr = rand() % clients_count + 1;
            // Get one random initial socket
            int socket_port = 100 + rand() % sockets_count + 1;
            Socket& s = *sim.clients.at(client_addr).initial_sockets.at(socket_port);
            // If it is not already in connection process or connected, connect
            if (s.state() == Socket::State::not_connected)
            {
                s.connect();
                connections_count++;
                continue;
            }
        }
         sim.medium->broken() = false;
        if (rand() % 2 == 0)
        {
            sim.medium->broken() = true;
        }

        // if (rand() % 7 == 0)
        // {
        //     sim.medium->set_broken(false);
        // }

    }

    int total_accepted_sockets = 0;
    for (int i = 1; i <= clients_count; i++)
    {
        total_accepted_sockets += sim.clients.at(i).accepted_sockets.size();
    }

    EXPECT_EQ(connections_count, total_accepted_sockets);
}
