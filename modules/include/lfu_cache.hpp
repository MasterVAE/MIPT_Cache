#pragma once

#include "cache.hpp"

#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

namespace lfu_cache {

template <typename KeyT, typename Value>
class lfu_cache : public CacheAPI<KeyT, Value> {
public:
    explicit lfu_cache(
        std::size_t sz,
        CacheAPI<KeyT, Value>* next = nullptr
    )
        : CacheAPI<KeyT, Value>(next),
          cache_sz_(sz),
          min_freq_(0) {}

    CacheResult<int, int> Request(KeyT key) override {
        int misses_below = 0;
        Value result_data;

        auto fetch_from_next = [&](KeyT k) -> Value {
            if (this->next_cache_level != nullptr) {
                auto res = this->next_cache_level->Request(k);
                misses_below = res.misses;

                return res.data;
            }

            return static_cast<Value>(k);
        };

        if (LookUpUpdate(key, fetch_from_next, result_data)) {
            return {result_data, 0};
        }
        return  {result_data, 1 + misses_below};
    }

private:
    struct Node {
        Value val;
        std::size_t frequent_count;
        typename std::list<KeyT>::iterator it;

        Node(Value value, std::size_t frequency,
             typename std::list<KeyT>::iterator pos = {})
            : val(std::move(value)), frequent_count(frequency), it(pos) {}
    };

    std::size_t cache_sz_;
    std::size_t min_freq_;

    std::unordered_map<KeyT, Node> hash_;
    std::unordered_map<std::size_t, std::list<KeyT>> freq_map_;

    bool isCacheFull() const {
        return hash_.size() >= cache_sz_;
    }

    void UpdateFreq(KeyT key) {
        auto hit_it = hash_.find(key);
        std::size_t freq = hit_it->second.frequent_count;

        freq_map_[freq].erase(hit_it->second.it);
        if (freq_map_[freq].empty()) {
            freq_map_.erase(freq);
            if (freq == min_freq_) {
                ++min_freq_;
            }
        }

        ++hit_it->second.frequent_count;
        std::size_t new_freq = hit_it->second.frequent_count;

        freq_map_[new_freq].push_front(key);
        hit_it->second.it = freq_map_[new_freq].begin();
    }

template <typename F>
    bool LookUpUpdate(KeyT key, F slow_get_page, Value& out_val) {
        if (cache_sz_ == 0) {
            out_val = slow_get_page(key);
            return false;
        }

        auto hit_it = hash_.find(key);

        if (hit_it != hash_.end()) {
            out_val = hit_it->second.val; 
            UpdateFreq(key);
            return true;
        }

        if (isCacheFull()) {
            KeyT evicted_key = freq_map_[min_freq_].back();
            freq_map_[min_freq_].pop_back();

            if (freq_map_[min_freq_].empty()) {
                freq_map_.erase(min_freq_);
            }
            hash_.erase(evicted_key);
        }

        out_val = slow_get_page(key); 
        min_freq_ = 1;
        
        freq_map_[min_freq_].push_front(key);
        hash_.emplace(
            key, Node(out_val, min_freq_, freq_map_[min_freq_].begin())
        );
        
        return false;
    }
};

} // namespace lfu_cache
