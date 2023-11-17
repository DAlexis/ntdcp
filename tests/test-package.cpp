#include "ntdcp/package.hpp"
#include <gtest/gtest.h>

using namespace ntdcp;

TEST(Package, Trivial_serialization_and_deserialization)
{
    Package p;
    uint32_t some_data = 42;
    p.data = reinterpret_cast<uint8_t*>(&some_data);
    p.size = sizeof(some_data);

    //std::vector<uint8_t> buf()
}
