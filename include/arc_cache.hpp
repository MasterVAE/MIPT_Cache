#pragma once

#include <list>
#include <unordered_map>

namespace arc_cache {
    template <typename KeyT, typename Value>
    class arc_cache : public CacheAPI<KeyT, Value>{

        size_t cache_sz_;
        
        public:
            arc_cache (size_t sz, CacheAPI<KeyT, Value>* next_cache_level = nullptr) : 
            cache_sz_{sz}, CacheAPI<KeyT, Value>(next_cache_level) {}

            std::list<KeyT> recent_list;
            std::list<KeyT> frequent_list;
            std::list<KeyT> recent_history;
            std::list<KeyT> frequent_history;
            
            enum class listIt { 
                T1_RECENT, 
                T2_FREQUENT, 
                B1_RECENT_GHOST, 
                B2_FREQUENT_GHOST 
            };

            struct node {
                Value val;
                listIt state;

                typename std::list<KeyT>::iterator it;  
                
                node (Value v, listIt s, typename std::list<KeyT>::iterator i = {})
                    :val(v), state(s), it(i) {} 
            };

            std::unordered_map<KeyT, node> hash_;

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

            bool isCacheFull()           { return (recent_list.size() + frequent_list.size()) >= cache_sz_; }
            bool isRecentHistoryFull()   { return recent_history.size() >= cache_sz_; }
            bool isFrequentHistoryFull() { return frequent_history.size() >= cache_sz_; }
            
            size_t target_recent_size = 0;

            void Replace() {
                auto evicted_recent_key = hash_.find(recent_list.back());
                if (recent_list.size() > target_recent_size) {
                    if (isRecentHistoryFull()) {
                        recent_history.pop_back();
                        hash_.erase(evicted_recent_key); 
                    }
                recent_history.splice(recent_history.begin(), recent_list, 
                                                      evicted_recent_key->second.it);
                
                evicted_recent_key->second.state = listIt::B1_RECENT_GHOST;
                evicted_recent_key->second.val = Value{};
                } else {
                auto evicted_frequent_key = hash_.find(frequent_list.back());
                    if (isFrequentHistoryFull()) {
                        frequent_history.pop_back();
                        hash_.erase(evicted_frequent_key); 
                    }

                    frequent_history.splice(frequent_history.begin(), frequent_list, 
                                                            evicted_frequent_key->second.it);

                    evicted_frequent_key->second.state = listIt::B2_FREQUENT_GHOST;
                    evicted_frequent_key->second.val = Value{};
                }
            }

        template <typename F>
        bool LookUpUpdate(KeyT key, F slow_get_page){
            
            auto hit_it = hash_.find(key);

            if (hit_it != hash_.end()) {
                if (hit_it->second.state == listIt::T1_RECENT) {
                    hit_it->second.state = listIt::T2_FREQUENT;
                    frequent_list.splice(frequent_list.begin(), recent_list, hit_it->second.it);

                    return true;
                } 
                
                if (hit_it->second.state == listIt::T2_FREQUENT) {
                    frequent_list.splice(frequent_list.begin(),frequent_list, hit_it->second.it);
                    return true; 
                }

                if (hit_it->second.state == listIt::B1_RECENT_GHOST) {
                    size_t delta = std::max<size_t>(1, frequent_list.size() / 
                                   std::max<size_t>(1, recent_list.size()));        
                    target_recent_size = std::min(cache_sz_, target_recent_size + delta);

                    if (isCacheFull()) {
                        Replace();
                    }

                    hit_it->second.state = listIt::T2_FREQUENT;  
                    frequent_list.splice(frequent_list.begin(), recent_history, hit_it->second.it);
                    hit_it->second.val = slow_get_page(key);     

                    return false;
                }

                if (hit_it->second.state == listIt::B2_FREQUENT_GHOST) {
                    size_t delta = std::max<size_t>(1, recent_list.size() / 
                                   std::max<size_t>(1,frequent_list.size()));  

                    target_recent_size = (target_recent_size >= delta) ? (target_recent_size - delta) : 0;
                    if (isCacheFull()) {
                        Replace();
                    }

                    hit_it->second.state = listIt::T2_FREQUENT;  
                    frequent_list.splice(frequent_list.begin(), frequent_history, hit_it->second.it);
                    hit_it->second.val = slow_get_page(key);   

                    return false;
                }
            }

            Value fresh_data  = slow_get_page(key);

            if (isCacheFull()) {
                Replace();
            }
            recent_list.push_front(key);

            node new_node(fresh_data, listIt::T1_RECENT, recent_list.begin());
            
            hash_.emplace(key, new_node);

            return false;
        }
    }; 
}