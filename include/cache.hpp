#pragma once

#include <vector>
#include <list>
#include <unordered_map>
#include <memory>
#include <cstddef>
#include <iostream>
#include <limits>
#include <set>
#include <cstddef>
#include <utility>
#include <iterator>



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
static int SimulatePerfectCache(const std::vector<Value>& requests,
                         size_t capacity,
                         std::vector<Value>& misses) {
    if (capacity == 0) {
        misses = requests;
        return static_cast<int>(requests.size());
    }

    const size_t n = requests.size();
    const size_t INF = n + 1;

    std::vector<size_t> next(n, INF);
    std::unordered_map<Value, size_t> last;
    for (size_t i = n; i-- > 0; ) {
        auto it = last.find(requests[i]);
        if (it != last.end()) {
            next[i] = it->second;
        }
        last[requests[i]] = i;
    }

    std::unordered_map<Value, size_t> cache;

    std::set<std::pair<size_t, Value>> evictSet;

    int missCount = 0;

    for (size_t i = 0; i < n; ++i) {
        const Value& val = requests[i];
        auto it = cache.find(val);

        if (it != cache.end()) {
            size_t oldNext = it->second;
            evictSet.erase({oldNext, val});
            it->second = next[i];
            evictSet.insert({next[i], val});
        } else {
            ++missCount;
            misses.push_back(val);

            if (cache.size() == capacity) {
                auto evictIt = std::prev(evictSet.end());
                Value evictVal = evictIt->second;
                cache.erase(evictVal);
                evictSet.erase(evictIt);
            }

            cache[val] = next[i];
            evictSet.insert({next[i], val});
        }
    }

    return missCount;
}


template <typename Value>
int RunPerfectCache(std::vector<size_t> capacity, std::vector<Value> values) {
    int totalMisses = 0;
    std::vector<Value> requests = std::move(values);

    for (size_t cap : capacity) {
        if (requests.empty()) {
            break;
        }

        std::vector<Value> misses;
        totalMisses += SimulatePerfectCache(requests, cap, misses);
        requests = std::move(misses);
    }

    return totalMisses;
}