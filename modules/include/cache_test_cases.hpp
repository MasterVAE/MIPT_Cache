
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstddef>

#include "cache.hpp"

#include "2Q_cache.hpp"
#include "arc_cache.hpp"
#include "lfu_cache.hpp"
#include "lirs_cache.hpp"

using TestCache = CacheAPI<int, int>;
using CacheFactory = std::function<std::unique_ptr<TestCache>(std::size_t)>;

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
        [](std::size_t capacity) -> std::unique_ptr<TestCache>
        {
            return std::make_unique<
                lirs_cache::lirs_cache<int, int>>(capacity);
        }
    });


    caches.push_back({
        "ARC",
        [](std::size_t capacity) -> std::unique_ptr<TestCache>
        {
            return std::make_unique<arc_cache::arc_cache<int, int>>(capacity);
        }
    });
        caches.push_back({
        "2Q",
        [](std::size_t capacity) -> std::unique_ptr<TestCache>
        {
            return std::make_unique<cache_2Q::cache_2Q_<int, int>>(capacity);
        }
        
    });
        caches.push_back({
        "LFU",
        [](std::size_t capacity) -> std::unique_ptr<TestCache>
        {
            return std::make_unique<lfu_cache::lfu_cache<int, int>>(capacity);
        }
    });
        // Аналогично добавляется ARC.

    return caches;
}