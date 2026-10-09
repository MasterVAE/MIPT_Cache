#pragma once

#include <vector>
#include <list>
#include <unordered_map>
#include <memory>
#include <cstddef>
#include <iostream>
#include <limits>



template <typename KeyT, typename Value>
struct CacheAPI
{
    CacheAPI<KeyT, Value>* next_cache_level;

    CacheAPI(CacheAPI<KeyT, Value>* next = nullptr) : next_cache_level(next) {}

    virtual ~CacheAPI()
    {
        delete next_cache_level;
    }
    virtual bool Request(KeyT) = 0;

    CacheAPI(const CacheAPI&) = delete;
    CacheAPI& operator=(const CacheAPI&) = delete;
};

#include "lirs_cache.hpp"
#include "config.h"

template <typename KeyT, typename Value>
CacheAPI<KeyT, Value>* CreateCache(std::unique_ptr<Config> config) {
    assert(config);

    CacheAPI<KeyT, Value>* current_top = nullptr;
    int sz = config->layers.size();

    for (int cache_level = sz - 1; cache_level >= 0; --cache_level) {
        switch (config->layers[cache_level].type) {
#if 0
        case LAYER_LFU:
            current_top = new Cache_LFU(sz, );
            break;
        case LAYER_ARC:
            current_top = new Cache_LFU<KeyT, Value>(sz, current_top);
            break;
         case LAYER_2Q:
            current_top = new cache_2Q_<KeyT, Value>(sz, current_top);
            break;
#endif
        case LAYER_LIRS:
            current_top = new lirs_cache::lirs_cache<KeyT, Value>(sz, current_top);
            break;
        default:
            break;
        }
    }

    return current_top;
}

template <typename KeyT, typename Value>

int RunCache(CacheAPI<KeyT, Value>* current_top, size_t count, std::vector<Value> values){
    assert(current_top);

    int miss = 0;

    for(size_t i = 0; i < count; i++) {

        miss += current_top->Request(values[i]);
    }

    return miss;
}

template <typename Value>
int RunPerfectCache(size_t capacity, std::vector<Value> values)
{
    if (capacity == 0)
        return static_cast<int>(values.size());

    const size_t n = values.size();

    std::unordered_map<Value, std::vector<size_t>> positions;
    for (size_t i = 0; i < n; ++i)
        positions[values[i]].push_back(i);

    std::unordered_map<Value, size_t> ptr;
    for (auto& kv : positions)
        ptr[kv.first] = 0;

    std::unordered_map<Value, size_t> cache;

    int miss = 0;

    for (size_t i = 0; i < n; ++i)
    {
        const Value& key = values[i];
        size_t& p = ptr[key];

        size_t next_use = std::numeric_limits<size_t>::max();
        if (p + 1 < positions[key].size())
            next_use = positions[key][p + 1];
        ++p;

        auto it = cache.find(key);
        if (it != cache.end())
        {
            it->second = next_use;
        }
        else
        {
            ++miss;

            if (cache.size() >= capacity)
            {
                auto victim = cache.begin();
                for (auto cit = cache.begin(); cit != cache.end(); ++cit)
                {
                    if (cit->second > victim->second)
                        victim = cit;
                }
                cache.erase(victim);
            }

            cache[key] = next_use;
        }
    }

    return miss;
}