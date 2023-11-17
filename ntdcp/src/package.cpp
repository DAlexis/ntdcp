#include "ntdcp/package.hpp"
#include <cstring>

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

bool ntdcp::parse_package(Package& out, MemBlock block)
{
    out = Package();

    /// Temporal trivial implementation
    if (!block.can_contain<Package>())
        return false;

    block >> out;
    if (out.size > block.size())
        return false;

    out.data = block.begin();
    return true;
    /*
    if (buffer_size < sizeof(MinimalHeader))
        return false;


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
    return false;*/
}

uint16_t ntdcp::package_size(const Package& out)
{
    /// Temporal trivial implementation
    return sizeof(Package) + out.size;
}

void ntdcp::write_package(const Package& package, SerialWriteAccessor& write_to)
{
    /// Temporal trivial implementation
    write_to.put_copy(package);
    write_to.put(package.data, package.size);
}
