#include <stdio.h>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <memory>

#include "config.h"
#include "cache.h"
#include "lirs_cache.hpp"
#include "2Q_cache.hpp"

struct Cache_LRU : CacheFunc
{
    size_t size;

    std::list<VALUE> values;

    std::unordered_map<VALUE, std::list<VALUE>::iterator> cache;

    Cache_LRU(size_t size) : size(size) {}

    int GetValue(VALUE value) override
    {
        auto it = cache.find(value);

        if (it != cache.end())
        {
            values.splice(values.begin(), values, it->second);
           
            it->second = values.begin();

            return 0;
        }

        int miss = 0;

        if (next != NULL)
            miss += next->GetValue(value);

        miss += 1;

        values.push_front(value);
        cache[value] = values.begin();

        if (values.size() > size)
        {
            VALUE old = values.back();
            values.pop_back();
            cache.erase(old);
        }

        return miss;
    }
};

Cache* CreateCache(std::unique_ptr<Config> config)
{
    if(!config) return NULL;// nullptr 

    Cache* cache = (Cache*)calloc(1, sizeof(Cache));
    if(!cache) return NULL;

    cache->layers_count = config->layers.size();// какая-то поебень, убрать все аллокации calloc
    cache->layers = (CacheFunc**)calloc(cache->layers_count, sizeof(CacheFunc*));//

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
            cache->layers[i] = new Cache_LRU(config->layers[i].size);;
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

void DestroyCache(Cache* cache)
{
    if(!cache) return;

    for(size_t i = 0; i < cache->layers_count; i++)
    {
        delete cache->layers[i];
    }
    free(cache->layers);
    free(cache);
}

int RunCache(Cache* cache, size_t count, VALUE* numbers)
{
    if(!cache) return -1;

    size_t miss = 0;

    for(size_t i = 0; i < count; i++)
    {
        int j;
        miss += cache->layers[0]->LookUpUpdate();
    }

    return (int)miss;
}