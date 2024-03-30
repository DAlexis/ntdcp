#pragma once

#include "ntdcp/system-driver.hpp"
#include <mutex>

namespace ntdcp {

class StdMutex : public IMutex
{
public:
    void lock() override;
    void unlock() override;
private:
    std::mutex m_mutex;
};

class SystemDriverDeterministic : public SystemDriver
{
public:
    uint32_t random() override;
    std::chrono::steady_clock::time_point now() const override;
    std::unique_ptr<IMutex> create_mutex() override;

    void increment_time(std::chrono::milliseconds dt);

private:
    uint32_t m_next_random = 1;
    std::chrono::steady_clock::time_point m_current_time;
};

class TransmissionMedium;

class VirtualPhysicalInterface : public IPhysicalInterface, public std::enable_shared_from_this<VirtualPhysicalInterface>
{
public:
    static std::shared_ptr<VirtualPhysicalInterface> create(PhysicalInterfaceOptions opts, SystemDriver::ptr sys, std::shared_ptr<TransmissionMedium> medium);

    SerialReadAccessor& incoming() override;
    void send(Buffer::ptr data) override;
    bool busy() const override;
    const PhysicalInterfaceOptions& options() const override;

    void receive_from_medium(Buffer::ptr data);

private:
    VirtualPhysicalInterface(PhysicalInterfaceOptions opts, SystemDriver::ptr sys, std::shared_ptr<TransmissionMedium> medium);

    PhysicalInterfaceOptions m_opts;
    SystemDriver::ptr m_sys;
    std::chrono::steady_clock::time_point m_last_tx;
    std::shared_ptr<TransmissionMedium> m_medium;
    RingBuffer m_data;
};

class TransmissionMedium : public PtrAliases<TransmissionMedium>
{
public:
    void add_client(std::shared_ptr<VirtualPhysicalInterface> client);
    void send(Buffer::ptr data, std::shared_ptr<VirtualPhysicalInterface> sender);
    bool& broken();

private:
    std::vector<std::weak_ptr<VirtualPhysicalInterface>> m_clients;
    bool m_broken = false;
};

}
