#pragma once

#include <cstdint>

namespace ntdcp
{

class ISystemDriver
{
public:
    virtual ~ISystemDriver() = default;
    virtual uint32_t random() const = 0;
};

}
