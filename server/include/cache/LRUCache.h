#pragma once

#include <list>
#include <unordered_map>
#include <optional>

// 【LRU 缓存模板类】
// K: 键类型 (例如: std::string 用于文件路径)
// V: 值类型 (例如: std::string 用于文件内容)
template<typename K, typename V>
class LRUCache {
public:
    // 构造函数：指定缓存容量
    explicit LRUCache(size_t capacity) : m_capacity(capacity) {}

    // 向缓存中放入数据
    void put(const K& key, const V& value) {
        // 检查 key 是否已存在
        auto it = m_lookup.find(key);
        if (it != m_lookup.end()) {
            // 如果存在，更新值，并将其移动到链表头部
            it->second->second = value;
            m_items.splice(m_items.begin(), m_items, it->second);
            return;
        }

        // 如果不存在，检查容量是否已满
        if (m_items.size() >= m_capacity) {
            // 缓存已满，淘汰最久未使用的数据（链表尾部）
            K lru_key = m_items.back().first;
            m_items.pop_back();
            m_lookup.erase(lru_key);
        }

        // 插入新数据到链表头部
        m_items.emplace_front(key, value);
        m_lookup[key] = m_items.begin();
    }

    // 从缓存中获取数据
    V get(const K& key) {
        auto it = m_lookup.find(key);
        if (it == m_lookup.end()) {
            // 数据不存在
            return V{}; // 返回默认值
        }

        // 数据存在，将其移动到链表头部（标记为最近使用）
        m_items.splice(m_items.begin(), m_items, it->second);
        
        // 返回找到的值
        return it->second->second;
    }

private:
    size_t m_capacity;
    // 双向链表存储 key-value 对，头部为最近使用
    std::list<std::pair<K, V>> m_items;
    // 哈希表用于快速查找，存储 key 到链表迭代器的映射
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> m_lookup;
};