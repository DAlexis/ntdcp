#include "ntdcp/virtual-device.hpp"

using namespace ntdcp;

void StdMutex::lock()
{
    m_mutex.lock();
}

void StdMutex::unlock()
{
    m_mutex.unlock();
}

uint32_t SystemDriverDeterministic::random()
{
    if (m_next_random == 0)
        m_next_random++;
    return m_next_random++;
}

std::chrono::steady_clock::time_point SystemDriverDeterministic::now() const
{
    return m_current_time;
}

std::unique_ptr<IMutex> SystemDriverDeterministic::create_mutex()
{
    return std::make_unique<StdMutex>();
}

void SystemDriverDeterministic::increment_time(std::chrono::milliseconds dt)
{
    m_current_time += dt;
}

VirtualPhysicalInterface::VirtualPhysicalInterface(PhysicalInterfaceOptions opts, ISystemDriver::ptr sys, std::shared_ptr<TransmissionMedium> medium) :
    m_opts(opts), m_sys(sys), m_medium(medium), m_data(opts.ring_buffer_size)
{
}

std::shared_ptr<VirtualPhysicalInterface> VirtualPhysicalInterface::create(PhysicalInterfaceOptions opts, ISystemDriver::ptr sys, std::shared_ptr<TransmissionMedium> medium)
{
    auto result = std::shared_ptr<VirtualPhysicalInterface>(new VirtualPhysicalInterface(opts, sys, medium));
    medium->add_client(result);
    return result;
}

SerialReadAccessor& VirtualPhysicalInterface::incoming()
{
    return m_data;
}

void VirtualPhysicalInterface::send(Buffer::ptr data)
{
    m_last_tx = m_sys->now();
    m_medium->send(data, shared_from_this());
}

bool VirtualPhysicalInterface::busy() const
{
    return m_sys->now() - m_last_tx < m_opts.tx_time;
}

const PhysicalInterfaceOptions& VirtualPhysicalInterface::options() const
{
    return m_opts;
}

void VirtualPhysicalInterface::receive_from_medium(Buffer::ptr data)
{
    if (m_sys->now() - m_last_tx < m_opts.tx_to_rx_time)
        return;

    m_data.put(data);
}


void TransmissionMedium::add_client(std::shared_ptr<VirtualPhysicalInterface> client)
{
    m_clients.push_back(client);
}

void TransmissionMedium::send(Buffer::ptr data, std::shared_ptr<VirtualPhysicalInterface> sender)
{
    if (m_broken)
        return;

    for (size_t i = 0; i < m_clients.size(); i++)
    {
        if (auto client = m_clients[i].lock())
        {
            if (client == sender)
                continue;

            client->receive_from_medium(data->clone());
        } else {
            m_clients[i] = m_clients.back();
            m_clients.pop_back();
            i--;
        }
    }
}

void TransmissionMedium::set_broken(bool broken)
{
    m_broken = broken;
}
