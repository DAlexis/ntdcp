#include "ntdcp/package.hpp"

using namespace ntdcp;

#pragma pack(push, 1)
struct MinimalHeader
{
    uint8_t version : 2;
    uint8_t source_addr_size : 2;
    uint8_t destination_addr_size : 2;
    uint8_t transport_type : 2;
};
#pragma pack(pop)

static_assert(sizeof(MinimalHeader) == 1, "struct MinimalHeader has size > 1 byte");

bool ntdcp::parse_package(Package& out, uint8_t* buffer, uint16_t buffer_size)
{
    if (buffer_size < sizeof(MinimalHeader))
        return false;

    out = Package();
    const MinimalHeader* header = reinterpret_cast<const MinimalHeader*>(buffer);

    uint8_t next_byte = *(buffer + sizeof(MinimalHeader));
    uint8_t source_port_size = 0;
    uint8_t dst_port_size = 0;
    if (next_byte & 1)
    {
        // Has source port, next bit gives port size
        next_byte >>= 1;
        source_port_size = (next_byte & 1) ? 2 : 1;
        next_byte >>= 1;
    }
    return false;
}

uint16_t ntdcp::package_size(const Package& out)
{
    return 0;
}

void ntdcp::write_package(const Package& package, uint8_t* buffer)
{
}
