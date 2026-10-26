
#pragma once

#include <cassert>
#include <cstddef>
#include <iterator>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>


template <typename KeyT, typename Value>
class CacheAPI {
protected:
    CacheAPI<KeyT, Value>* next_cache_level;

public:
    explicit CacheAPI(CacheAPI<KeyT, Value>* next = nullptr)
        : next_cache_level(next) {}

    virtual ~CacheAPI() {
        delete next_cache_level;
    }

    virtual int Request(KeyT key) = 0;

    CacheAPI(const CacheAPI&) = delete;
    CacheAPI& operator=(const CacheAPI&) = delete;
};


template <typename KeyT, typename Value>
int RunCache(
    CacheAPI<KeyT, Value>* current_top,
    std::size_t count,
    const std::vector<Value>& values
) {
    assert(current_top);
    assert(count <= values.size());

    int misses = 0;

    for (std::size_t i = 0; i < count; ++i) {
        misses += current_top->Request(values[i]);
    }

    return misses;
}


template <typename Value>
int SimulatePerfectCache(
    const std::vector<Value>& requests,
    std::size_t capacity,
    std::vector<Value>& misses
) {
    if (capacity == 0) {
        misses = requests;
        return static_cast<int>(requests.size());
    }

    const std::size_t n = requests.size();
    const std::size_t INF = n + 1;

    std::vector<std::size_t> next(n, INF);
    std::unordered_map<Value, std::size_t> last;

    for (std::size_t i = n; i-- > 0;) {
        auto it = last.find(requests[i]);

        if (it != last.end()) {
            next[i] = it->second;
        }

        last[requests[i]] = i;
    }

    std::unordered_map<Value, std::size_t> cache;
    std::set<std::pair<std::size_t, Value>> evictSet;

    int missCount = 0;

    for (std::size_t i = 0; i < n; ++i) {
        const Value& val = requests[i];
        auto it = cache.find(val);

        if (it != cache.end()) {
            std::size_t oldNext = it->second;

            evictSet.erase({oldNext, val});
            it->second = next[i];
            evictSet.insert({next[i], val});
        } else {
            ++missCount;
            misses.push_back(val);

            if (cache.size() == capacity) {
                auto evictIt = std::prev(evictSet.end());
                Value evictVal = evictIt->second;

                cache.erase(evictVal);
                evictSet.erase(evictIt);
            }

            cache[val] = next[i];
            evictSet.insert({next[i], val});
        }
    }

    return missCount;
}


template <typename Value>
int RunPerfectCache(
    std::vector<std::size_t> capacity,
    std::vector<Value> values
) {
    int totalMisses = 0;
    std::vector<Value> requests = std::move(values);

    for (std::size_t cap : capacity) {
        if (requests.empty()) {
            break;
        }

        std::vector<Value> misses;

        totalMisses += SimulatePerfectCache(
            requests, cap, misses
        );

        requests = std::move(misses);
    }

    return totalMisses;
}
