
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cache.hpp"

using TestCache = CacheAPI<int, int>;
using CacheFactory = std::function<std::unique_ptr<TestCache>(size_t)>;

struct CacheImplementation
{
    std::string name;
    CacheFactory create;
};

inline std::vector<CacheImplementation> GetCacheImplementations()
{
    std::vector<CacheImplementation> caches;

    caches.push_back({
        "LIRS",
        [](size_t capacity) -> std::unique_ptr<TestCache>
        {
            return std::make_unique<
                lirs_cache::lirs_cache<int, int>
            >(capacity);
        }
    });

    // Когда алгоритмы будут реализованы и будут наследоваться от CacheAPI,
    // добавь их сюда по аналогии:
    //
    // caches.push_back({
    //     "LRU",
    //     [](size_t capacity) -> std::unique_ptr<TestCache>
    //     {
    //         return std::make_unique<LRUCache<int, int>>(capacity);
    //     }
    // });
    //
    // caches.push_back({
    //     "2Q",
    //     [](size_t capacity) -> std::unique_ptr<TestCache>
    //     {
    //         return std::make_unique<TwoQCache<int, int>>(capacity);
    //     }
    // });
    //
    // caches.push_back({
    //     "LFU",
    //     [](size_t capacity) -> std::unique_ptr<TestCache>
    //     {
    //         return std::make_unique<LFUCache<int, int>>(capacity);
    //     }
    // });
    //
    // Аналогично добавляется ARC.

    return caches;
}