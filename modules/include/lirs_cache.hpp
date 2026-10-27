#pragma once

#include "cache.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <list>
#include <unordered_map>
#include <utility>

namespace lirs_cache {

template <typename KeyT, typename Value>
class lirs_cache : public CacheAPI<KeyT, Value> {
public:
    explicit lirs_cache(
        std::size_t sz,
        CacheAPI<KeyT, Value>* next_cache_level = nullptr
    )
        : CacheAPI<KeyT, Value>(next_cache_level),
          cache_sz_(sz),
          stack_sz_(std::max<std::size_t>(1, sz * 3)),
          queue_sz_(std::max<std::size_t>(1, sz / 100)),
          lir_count_(0),
          lir_space_sz_(std::max<std::size_t>(1, (sz * 99) / 100)) {}

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
    enum class ListKind { RES_HIR, LIR, NON_RES_HIR };

    struct Node {
        Value val{};
        ListKind state = ListKind::RES_HIR;
        bool is_ghost = false;
        bool in_stack = false;
        bool in_queue = false;

        using ListIterator = typename std::list<KeyT>::iterator;
        ListIterator iter_Q = {};
        ListIterator iter_S = {};

        Node() = default;
        Node(Value value, ListKind list = ListKind::RES_HIR)
            : val(std::move(value)), state(list) {}
    };

    std::size_t cache_sz_;
    std::size_t stack_sz_;
    std::size_t queue_sz_;
    std::size_t lir_count_;
    std::size_t lir_space_sz_;

    std::list<KeyT> stack_;
    std::list<KeyT> queue_;
    std::unordered_map<KeyT, Node> hash_;

    bool isStackFull() const { return stack_.size() > stack_sz_; }
    bool isQueueFull() const { return queue_.size() > queue_sz_; }
    bool isLirFull() const { return lir_count_ == lir_space_sz_; }

    void PruneStack() {
        while (!stack_.empty()) {
            KeyT bottom_key = stack_.front();
            auto it_hash = hash_.find(bottom_key);

            if (it_hash->second.state == ListKind::LIR) {
                break;
            }

            it_hash->second.in_stack = false;
            if (it_hash->second.state == ListKind::NON_RES_HIR) {
                hash_.erase(it_hash);
            }
            stack_.pop_front();
        }
    }

    void DemoteBottomLir() {
        auto stack_bottom = stack_.begin();
        auto it = hash_.find(*stack_bottom);

        if (it->second.state == ListKind::LIR) {
            it->second.state = ListKind::RES_HIR;
            it->second.in_stack = false;
            it->second.in_queue = true;

            queue_.splice(queue_.end(), stack_, it->second.iter_S);
            it->second.iter_Q = std::prev(queue_.end());

            PruneStack();

            if (isQueueFull()) {
                KeyT displaced_key = queue_.front();
                auto displaced_it = hash_.find(displaced_key);

                displaced_it->second.in_queue = false;
                queue_.pop_front();

                if (displaced_it->second.in_stack) {
                    displaced_it->second.state = ListKind::NON_RES_HIR;
                    displaced_it->second.is_ghost = true;
                } else {
                    hash_.erase(displaced_it);
                }
            }
        }
    }

    template <typename F>
    bool LookUpUpdate(KeyT key, F slow_get_page) {
        auto hit_it = hash_.find(key);

        if (hit_it != hash_.end()) {
            auto& entry = hit_it->second;

            if (entry.state == ListKind::LIR) {
                bool is_bottom = (key == stack_.front());
                stack_.splice(stack_.end(), stack_, entry.iter_S);
                if (is_bottom) {
                    PruneStack();
                }
                return true;
            }

            if (entry.state == ListKind::RES_HIR) {
                if (entry.in_queue) {
                    queue_.erase(entry.iter_Q);
                    entry.in_queue = false;
                    entry.state = ListKind::LIR;

                    if (!isLirFull()) {
                        ++lir_count_;
                    } else {
                        DemoteBottomLir();
                    }
                }

                if (entry.in_stack) {
                    stack_.splice(stack_.end(), stack_, entry.iter_S);
                } else {
                    entry.iter_S = stack_.insert(stack_.end(), key);
                    entry.in_stack = true;
                }
                entry.in_queue = false;
                return true;
            }

            if (entry.state == ListKind::NON_RES_HIR) {
                entry.state = ListKind::LIR;
                entry.in_stack = true;
                stack_.splice(stack_.end(), stack_, entry.iter_S);
                entry.val = slow_get_page(key);

                if (!isLirFull()) {
                    ++lir_count_;
                } else {
                    DemoteBottomLir();
                }
                return false;
            }
        }

        Value fresh_data = slow_get_page(key);
        Node new_node;
        new_node.val = std::move(fresh_data);

        if (!isLirFull()) {
            new_node.state = ListKind::LIR;
            new_node.in_stack = true;
            new_node.in_queue = false;
            new_node.iter_S = stack_.insert(stack_.end(), key);
            ++lir_count_;
        } else {
            new_node.state = ListKind::RES_HIR;
            new_node.in_queue = true;
            new_node.in_stack = true;
            new_node.iter_S = stack_.insert(stack_.end(), key);
            new_node.iter_Q = queue_.insert(queue_.end(), key);

            if (isQueueFull()) {
                KeyT victim_key = queue_.front();
                auto victim_it = hash_.find(victim_key);

                queue_.pop_front();
                victim_it->second.in_queue = false;

                if (victim_it->second.in_stack) {
                    victim_it->second.state = ListKind::NON_RES_HIR;
                    victim_it->second.val = Value{};
                } else {
                    hash_.erase(victim_key);
                }
            }
        }

        hash_.emplace(key, std::move(new_node));
        return false;
    }
};

} // namespace lirs_cache
