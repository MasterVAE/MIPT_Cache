#include <stdio.h>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <memory>

#include "config.h"
#include "cache.h"


struct Cache_LRU : CacheFunc
{
    size_t size;

    std::list<VALUE> values;

    std::unordered_map<VALUE, std::list<VALUE>::iterator> cache;

    Cache_LRU(size_t size) : size(size) {}

    int GetValue(VALUE value) override
    {
        auto it = cache.find(value);

        if (it != cache.end())
        {
            values.splice(values.begin(), values, it->second);
           
            it->second = values.begin();

            return 0;
        }

        int miss = 0;

        if (next != NULL)
            miss += next->GetValue(value);

        miss += 1;

        values.push_front(value);
        cache[value] = values.begin();

        if (values.size() > size)
        {
            VALUE old = values.back();
            values.pop_back();
            cache.erase(old);
        }

        return miss;
    }
};

Cache* CreateCache(std::unique_ptr<Config> config)
{
    if(!config) return NULL;

    Cache* cache = (Cache*)calloc(1, sizeof(Cache));
    if(!cache) return NULL;

    cache->layers_count = config->layers.size();
    cache->layers = (CacheFunc**)calloc(cache->layers_count, sizeof(CacheFunc*));

    for(size_t i = 0; i < cache->layers_count; i++)
    {
        switch (config->layers[i].type)
        {
        #if 0
        case LAYER_ARC:
            cache->layers[i] = new Cache_ARC(config->layers[i].size);;
            break;
        case LAYER_2Q:
            cache->layers[i] = new Cache_2Q(config->layers[i].size);;
            break;
        case LAYER_LFU:
            cache->layers[i] = new Cache_LFU(config->layers[i].size);;
            break;
        #endif
        case LAYER_LRU:
            cache->layers[i] = new Cache_LRU(config->layers[i].size);;
            break;
        #if 0
        case LAYER_LIRS:
            cache->layers[i] = new Cache_LIRS(config->layers[i].size);;
            break;
        #endif

        default:
            break;
        }
    }

    for(size_t i = 0; i < cache->layers_count - 1; i++)
    {
        cache->layers[i]->next = cache->layers[i + 1];
    }
    cache->layers[cache->layers_count - 1]->next = NULL;

    return cache;
}

