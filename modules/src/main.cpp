#include <iostream>
#include <vector>
#include <memory>
#include <cassert>
#include <cstdlib>
#include <utility>
#include <cstddef>

#include "config.h"
#include "cache.hpp"
#include "tests.hpp"
#include "cache.hpp"
#include "cache_factory.hpp"

int main(int argc, char* argv[])
{
#ifdef ENABLE_UNIT_TESTS
    RunUnitTests();
#endif

    if(argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_config>\n";
        exit(EXIT_FAILURE);
    }

    std::unique_ptr<Config> config = LoadConfig(argv[1]);
    if(!config) exit(EXIT_FAILURE);

    std::vector<std::size_t> layers;
    for(auto layer : config->layers)
    {
        layers.push_back(layer.size);
    }

    CacheAPI<int, int>* system_cache = CreateCache<int, int>(std::move(config));
    if(!system_cache) exit(EXIT_FAILURE);

    std::size_t count = 0;
    std::cin >> count;

    assert(std::cin.good());

    std::vector<int> numbers(count);

    for(std::size_t i = 0; i < count; i++) std::cin >> numbers[i];

    int miss = RunCache(system_cache, count, numbers);
    int perfect_miss = RunPerfectCache(layers, numbers);

    std::cout << "Misses: " << miss << std::endl;
    std::cout << "Perfect cache misses: " << perfect_miss << std::endl;

    delete system_cache;

    return 0;
}