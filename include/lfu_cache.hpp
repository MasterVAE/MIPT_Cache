#pragma once

#include "cache.hpp"
#include <list>
#include <unordered_map>
#include <utility>


namespace lfu_cache {
    template <typename KeyT , typename Value>
    class lfu_cache : public CacheAPI<KeyT, Value> {
        size_t cache_sz_;
        size_t min_freq_;

        public:

        lfu_cache (size_t sz, CacheAPI<KeyT, Value>* next_cache_level = nullptr) : 
           cache_sz_(sz), min_freq_(0), CacheAPI<KeyT, Value>(next_cache_level) {}
          struct node {
                    Value val;
                    size_t frequent_count; 

                    using ListIterator = typename std::list<KeyT>::iterator;

                    node() = default;
                    typename std::list<KeyT>::iterator it;  
                
                    node (Value v, size_t f,  typename std::list<KeyT>::iterator i = {})
                    :val(std::move(v)), frequent_count(f), it(i) {} 
                };

                std::unordered_map<KeyT, node> hash_;
                std::unordered_map<size_t, std::list<KeyT>> freq_map_;

                   int Request(KeyT key) override {
                    size_t misses = 0;
                    auto fetch_from_next = [&] (KeyT k) -> Value {
                        if (this->next_cache_level != nullptr) {
                          
                             misses = this->next_cache_level->Request(k);
                        }
                            return static_cast<Value>(k);
                    };
                    
                    bool is_hit = LookUpUpdate(key, fetch_from_next);
                    if (is_hit) {
                        return 0;
                    }
                    
                    return 1 + misses;
                }

                bool isCacheFull() { return hash_.size() >= cache_sz_; }
                void UpdateFreq(KeyT key){
                    auto hit_it = hash_.find(key);
                    size_t freq = hit_it->second.frequent_count;
                    auto freq_it = hit_it->second.it;
                    freq_map_[freq].erase(freq_it);
                                                       
                    if (freq_map_[freq].empty()) {
                        freq_map_.erase(freq);

                        if (freq == min_freq_) {
                            ++min_freq_;
                        }
                    }

                    ++(hit_it->second.frequent_count);
                    
                    auto new_freq = hit_it->second.frequent_count; 
                    freq_map_[new_freq].push_front(key);
                    hit_it->second.it = freq_map_[new_freq].begin();
                }

        template <typename F>
        bool LookUpUpdate(KeyT key, F slow_get_page){
            auto hit_it = hash_.find(key);
            
            if (hit_it != hash_.end()) {
                UpdateFreq(key);
                return true;
            }
            if (isCacheFull()) {
                auto evicted_key = freq_map_[min_freq_].back();

                freq_map_[min_freq_].pop_back();

                if (freq_map_[min_freq_].empty()) {
                    freq_map_.erase(min_freq_);
                }

                hash_.erase(evicted_key);
            }
            
            Value fresh_data = slow_get_page(key);
            min_freq_ = 1;
            freq_map_[min_freq_].push_front(key);

            node new_node{fresh_data, min_freq_, freq_map_[min_freq_].begin()};
          
            hash_.emplace(key, std::move(new_node));
            return false;
        }
    };
};