template <typename KeyT, typename Value>
    
        class lirs_cache : CacheFunc{
            private:
                size_t     cache_sz_;
                size_t     stack_sz_;
                size_t     queue_sz_;
                size_t    lir_count_;
                size_t lir_space_sz_;
                
            public:
                lirs_cache(size_t cache_sz_) : cache_sz_(cache_sz_) {
                    queue_sz_ = std::max<size_t>(1, cache_sz_ / 100);
                    stack_sz_ = std::max<size_t>(1, cache_sz_ * 3);
                    lir_space_sz_ = std::max<size_t>(1, (cache_sz_ * 99) / 100);
                    
                    lir_count_ = 0;
                }
                enum class ListIt {RES_HIR, LIR, NON_RES_HIR};

                struct node {
                    Value val;
                    ListIt  state; 
                    bool is_ghost = false;
                    bool in_stack = false;
                    bool in_queue = false;

                    using ListIterator = typename std::list<KeyT>::iterator;
                    ListIterator iter_Q = {};
                    ListIterator iter_S = {};

                    node() = default;

                    node (Value v, ListIt s = ListIt::RES_HIR)
                    :val(std::move(v)), state(s) {} 
                };

                std::list<KeyT> stack;
                std::list<KeyT> queue;
                
                   void PruneStack(){
                    
                    while (!stack.empty()) {
                        
                        auto bottom_key = stack.front();
                        auto it_hash = hash_.find(bottom_key);

                        if (it_hash->second.state == ListIt::LIR) {
                            break;
                        }

                        it_hash->second.in_stack = false;

                        if (it_hash->second.state == ListIt::NON_RES_HIR) {
                            hash_.erase(it_hash);
                        }
                        stack.pop_front();  
                    }    
                }

                void DemoteBottomLir() {
                    auto stack_bottom = stack.begin();
                    auto it = hash_.find(*stack_bottom);

                    if (it->second.state == ListIt::LIR) {

                        it->second.state = ListIt::RES_HIR;
                        it->second.in_stack = false;
                        it->second.in_queue = true;

                        queue.splice(queue.end(), stack, it->second.iter_S);
                        it->second.iter_Q = std::prev(queue.end());

                        PruneStack();

                        if (isQueueFull()) {
                            auto displaced_hir_key = queue.front();
                            auto displaced_hir_it = hash_.find(displaced_hir_key);

                            displaced_hir_it->second.in_queue = false;
                            queue.pop_front();
                            
                            if (displaced_hir_it->second.in_stack) {
                                displaced_hir_it->second.state = ListIt::NON_RES_HIR;
                                displaced_hir_it->second.is_ghost = true;
                            } 
                            
                            else { 
                                hash_.erase(displaced_hir_it);
                            }
                        } 
                    }
                }

                bool isStackFull() const { return stack.size() > stack_sz_; }
                bool isQueueFull() const { return queue.size() > queue_sz_; }
                bool isLirFull()   const { return lir_count_ == lir_space_sz_; }

                std::unordered_map<KeyT, node> hash_;

                template <typename F>
                bool LookUpUpdate(KeyT key, F slow_get_page) {
                    auto hit_it = hash_.find(key);
                  

                    if (hit_it != hash_.end()) {

                        auto key = hit_it->first;
                        auto& node = hit_it->second;

                        if (node.state == ListIt::LIR) {
                            
                            bool is_bottom = false;

                            if (key == stack.front()) { is_bottom = true; }

                            stack.splice(stack.end(), stack, node.iter_S);
                            
                            if (is_bottom) { PruneStack(); }

                            return true;
                        }

                        else if (hit_it->second.state == ListIt::RES_HIR) {
                            if (node.in_queue) {
                                queue.erase(node.iter_Q);
                                node.in_queue = false;
                                node.state = ListIt::LIR;

                                if (!isLirFull()) { 
                                    ++lir_count_; 
                                } else {
                                    DemoteBottomLir();
                                } 
                            }

                            if (node.in_stack) {
                                stack.splice(stack.end(), stack, node.iter_S);
                            } else {
                                
                                node.iter_S = stack.insert(stack.end(), key);
                                node.in_stack = true;
                            }
                            node.in_queue = false;

                            return true;
                        } 
                        
                        else if (node.state == ListIt::NON_RES_HIR) {
                            
                                node.state = ListIt::LIR;
                                node.in_stack = true;
                                stack.splice(stack.end(), stack, node.iter_S);
                                node.val = slow_get_page(key);

                                if (!isLirFull()) { 
                                    ++lir_count_; 
                                } else {
                                    DemoteBottomLir();
                                }
                                                  
                            return false;
                        }    
                    }

                    Value fresh_data = slow_get_page(key);
                    node new_node;
                    new_node.val = fresh_data;

                    if (!isLirFull()) {
                        new_node.state = ListIt::LIR;
                        new_node.in_stack = true;
                        new_node.in_queue = false;

                        new_node.iter_S = stack.insert(stack.end(), key);

                        ++lir_count_;
                    } else {
                        new_node.state = ListIt::RES_HIR;
                        new_node.in_queue = true;
                        new_node.in_stack = true;

                        new_node.iter_S = stack.insert(stack.end(), key);
                        new_node.iter_Q = queue.insert(queue.end(), key);
                        
                        if (isQueueFull()) {
                            auto victim_key = queue.front();
                            auto victim_it = hash_.find(victim_key);
                            
                            queue.pop_front();
                            victim_it->second.in_queue = false;

                            if (victim_it->second.in_stack) { 
                                victim_it->second.state = ListIt::NON_RES_HIR;
                                
                                victim_it->second.val = Value{};
                                
                            } else {
                                hash_.erase(victim_key);                            
                            }
                        }
                    }

                    hash_.emplace(key, new_node);
                    return false;
                }   
                
            };

void DestroyCache(Cache* cache)
{
    if(!cache) return;

    for(size_t i = 0; i < cache->layers_count; i++)
    {
        delete cache->layers[i];
    }
    free(cache->layers);
    free(cache);
}

int RunCache(Cache* cache, size_t count, VALUE* numbers)
{
    if(!cache) return -1;

    size_t miss = 0;

    for(size_t i = 0; i < count; i++)
    {
        int j
        miss += cache->layers[0]->LookUpUpdate();
    }

    return (int)miss;
}