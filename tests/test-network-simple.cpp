#include "ntdcp/network.hpp"
#include "ntdcp/virtual-device.hpp"
#include "test-helpers.hpp"
#include <gtest/gtest.h>

using namespace ntdcp;
using namespace std::literals::chrono_literals;



class NetworkTest : public testing::Test {
protected:
    void SetUp() override {
        medium = std::make_shared<TransmissionMedium>();
        sys = std::make_shared<SystemDriverDeterministic>();
    }

    void TearDown() override {
        medium.reset();
        sys.reset();
        physicals.clear();
        networks.clear();
    }

    void add_net_user(uint64_t addr, const PhysicalInterfaceOptions& opts = PhysicalInterfaceOptions())
    {
        auto phys = VirtualPhysicalInterface::create(opts, sys, medium);
        auto net = std::make_shared<NetworkLayer>(sys, addr);
        net->add_physical(phys);
        physicals.push_back(phys);
        networks[addr] = net;
    }

    void serve_all_nets()
    {
        for (auto it = networks.begin(); it != networks.end(); ++it)
        {
            it->second->serve();
        }
    }

    // void TearDown() override {}
    TransmissionMedium::ptr medium;
    ISystemDriver::ptr sys;
    std::vector<std::shared_ptr<VirtualPhysicalInterface>> physicals;
    std::map<uint64_t, std::shared_ptr<NetworkLayer>> networks;
};

TEST_F(NetworkTest, TwoPointsWired)
{
    add_net_user(123);
    add_net_user(321);

    auto test_buf_1 = Buffer::create_from_string(test_string_1);
    auto test_buf_2 = Buffer::create_from_string(test_string_2);
    auto test_buf_3 = Buffer::create_from_string(test_string_3);

    // 1 -> 2
    networks[123]->send(test_buf_1, 321);
    serve_all_nets();
    auto in1 = networks[123]->incoming();
    auto in2 = networks[321]->incoming();
    ASSERT_FALSE(in1);
    ASSERT_TRUE(in2);
    EXPECT_EQ(in2->source_addr, 123);
    EXPECT_EQ(strcmp((const char*) in2->data->data(), test_string_1), 0);

    // 1 -> 1
    networks[123]->send(test_buf_2, 123);
    serve_all_nets();
    in1 = networks[123]->incoming();
    in2 = networks[321]->incoming();
    ASSERT_TRUE(in1);
    ASSERT_FALSE(in2);
    EXPECT_EQ(in1->source_addr, 123);
    EXPECT_EQ(strcmp((const char*) in1->data->data(), test_string_2), 0);

    // 2 -> 1, 2
    networks[321]->send(test_buf_3, 0xFF);
    serve_all_nets();
    serve_all_nets();
    in1 = networks[123]->incoming();
    in2 = networks[321]->incoming();
    ASSERT_TRUE(in1);
    ASSERT_TRUE(in2);
    EXPECT_EQ(in1->source_addr, 321);
    EXPECT_EQ(strcmp((const char*) in1->data->data(), test_string_3), 0);
    EXPECT_EQ(in2->source_addr, 321);
    EXPECT_EQ(strcmp((const char*) in2->data->data(), test_string_3), 0);
}


