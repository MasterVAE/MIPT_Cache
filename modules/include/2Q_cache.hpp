#pragma once

#include "cache.hpp"

#include <algorithm>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

namespace cache_2Q {

template <typename KeyT, typename Value>
class cache_2Q_ : public CacheAPI<KeyT, Value> {
public:
    explicit cache_2Q_(
        std::size_t cache_sz,
        CacheAPI<KeyT, Value>* next = nullptr
    )
        : CacheAPI<KeyT, Value>(next),
          cache_sz_(cache_sz),
          in_sz_(std::max<std::size_t>(1, cache_sz / 4)),
          out_sz_(std::max<std::size_t>(1, cache_sz)) {}

    int Request(KeyT key) override {
        int misses_below = 0;

        auto fetch_from_next = [&](KeyT k) -> Value {
            if (this->next_cache_level != nullptr) {
                misses_below = this->next_cache_level->Request(k);
            }
            return static_cast<Value>(k);
        };

        if (LookUpUpdate(key, fetch_from_next)) {
            return 0;
        }
        return 1 + misses_below;
    }

private:
    enum class ListKind { IN_, HOT_, OUT_ };

    struct Node {
        Value val;
        ListKind state;
        typename std::list<KeyT>::iterator it;

        Node(Value value, ListKind list,
             typename std::list<KeyT>::iterator pos = {})
            : val(std::move(value)), state(list), it(pos) {}
    };

    std::size_t cache_sz_;
    std::size_t in_sz_;
    std::size_t out_sz_;

    std::list<KeyT> cache_in_;
    std::list<KeyT> cache_out_;
    std::list<KeyT> cache_hot_;
    std::unordered_map<KeyT, Node> hash_;

    template <typename F>
    bool LookUpUpdate(KeyT key, F slow_get_page) {
        auto hit_it = hash_.find(key);

        if (hit_it != hash_.end()) {
            if (hit_it->second.state == ListKind::IN_) {
                return true;
            }

            if (hit_it->second.state == ListKind::HOT_) {
                cache_hot_.splice(cache_hot_.begin(), cache_hot_, hit_it->second.it);
                return true;
            }

            if (hit_it->second.state == ListKind::OUT_) {
                cache_out_.erase(hit_it->second.it);
                EnsureFreeSpace();

                cache_hot_.push_front(key);
                hit_it->second.val = slow_get_page(key);
                hit_it->second.state = ListKind::HOT_;
                hit_it->second.it = cache_hot_.begin();
                return false;
            }
        }

        EnsureFreeSpace();

        Value fresh_data = slow_get_page(key);
        cache_in_.push_front(key);
        hash_.emplace(key, Node{std::move(fresh_data), ListKind::IN_, cache_in_.begin()});
        return false;
    }

    void EnsureFreeSpace() {
        if (cache_in_.size() + cache_hot_.size() < cache_sz_) {
            return;
        }

        if (cache_in_.size() > in_sz_ || cache_hot_.empty()) {
            KeyT victim = cache_in_.back();
            cache_in_.pop_back();

            if (cache_out_.size() >= out_sz_) {
                hash_.erase(cache_out_.back());
                cache_out_.pop_back();
            }

            cache_out_.push_front(victim);
            hash_.at(victim).state = ListKind::OUT_;
            hash_.at(victim).it = cache_out_.begin();
        } else {
            KeyT victim = cache_hot_.back();
            cache_hot_.pop_back();
            hash_.erase(victim);
        }
    }
};

} // namespace cache_2Q
