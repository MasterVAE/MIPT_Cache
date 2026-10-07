#include <iostream>
#include <cassert>

#include "lirs_cache.hpp"

int slow_get_page(int key) { return key; }

int main(){

    int hits   = 0; 
    size_t cache_sz  = 0;
    size_t cache_cap = 0;
    
    assert(std::cin.good());

    std::cin >> cache_sz >> cache_cap;
    lirs_cache::lirs_cache<int, int> my_cache{cache_sz};

    int key = 0;
    
    for (int i = 0; i < cache_cap; i++) {
        assert(std::cin.good());
        std::cin >> key;
        if (my_cache.LookUpUpdate(key, slow_get_page)) {
            hits++;
        }
    }
    std::cout << hits << std::endl;
    return 0;
}