#pragma once

#include <string>
#include <vector>

struct CacheTestCase {
    std::string name;
    size_t capacity;
    std::vector<int> requests;
    int expected_misses;
};