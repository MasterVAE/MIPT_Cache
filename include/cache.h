#pragma once

#include <vector>

#include "config.h"

template <typename T>
struct CacheFunc
{
    CacheFunc<T>* next;

    virtual ~CacheFunc() = default;
    virtual int GetValue(T) = 0;
};

template <typename T>
struct Cache
{
    size_t layers_count;
    std::vector<CacheFunc<T>*> layers;
};

template <typename T>
Cache<T>* CreateCache(std::unique_ptr<Config>);

template <typename T>
void DestroyCache(Cache<T>*);

template <typename T>
int RunCache(Cache<T>* cache, size_t count, std::vector<T> values);