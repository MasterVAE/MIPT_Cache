#include <stdio.h>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <memory>

#include "config.h"
#include "cache.h"

#if 0

#include "lirs_cache.hpp"
#include "2Q_cache.hpp"

#endif

template <typename T>
struct Cache_LRU : CacheFunc<T>
{
    size_t size;

    std::list<T> values;

    std::unordered_map<T, typename std::list<T>::iterator> cache;

    Cache_LRU(size_t size) : size(size) {}

    int GetValue(T value) override
    {
        auto it = cache.find(value);

        if (it != cache.end())
        {
            values.splice(values.begin(), values, it->second);

            it->second = values.begin();

            return 0;
        }

        int miss = 0;

        if (this->next != NULL)
            miss += this->next->GetValue(value);

        miss += 1;

        values.push_front(value);
        cache[value] = values.begin();

        if (values.size() > size)
        {
            T old = values.back();
            values.pop_back();
            cache.erase(old);
        }

        return miss;
    }
};

template <typename T>
Cache<T>* CreateCache(std::unique_ptr<Config> config)
{
    if(!config) return nullptr;

    Cache<T>* cache = new Cache<T>;
    if(!cache) return nullptr;

    cache->layers_count = config->layers.size();
    cache->layers = std::vector<CacheFunc<T>*>(cache->layers_count);

    for(size_t i = 0; i < cache->layers_count; i++)
    {
        switch (config->layers[i].type)
        {
        #if 0
        case LAYER_ARC:
            cache->layers[i] = new Cache_ARC(config->layers[i].size);;
            break;
        case LAYER_2Q:
            cache->layers[i] = new Cache_2Q(config->layers[i].size);;
            break;
        case LAYER_LFU:
            cache->layers[i] = new Cache_LFU(config->layers[i].size);;
            break;
        #endif
        case LAYER_LRU:
            cache->layers[i] = new Cache_LRU<T>(config->layers[i].size);
            break;
        #if 0
        case LAYER_LIRS:
            cache->layers[i] = new Cache_LIRS(config->layers[i].size);;
            break;
        #endif

        default:
            break;
        }
    }

    for(size_t i = 0; i < cache->layers_count - 1; i++)
    {
        cache->layers[i]->next = cache->layers[i + 1];
    }
    cache->layers[cache->layers_count - 1]->next = NULL;

    return cache;
}

template <typename T>
int RunCache(Cache<T>* cache, size_t count, std::vector<T> values)
{
    if(!cache) return -1;

    size_t miss = 0;

    for(size_t i = 0; i < count; i++)
    {
        int j;
        miss += cache->layers[0]->GetValue(values[i]);
    }

    return (int)miss;
}