#pragma once

#include <set>
#include <map>
#include <list>
#include <cstdlib>


template<typename T>
class CachingSet
{
public:
    CachingSet(size_t size) :
        m_size(size)
    {}

    bool check_update(const T& obj)
    {
        auto it = m_map.find(obj);
        if (it != m_map.end())
        {
            // update list
            auto list_it = it->second;
            m_list.erase(list_it);
            m_list.push_back(&it->first);
            return true;
        }

        if (m_map.size() == m_size)
        {
            m_map.erase(*m_list.front());
            m_list.pop_front();
        }

        auto jt = m_map.emplace(obj, m_list.end());
        m_list.push_back(&jt.first->first);
        jt.first->second = std::prev(m_list.end());
        return false;
    }

private:
    using ListType = std::list<const T*>;

    ListType m_list;
    std::map<T, typename ListType::iterator> m_map;

    size_t m_size;
};
