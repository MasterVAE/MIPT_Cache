#pragma once

#include <unordered_map>
#include <vector>
#include <iostream>

template <typename keyT, typename Value>
    class ring_buffer : 2Q_cache {
        public:

        size_t in_sz_;
        std::vector<std::pair<keyT, Value>> cache_in_;
        int tail = 0;
        int head = 0;

        ring_buffer(size_t size = 0) : in_sz_{size}, cache_in_(size) {}
        
        bool isFull()  const { return cache_in_.capacity() ==  cache_in_.size(); }
        bool isEmpty() const {       return cache_in_.empty(); }
        
        auto RingPush(keyT key) {
            int temp_head = head;

            cache_in_[temp_head].first = key;
            head = (head + 1) % in_sz_;
            
            return cache_in_[temp_head];
        }
        
        keyT RingPop() {
            if (!isEmpty()) { 
                keyT temp_tail_key = cache_in_[tail].first; 
                
                tail = (tail + 1) % in_sz_;
                return temp_tail_key;
            }
            return keyT{};
        }
         
    };