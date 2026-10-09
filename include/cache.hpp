#pragma once

#include <vector>
#include <list>
#include <unordered_map>
#include <memory>
#include <cstddef>



template <typename KeyT, typename Value>
struct CacheAPI
{
    CacheAPI<KeyT, Value>* next_cache_level;

    CacheAPI(CacheAPI<KeyT, Value>* next = nullptr) : next_cache_level(next) {}

    virtual ~CacheAPI() = default;
    virtual bool Request(KeyT) = 0;

    CacheAPI(const CacheAPI&) = delete;
    CacheAPI& operator=(const CacheAPI&) = delete;
};

#include "lirs_cache.hpp"
#include "config.h"

template <typename KeyT, typename Value>
CacheAPI<KeyT, Value>* CreateCache(std::unique_ptr<Config> config){
    assert(config);

    CacheAPI<KeyT, Value>* current_top = nullptr;
    size_t sz = config->layers.size();

    for (size_t cache_level = static_cast<size_t>(sz) - 1; cache_level >= 0; --cache_level) {

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