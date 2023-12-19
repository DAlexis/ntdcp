#pragma once

#include "ntdcp/utils.hpp"
#include <chrono>
#include <cstdint>

namespace ntdcp
{

class ISystemDriver : public PtrAliases<ISystemDriver>
{
public:
    virtual ~ISystemDriver() = default;
    virtual uint32_t random() = 0;
    virtual std::chrono::steady_clock::time_point now() const = 0;
};

struct PhysicalInterfaceOptions
{
    enum class DuplexType
    {
        simplex, half_duplex, duplex
    };

    DuplexType duplex_type = DuplexType::duplex;
    std::chrono::milliseconds tx_to_rx_time{0};
    std::chrono::milliseconds tx_time{0};
    bool retransmit_back = false;
    int ring_buffer_size = 1024;
};

class IPhysicalInterface : public PtrAliases<IPhysicalInterface>
{
public:
    virtual SerialReadAccessor& incoming() = 0;
    virtual void send(Buffer::ptr data) = 0;
    virtual bool busy() const = 0;
    virtual const PhysicalInterfaceOptions& options() const = 0;
};

}
