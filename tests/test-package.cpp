#include "ntdcp/package.hpp"
#include <gtest/gtest.h>

using namespace ntdcp;

TEST(Package, Trivial_serialization_and_deserialization)
{
    Package p;
    uint32_t some_data = 42;
    p.data = MemBlock::wrap(some_data);

    p.destination_addr = 12345;
    p.source_addr = 43212;

    p.source_port = 25;
    p.destination_port = 48;
    p.hop_limit = 23;
    p.package_id = 3423;
    p.session_id = 312;

    Package p2 = p;

    ASSERT_EQ(p2, p) << "Unexpected behaviour of package equal operator";

    size_t size = package_size(p);
    Buffer::ptr buf = Buffer::create();
    ASSERT_NO_THROW(write_package(p, *buf));

    EXPECT_EQ(size, buf->size());

    Package p3;
    parse_package(p3, buf->contents());
    EXPECT_EQ(p, p3);
    //std::vector<uint8_t> buf()
}
