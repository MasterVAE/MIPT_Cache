#include <iostream>
#include <vector>
#include <memory>

#include "config.h"
#include "cache.h"
#include "test.h"

static const char* CONFIG_FILENAME = "config.cfg";

int main()
{
    std::unique_ptr<Config> config = LoadConfig(CONFIG_FILENAME);
    if(!config) exit(EXIT_FAILURE);

    Cache* cache = CreateCache(std::move(config));
    if(!cache) exit(EXIT_FAILURE);

    size_t count;
    std::cin >> count;
    if(count <= 0) 
    {
        DestroyCache(cache);
        exit(EXIT_FAILURE);
    }

    int* numbers = (int*)calloc(count, sizeof(int)); // change this, все со
    
    for(size_t i = 0; i < count; i++) std::cin >> numbers[i];

    int miss = RunCache(cache, count, numbers);
    free(numbers); //!!!change this
    DestroyCache(cache); // and this

    if(miss < 0) exit(EXIT_FAILURE);

    std::cout << "Misses: " << miss << std::endl;

    return 0;
}