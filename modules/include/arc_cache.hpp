#pragma once

#include "cache.hpp"

#include <algorithm>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

namespace arc_cache {

template <typename KeyT, typename Value>
class arc_cache : public CacheAPI<KeyT, Value> {
public:
    explicit arc_cache(
        std::size_t sz,
        CacheAPI<KeyT, Value>* next = nullptr
    )
        : CacheAPI<KeyT, Value>(next), cache_sz_(sz) {}

    CacheResult<int, int> Request(KeyT key) override {
        int misses_below = 0;
        Value result_data;

        auto fetch_from_next = [&](KeyT k) -> Value {
            if (this->next_cache_level != nullptr) {
                auto res = this->next_cache_level->Request(k);
                misses_below = res.misses;

                return res.data;
            }

            return static_cast<Value>(k);
        };

        if (LookUpUpdate(key, fetch_from_next, result_data)) {
            return {result_data, 0};
        }
        return  {result_data, 1 + misses_below};
    }

private:
    enum class ListKind {
        T1_RECENT,
        T2_FREQUENT,
        B1_RECENT_GHOST,
        B2_FREQUENT_GHOST
    };

    struct Node {
        Value val;
        ListKind state;
        typename std::list<KeyT>::iterator it;

        Node(Value value, ListKind list,
            typename std::list<KeyT>::iterator pos = {})
            : val(std::move(value)), state(list), it(pos) {}
    };

    std::size_t cache_sz_;
    std::size_t target_recent_size_ = 0;

    std::list<KeyT> recent_list_;
    std::list<KeyT> frequent_list_;
    std::list<KeyT> recent_history_;
    std::list<KeyT> frequent_history_;
    std::unordered_map<KeyT, Node> hash_;

    bool isCacheFull() const {
        return recent_list_.size() + frequent_list_.size() >= cache_sz_;
    }

    bool isRecentHistoryFull() const {
        return recent_history_.size() >= cache_sz_;
    }

    bool isFrequentHistoryFull() const {
        return frequent_history_.size() >= cache_sz_;
    }

    void Replace() {
        if (recent_list_.size() >= std::max<std::size_t>(1, target_recent_size_)) {
            auto evicted_recent_key = hash_.find(recent_list_.back());

            if (isRecentHistoryFull()) {
                hash_.erase(recent_history_.back());
                recent_history_.pop_back();
            }

            recent_history_.splice( recent_history_.begin(), recent_list_, 
                                           evicted_recent_key->second.it);

            evicted_recent_key->second.state = ListKind::B1_RECENT_GHOST;
            evicted_recent_key->second.val = Value{};
        } else {
            auto evicted_frequent_key = hash_.find(frequent_list_.back());

            if (isFrequentHistoryFull()) {
                hash_.erase(frequent_history_.back());
                frequent_history_.pop_back();
            }

            frequent_history_.splice(frequent_history_.begin(), frequent_list_, 
                                              evicted_frequent_key->second.it);

            evicted_frequent_key->second.state = ListKind::B2_FREQUENT_GHOST;
            evicted_frequent_key->second.val = Value{};
        }
    }

template <typename F>
    bool LookUpUpdate(KeyT key, F slow_get_page, Value& out_val) {
        if (cache_sz_ == 0) {
            out_val = slow_get_page(key);
            return false;
        }

        auto hit_it = hash_.find(key);

        if (hit_it != hash_.end()) {
            out_val = hit_it->second.val; 

            if (hit_it->second.state == ListKind::T1_RECENT) {
                hit_it->second.state = ListKind::T2_FREQUENT;
                frequent_list_.splice(frequent_list_.begin(), 
                                      recent_list_, hit_it->second.it);
                return true;
            }

            if (hit_it->second.state == ListKind::T2_FREQUENT) {
                frequent_list_.splice(frequent_list_.begin(), 
                                      frequent_list_, hit_it->second.it);
                return true;
            }

            if (hit_it->second.state == ListKind::B1_RECENT_GHOST) {
                std::size_t delta = std::max<std::size_t>(1, frequent_list_.size() /
                                    std::max<std::size_t>(1, recent_list_.size()));

                target_recent_size_ = std::min(cache_sz_, target_recent_size_ + delta);
                
                out_val = slow_get_page(key); 

                if (isCacheFull()) {
                    Replace();
                    hit_it = hash_.find(key);
                    if (hit_it == hash_.end()) {
                        frequent_list_.push_front(key);
                        hash_.emplace(key, Node(out_val, ListKind::T2_FREQUENT,
                                                frequent_list_.begin()));
                        return false;
                    }
                }

                hit_it->second.state = ListKind::T2_FREQUENT;
                frequent_list_.splice(frequent_list_.begin(), 
                                      recent_history_, hit_it->second.it);
                hit_it->second.val = out_val;
                return false;
            }

            if (hit_it->second.state == ListKind::B2_FREQUENT_GHOST) {
                std::size_t delta = std::max<std::size_t>(1, recent_list_.size() /
                                    std::max<std::size_t>(1, frequent_list_.size()));
                target_recent_size_ = (target_recent_size_ >= delta) ? 
                                      target_recent_size_ - delta : 0;
                
                out_val = slow_get_page(key);

                if (isCacheFull()) {
                    Replace();
                    hit_it = hash_.find(key);
                    if (hit_it == hash_.end()) {
                        frequent_list_.push_front(key);
                        hash_.emplace(key, Node(out_val, ListKind::T2_FREQUENT,
                                                frequent_list_.begin()));
                        return false;
                    }
                }

                hit_it->second.state = ListKind::T2_FREQUENT;
                frequent_list_.splice(frequent_list_.begin(), frequent_history_, 
                                      hit_it->second.it);
                hit_it->second.val = out_val;
                return false;
            }
        }

        out_val = slow_get_page(key);

        if (isCacheFull()) {
            Replace();
        }
        recent_list_.push_front(key);
        hash_.emplace(key, Node(out_val, ListKind::T1_RECENT, recent_list_.begin()));
        return false;
    }
};

} // namespace arc_cache
