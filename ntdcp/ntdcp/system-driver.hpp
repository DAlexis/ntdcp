#pragma once

#include "ntdcp/utils.hpp"
#include <chrono>
#include <cstdint>

namespace ntdcp
{

class IMutex
{
public:
    virtual ~IMutex() = default;
    virtual void lock() = 0;
    virtual void unlock() = 0;
};

class SystemDriver : public PtrAliases<SystemDriver>
{
public:
    virtual ~SystemDriver() = default;
    virtual uint32_t random() = 0;
    virtual uint32_t random_nonzero();
    virtual std::chrono::steady_clock::time_point now() const = 0;
    virtual std::unique_ptr<IMutex> create_mutex() = 0;
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
