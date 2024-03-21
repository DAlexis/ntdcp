#pragma once

#include <set>
#include <map>
#include <list>
#include <optional>
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
            it->second = std::prev(m_list.end());
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

template<typename KeyType, typename ValueType>
class CachingMap
{
public:
    CachingMap(size_t size) :
        m_size(size)
    {}

    bool erase(const KeyType& key)
    {
        auto it = m_map.find(key);
        if (it == m_map.end())
            return false;

        auto list_iter = it->second.first;
        m_list.erase(list_iter);
        m_map.erase(it);
        return true;
    }

    std::optional<ValueType*> get(const KeyType& key)
    {
        auto it = m_map.find(key);
        if (it == m_map.end())
            return std::nullopt;
        return &it->second.second;
    }

    std::optional<ValueType*> get_update(const KeyType& key)
    {
        auto it = m_map.find(key);
        if (it == m_map.end())
            return std::nullopt;

        auto list_iter = it->second.first;
        m_list.push_back(*list_iter);
        m_list.erase(list_iter);
        it->second.first = std::prev(m_list.end());
        return &it->second.second;
    }

    /**
     * @brief Update or put new value to a map
     * @param key
     * @param value
     * @return true, if key already existed and false if object is new
     */
    bool put_update(const KeyType& key, const ValueType& value)
    {
        auto it = m_map.find(key);
        if (it != m_map.end())
        {
            auto list_iter = it->second.first;
            m_list.push_back(*list_iter);
            m_list.erase(list_iter);
            it->second.first = std::prev(m_list.end());
            it->second.second = value;
            return true;
        }

        if (m_list.size() == m_size)
        {
            const KeyType* k = m_list.front();
            m_list.pop_front();
            m_map.erase(*k);
        }

        auto emp = m_map.emplace(key, std::make_pair(m_list.end(), value));
        m_list.push_back(&emp.first->first);
        emp.first->second.first = std::prev(m_list.end());
        return false;
    }


private:
    using ListType = std::list<const KeyType*>;

    ListType m_list;
    std::map<KeyType, std::pair<typename ListType::iterator, ValueType>> m_map;

    size_t m_size;
};
