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
class lirs_cache final : public CacheAPI<KeyT, Value> {
public:
    explicit lirs_cache(
        std::size_t sz,
        std::unique_ptr<CacheAPI<KeyT, Value>> next = nullptr
    )
        : CacheAPI<KeyT, Value>(std::move(next)),
          cache_sz_(sz),
          stack_sz_(std::max<std::size_t>(1, sz * 3)),
          queue_sz_(std::max<std::size_t>(1, sz / 100)),
          lir_count_(0),
          lir_space_sz_(std::max<std::size_t>(1, (sz * 99) / 100)) {}

    CacheResult<int, int> Request(KeyT key) override {
        int misses_below = 0;
        Value result_data;

        auto fetch_from_next = [&](KeyT k) -> Value {
            if (this->next_cache_level_ != nullptr) {
                auto res = this->next_cache_level_->Request(k);
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
    enum class ListKind { RES_HIR, LIR, NON_RES_HIR };

    struct Node final {
        Value val_{};
        ListKind state_ = ListKind::RES_HIR;
        bool is_ghost_ = false;
        bool in_stack_ = false;
        bool in_queue_ = false;

        using ListIterator = typename std::list<KeyT>::iterator;
        ListIterator iter_Q_ = {};
        ListIterator iter_S_ = {};

        Node() = default;
        Node(Value value, ListKind list = ListKind::RES_HIR)
            : val_(std::move(value)), state_(list) {}
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

            if (it_hash->second.state_ == ListKind::LIR) {
                break;
            }

            it_hash->second.in_stack_ = false;
            if (it_hash->second.state_ == ListKind::NON_RES_HIR) {
                hash_.erase(it_hash);
            }
            stack_.pop_front();
        }
    }

    void DemoteBottomLir() {
        auto stack_bottom = stack_.begin();
        auto it = hash_.find(*stack_bottom);

        if (it->second.state_ == ListKind::LIR) {
            it->second.state_ = ListKind::RES_HIR;
            it->second.in_stack_ = false;
            it->second.in_queue_ = true;

            queue_.splice(queue_.end(), stack_, it->second.iter_S_);
            it->second.iter_Q_ = std::prev(queue_.end());

            PruneStack();

            if (isQueueFull()) {
                KeyT displaced_key = queue_.front();
                auto displaced_it = hash_.find(displaced_key);

                displaced_it->second.in_queue_ = false;
                queue_.pop_front();

                if (displaced_it->second.in_stack_) {
                    displaced_it->second.state_ = ListKind::NON_RES_HIR;
                    displaced_it->second.is_ghost_ = true;
                } else {
                    hash_.erase(displaced_it);
                }
            }
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
            auto& entry = hit_it->second;

            if (entry.state_ == ListKind::LIR) {
                out_val = entry.val_;
                bool is_bottom = (key == stack_.front());
                stack_.splice(stack_.end(), stack_, entry.iter_S_);
                if (is_bottom) {
                    PruneStack();
                }
                return true;
            }

            if (entry.state_ == ListKind::RES_HIR) {
                out_val = entry.val_; 
                
                if (entry.in_queue_) {
                    queue_.erase(entry.iter_Q_);
                    entry.in_queue_ = false;
                    entry.state_ = ListKind::LIR;

                    if (!isLirFull()) {
                        ++lir_count_;
                    } else {
                        DemoteBottomLir();
                    }
                }

                if (entry.in_stack_) {
                    stack_.splice(stack_.end(), stack_, entry.iter_S_);
                } else {
                    entry.iter_S_ = stack_.insert(stack_.end(), key);
                    entry.in_stack_ = true;
                }
                entry.in_queue_ = false;
                return true;
            }

            if (entry.state_ == ListKind::NON_RES_HIR) {
                out_val = slow_get_page(key);

                entry.state_ = ListKind::LIR;
                entry.in_stack_ = true;
                stack_.splice(stack_.end(), stack_, entry.iter_S_);
                entry.val_ = out_val;

                if (!isLirFull()) {
                    ++lir_count_;
                } else {
                    DemoteBottomLir();
                }
                return false;
            }
        }

        out_val = slow_get_page(key);
        
        Node new_node;
        new_node.val_ = out_val;

        if (!isLirFull()) {
            new_node.state_ = ListKind::LIR;
            new_node.in_stack_ = true;
            new_node.in_queue_ = false;
            new_node.iter_S_ = stack_.insert(stack_.end(), key);
            ++lir_count_;
        } else {
            new_node.state_ = ListKind::RES_HIR;
            new_node.in_queue_ = true;
            new_node.in_stack_ = true;
            new_node.iter_S_ = stack_.insert(stack_.end(), key);
            new_node.iter_Q_ = queue_.insert(queue_.end(), key);

            if (isQueueFull()) {
                KeyT victim_key = queue_.front();
                auto victim_it = hash_.find(victim_key);

                queue_.pop_front();
                victim_it->second.in_queue_ = false;

                if (victim_it->second.in_stack_) {
                    victim_it->second.state_ = ListKind::NON_RES_HIR;
                    victim_it->second.val_ = Value{};
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
