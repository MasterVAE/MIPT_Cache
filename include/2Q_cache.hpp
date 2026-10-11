#pragma once 

#include <list>
#include <unordered_map>
#include <algorithm>

namespace cache_2Q {

    template <typename KeyT, typename Value>
    struct cache_2Q_ : public CacheAPI<KeyT, Value> {

        size_t cache_sz_;
        size_t in_sz_;
        size_t out_sz_;

        std::list<KeyT> cache_in_;
        std::list<KeyT> cache_out_;
        std::list<KeyT> cache_hot_;

        cache_2Q_(size_t cache_sz, CacheAPI<KeyT, Value>* next = nullptr) :
                     CacheAPI<KeyT, Value>(next), cache_sz_{cache_sz} {
            
            in_sz_ = std::max<size_t>(1, cache_sz_ / 4); 
            out_sz_ = std::max<size_t>(1, cache_sz_); 
        }

        enum class listIt{ IN_, HOT_, OUT_ };
        
        struct node {
            Value val;
            listIt state;
            typename std::list<KeyT>::iterator it;  
            
            node (Value v, listIt s, typename std::list<KeyT>::iterator i = {})
                :val(std::move(v)), state(s), it(i) {} 
        };
        
        std::unordered_map<KeyT, node> hash_;

        int Request(KeyT key) override {
            int misses_below = 0;
            auto fetch_from_next = [&] (KeyT k) -> Value {
                if (this->next_cache_level != nullptr) {
                     misses_below = this->next_cache_level->Request(k);
                }
                return static_cast<Value>(k);
            };
                 
            bool is_hit = LookUpUpdate(key, fetch_from_next);
            if (is_hit) {
                return 0;
            }
            return 1 + misses_below;
        }

        template <typename F>
        bool LookUpUpdate(KeyT key, F slow_get_page) {
            auto hit_it = hash_.find(key);
            
            if (hit_it != hash_.end()) {
                if (hit_it->second.state == listIt::IN_) { 
                    return true;
                }
                else if (hit_it->second.state == listIt::HOT_) {
                 
                    cache_hot_.splice(cache_hot_.begin(), cache_hot_, hit_it->second.it);
                    return true;
                }
                else if (hit_it->second.state == listIt::OUT_) {
                   
                    cache_out_.erase(hit_it->second.it);

                    EnsureFreeSpace();

                    cache_hot_.push_front(key);

                    hit_it->second.val = slow_get_page(key); 
                    hit_it->second.state = listIt::HOT_;
                    hit_it->second.it = cache_hot_.begin();

                    return false; 
                }
            }

            EnsureFreeSpace();

            Value fresh_data = slow_get_page(key);
            cache_in_.push_front(key);
            hash_.emplace(key, node{fresh_data, listIt::IN_, cache_in_.begin()});

            return false;
        }

    private:
        void EnsureFreeSpace() {
            if (cache_in_.size() + cache_hot_.size() >= cache_sz_) {
                
                if (cache_in_.size() > in_sz_ || cache_hot_.empty()) {
                    KeyT victim = cache_in_.back();
                    cache_in_.pop_back();

                    if (cache_out_.size() >= out_sz_) {
                        hash_.erase(cache_out_.back());
                        cache_out_.pop_back();
                    }
                    cache_out_.push_front(victim);
                    hash_.at(victim).state = listIt::OUT_;
                    hash_.at(victim).it = cache_out_.begin();
                } 
                else {
                    KeyT victim = cache_hot_.back();
                    cache_hot_.pop_back();
                    hash_.erase(victim);
                }
            }
        }
    };
}