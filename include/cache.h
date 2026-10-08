#pragma once

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

Cache* CreateCache(std::unique_ptr<Config>);
void DestroyCache(Cache*);

int RunCache(Cache*, size_t, int*);