
#pragma once

#include <cassert>
#include <cstddef>
#include <memory>

#include "cache.hpp"
#include "config.h"

#include "2Q_cache.hpp"
#include "arc_cache.hpp"
#include "lfu_cache.hpp"
#include "lirs_cache.hpp"


template <typename KeyT, typename Value>
CacheAPI<KeyT, Value>* CreateCache(
    std::unique_ptr<Config> config
) {
    assert(config);

    std::unique_ptr<CacheAPI<KeyT, Value>> current_top;

    for (std::size_t i = config->layers.size(); i-- > 0;) {
        const auto& layer = config->layers[i];

        CacheAPI<KeyT, Value>* next = current_top.get();
        CacheAPI<KeyT, Value>* new_top = nullptr;

        switch (layer.type) {
            case LAYER_LFU:
                new_top = new lfu_cache::lfu_cache<KeyT, Value>(
                    layer.size, next
                );
                break;

            case LAYER_ARC:
                new_top = new arc_cache::arc_cache<KeyT, Value>(
                    layer.size, next
                );
                break;

            case LAYER_2Q:
                new_top = new cache_2Q::cache_2Q_<KeyT, Value>(
                    layer.size, next
                );
                break;

            case LAYER_LIRS:
                new_top = new lirs_cache::lirs_cache<KeyT, Value>(
                    layer.size, next
                );
                break;

            default:
                continue;
        }

        current_top.release();
        current_top.reset(new_top);
    }

    return current_top.release();
}
