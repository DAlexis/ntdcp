#pragma once

#include "ntdcp/system-driver.hpp"
#include <queue>
#include <mutex>
#include <optional>

namespace ntdcp
{

template<typename T>
class QueueLocking
{
public:
    QueueLocking(SystemDriver& system_driver, size_t max_size = 10) :
        m_max_size(max_size)
    {
        m_mutex = system_driver.create_mutex();
    }

    size_t size() const
    {
        return m_queue.size();
    }

    bool empty() const
    {
        return size() == 0;
    }

    bool full() const
    {
        return size() >= m_max_size;
    }

    bool push(const T& obj)
    {
        std::unique_lock<IMutex> lck(*m_mutex);
        if (m_queue.size() >= m_max_size)
            return false;
        m_queue.push(obj);
        return true;
    }

    std::optional<T> pop()
    {
        std::unique_lock<IMutex> lck(*m_mutex);
        if (m_queue.empty())
            return std::nullopt;
        T front = m_queue.front();
        m_queue.pop();
        return front;
    }

private:
    std::unique_ptr<IMutex> m_mutex;
    size_t m_max_size;
    std::queue<T> m_queue;
};

}
