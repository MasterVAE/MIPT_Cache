
#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>

#include "cache.hpp"
#include "config.h"

#include "2Q_cache.hpp"
#include "arc_cache.hpp"
#include "lfu_cache.hpp"
#include "lirs_cache.hpp"


template <typename KeyT, typename Value>
CacheAPI<KeyT, Value>* CreateCache(std::unique_ptr<Config> config) {
    
    assert(config);

    std::unique_ptr<CacheAPI<KeyT, Value>> current_top;

    for (std::size_t i = config->layers.size(); i-- > 0;) {
        const auto& layer = config->layers[i];

        switch (layer.type) {
            case LAYER_LFU:
                current_top = std::make_unique<lfu_cache::lfu_cache<KeyT, Value>>(
                    layer.size, std::move(current_top));
                break;

            case LAYER_ARC:
                current_top = std::make_unique<arc_cache::arc_cache<KeyT, Value>>(
                    layer.size, std::move(current_top));
                break;

            case LAYER_2Q:
                current_top = std::make_unique<cache_2Q::cache_2Q_<KeyT, Value>>(
                    layer.size, std::move(current_top));
                break;

            case LAYER_LIRS:
                current_top = std::make_unique<lirs_cache::lirs_cache<KeyT, Value>>(
                    layer.size, std::move(current_top));
                break;

            default:
                continue;
        }
    }

    return current_top.release();
}
