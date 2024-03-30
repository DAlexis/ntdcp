#include "ntdcp/system-driver.hpp"

using namespace ntdcp;

uint32_t SystemDriver::random_nonzero()
{
    uint32_t result = 0;
    while (result == 0)
        result = random();
    return result;
}
