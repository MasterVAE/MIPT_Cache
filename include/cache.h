#ifndef CACHE_H
#define CACHE_H

#include "config.h"

struct CacheFunc
{
    CacheFunc* next;

    virtual ~CacheFunc() = default; 
    virtual VALUE GetValue(VALUE value) = 0;
};

struct Cache
{
    size_t layers_count;
    CacheFunc** layers;
};

Cache* CreateCache(Config* config);
void DestroyCache(Cache* cache);

int RunCache(Cache* cache, size_t count, int* numbers);

#endif //CACHE_H