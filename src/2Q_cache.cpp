#include <iostream>
#include <cassert>

#include "2Q_cache.hpp"

int slow_get_page(int key) { return key; }

int main(){

    int hits   = 0; 
    size_t cache_sz  = 0;
    size_t cache_cap = 0;
    
    assert(std::cin.good());

    std::cin >> cache_sz >> cache_cap;
    cache::cache_2Q_<int, int> my_cache{cache_cap};

    for (int i = 0; i < cache_sz; i++) {
        int key;

        assert(std::cin.good());
        std::cin >> key;
        if (my_cache.LookUpUpdate(key, slow_get_page)) {
            hits++;
        }
    }
    std::cout << hits;
    return 0;
}