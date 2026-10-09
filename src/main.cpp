#include <iostream>
#include <vector>
#include <memory>
#include <cassert>

#include "config.h"
#include "cache.hpp"
#include "tests.hpp"

static const char* CONFIG_FILENAME = "config.cfg";

int main()
{

    RunUnitTests();

    std::unique_ptr<Config> config = LoadConfig(CONFIG_FILENAME);
    if(!config) exit(EXIT_FAILURE);

    CacheAPI<int, int>* system_cache = CreateCache<int, int>(std::move(config));
    if(!system_cache) exit(EXIT_FAILURE);

    size_t count = 0;
    std::cin >> count;

    assert(std::cin.good());

    std::vector<int> numbers(count);

    for(size_t i = 0; i < count; i++) std::cin >> numbers[i]; 

    int miss = RunCache(system_cache, count, numbers); 
    
    std::cout << "Misses: " << miss << std::endl;

    delete system_cache;

    return 0;
}