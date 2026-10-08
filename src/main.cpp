#include <iostream>
#include <vector>
#include <memory>

#include "config.h"
#include "cache.h"

static const char* CONFIG_FILENAME = "config.cfg";

int main()
{
    std::unique_ptr<Config> config = LoadConfig(CONFIG_FILENAME);
    if(!config) exit(EXIT_FAILURE);

    Cache<int>* cache = CreateCache<int>(std::move(config));
    if(!cache) exit(EXIT_FAILURE);

    size_t count;
    std::cin >> count;
    if(count <= 0) 
    {
        delete cache;
        exit(EXIT_FAILURE);
    }


    std::vector<int> numbers(count);

    for(size_t i = 0; i < count; i++) std::cin >> numbers[i]; 

    int miss = RunCache(cache, count, numbers);

    delete cache;

    if(miss < 0) exit(EXIT_FAILURE);

    std::cout << "Misses: " << miss << std::endl;

    return 0;
}