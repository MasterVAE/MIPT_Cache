#pragma once

#include <string>
#include <vector>
#include <cstddef>

struct CacheTestCase {
    std::string name;
    std::size_t capacity;
    std::vector<int> requests;
    int expected_misses;
};


int RunUnitTests();