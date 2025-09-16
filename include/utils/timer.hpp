/**
 * @file timer.hpp
 * @author stroll (116356647@qq.com)
 * @brief 定时器
 * @version 0.1
 * @date 2025-09-12
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "utils/logger.hpp"

namespace stroll {

using TimerFunc = std::function<void()>;

struct TimerNode {
    static const uint64_t kMaxTimePoint = 0x7ffffffffffffffful;

    class RunningGuard {
       public:
        RunningGuard(std::atomic<bool> &flag) : flag_(flag) { flag_ = true; }

        ~RunningGuard() { flag_ = false; }

       private:
        std::atomic<bool> &flag_;
    };

    std::string name;
    TimerFunc func;
    uint32_t index = 0;
    uint64_t interval_ns = 0;
    uint64_t delay_ns = 0;
    uint64_t next_tp = kMaxTimePoint;  //< next timepoint
    std::atomic<bool> running{false};

    bool operator<(const TimerNode &other) { return next_tp < other.next_tp; }

    void dump() {
        printf("name: %s, index: %u, interval_ns: %" PRIu64 ", delay_ns: %" PRIu64
               ", next tp: %" PRIu64 "\n",
               name.c_str(), index, interval_ns, delay_ns, next_tp);
    }
};

using TimerHandler = std::shared_ptr<TimerNode>;
using ConstTimerHandler = std::shared_ptr<const TimerNode>;

static inline bool operator<(const TimerHandler &left, const TimerHandler &right) {
    // std::cout << "left: " << left->next_tp << " right: " << right->next_tp << std::endl;
    return left->next_tp < right->next_tp;
}

/// @brief 最小堆，用来存放定时任务，堆顶放置最优先执行的任务
class MinHeap {
   public:
    MinHeap() { buff_.reserve(64); }

    ~MinHeap() = default;

    bool empty() const { return buff_.empty(); }

    TimerHandler &top() { return buff_.at(0); }

    void update_top(uint64_t tp) {
        auto &sp = buff_.at(0);
        sp->next_tp = tp;
        percolate_down(0);
    }

    void push(const TimerHandler &h) {
        h->index = buff_.size();
        buff_.push_back(h);
    }

    void push_and_sort(const TimerHandler &h) {
        push(h);
        percolate_up(h->index);
    }

    void update_place(const TimerHandler &h, uint64_t tp) {
        auto old = h->next_tp;
        h->next_tp = tp;
        if (tp > old) {
            percolate_down(h->index);
        } else if (tp < old) {
            percolate_up(h->index);
        }
    }

    void dump() {
        for (auto &node : buff_) {
            node->dump();
        }
        printf("\n");
    }

    /// @brief 获取指定索引处对象，仅用来测试，不要使用
    /// @param index
    /// @return
    auto at(unsigned index) { return buff_.at(index); }

   private:
    /// @brief 向上调整，和父节点比较，如果比父节点小就替换
    /// @param pos 当前节点索引
    void percolate_up(uint32_t pos) {
        auto wrap_pos = pos + 1;
        auto tmp = buff_.at(pos);

        auto wrap_pi = wrap_pos >> 1;  // wrap parent index
        while (wrap_pi >= 1 && tmp < buff_.at(wrap_pi - 1)) {
            auto index = wrap_pos - 1;
            buff_[index] = buff_.at(wrap_pi - 1);
            buff_[index]->index = index;

            wrap_pos = wrap_pi;
            wrap_pi >>= 1;
        }

        if (wrap_pos - 1 != pos) {
            auto index = wrap_pos - 1;
            tmp->index = index;
            buff_.at(index) = tmp;
        }
    }

    /// @brief 向下调整，和子节点比较，如果比子节点大就替换
    /// @param pos
    void percolate_down(uint32_t pos) {
        auto tmp = buff_.at(pos);
        auto wrap_pos = pos + 1;
        auto child_index = find_min_child(pos);

        while (child_index != wrap_pos - 1 && buff_.at(child_index) < tmp) {
            auto index = wrap_pos - 1;
            buff_.at(index) = buff_.at(child_index);
            buff_[index]->index = index;

            wrap_pos = child_index + 1;
            child_index = find_min_child(child_index);
        }

        if (wrap_pos - 1 != pos) {
            auto index = wrap_pos - 1;
            tmp->index = index;
            buff_.at(index) = tmp;
        }
    }

    uint32_t find_min_child(uint32_t pos) {
        auto wrap_pos = pos + 1;
        auto left_pos = (wrap_pos << 1) - 1;
        auto right_pos = left_pos + 1;

        //< 没有子节点
        if (left_pos >= buff_.size()) {
            return pos;
        } else if (right_pos >= buff_.size()) {
            return left_pos;
        } else {
            return buff_.at(left_pos) < buff_.at(right_pos) ? left_pos : right_pos;
        }
    }

   private:
    std::vector<TimerHandler> buff_;
};

class TimerManager final {
    static const unsigned max_thread_num = 4;

   public:
    static TimerManager &instance() {
        static TimerManager _inst;
        return _inst;
    }

    ~TimerManager() { quit_and_wait(); }

    TimerHandler add_timer(const char *name, const TimerFunc &func, unsigned interval_ms,
                           unsigned delay_ms) {
        auto handler = std::make_shared<TimerNode>();
        handler->func = func;
        handler->name = name;
        handler->interval_ns = 1000ull * 1000 * interval_ms;
        handler->delay_ns = 1000ull * 1000 * delay_ms;
        mtx_heap_.lock();
        min_heap_.push(handler);
        mtx_heap_.unlock();
        handler->dump();
        return handler;
    }

    int start(TimerHandler &handler) {
        if (!handler) {
            return 0;
        }

        {
            std::lock_guard guard(mtx_heap_);
            auto next_tp = get_system_ns() + handler->delay_ns;
            min_heap_.update_place(handler, next_tp);
        }
        set_heap_update_flag();
        return 0;
    }

    int stop(TimerHandler &handler) {
        if (!handler) {
            return 0;
        }

        {
            std::lock_guard guard(mtx_heap_);
            auto next_tp = TimerNode::kMaxTimePoint;
            min_heap_.update_place(handler, next_tp);
        }
        set_heap_update_flag();
        return 0;
    }

    int set_interval(TimerHandler &handler, unsigned ms) {
        if (!handler) {
            return 0;
        }

        {
            std::lock_guard guard(mtx_heap_);
            handler->interval_ns = 1000ull * 1000 * ms;
            auto next_tp = get_system_ns() + handler->interval_ns;
            min_heap_.update_place(handler, next_tp);
        }
        set_heap_update_flag();
        return 0;
    }

    void dump() {
        sl_info("free thread number:%d \n", free_thread_num_);
        min_heap_.dump();
    }

   private:
    TimerManager() {
        for (auto i = 0u; i < max_thread_num; ++i) {
            thread_pool_[i] = std::thread(&TimerManager::on_work, this);
        }

        std::unique_lock lock(mtx_tp_);
        wakeup_flag_ = true;
        if (!exit_flag_) {
            cond_.notify_one();
        } else {
            cond_.notify_all();
        }
    }

    void on_work() {
        while (!exit_flag_) {
            //< 线程先统一阻塞，等待唤醒一个线程做为检测线程
            {
                std::unique_lock lock(mtx_tp_);
                ++free_thread_num_;
                cond_.wait(lock, [this]() -> bool { return exit_flag_ || wakeup_flag_; });
                --free_thread_num_;
                wakeup_flag_ = false;
                if (exit_flag_) {
                    ++free_thread_num_;
                    break;
                }
            }

            //< 检查定时任务
            check_and_dispatch();
        }
        sl_warn("timer thread pool exit, free_thread_num: %u\n", free_thread_num_);
    }

    void check_and_dispatch() {
        auto handler = check_task();
        if (!handler) {
            return;
        }

        //< 去执行定时器任务，执行前需要唤醒一个线程来做当前任务
        {
            std::unique_lock lock(mtx_tp_);
            wakeup_flag_ = true;
            cond_.notify_one();
        }

        TimerNode::RunningGuard running_guard(handler->running);
        if (handler->func) {
            handler->func();
        } else {
            sl_warn("name: %s no callback func\n", handler->name.c_str());
        }
    }

    TimerHandler check_task() {
        while (!exit_flag_) {
            std::unique_lock lock(mtx_heap_);
            if (min_heap_.empty()) {
                lock.unlock();
                //< 等待定时任务加入
                sleep_checker_for(TimerNode::kMaxTimePoint);
                continue;
            }

            auto handler = min_heap_.top();
            if (get_system_ns() >= handler->next_tp) {
                uint64_t next_tp = handler->interval_ns == 0
                                       ? TimerNode::kMaxTimePoint
                                       : handler->next_tp + handler->interval_ns;
                min_heap_.update_top(next_tp);
                //< 正在运行的任务，推迟到下一个周期，防止耗时任务把线程池全部阻塞
                if (handler->running) {
                    sl_warn("name: %s is running\n", handler->name.c_str());
                    continue;
                }
                return handler;
            }
            lock.unlock();

            //< 等待定时任务到期
            sleep_checker_for(handler->next_tp);
        }
        return nullptr;
    }

    void sleep_checker_for(uint64_t next_tp) {
        auto func = [this]() -> bool { return exit_flag_ || heap_update_flag_; };

        std::unique_lock checker_lock(checker_mtx_);
        if (next_tp != TimerNode::kMaxTimePoint) {
            auto tp = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(next_tp));
            checker_cond_.wait_until(checker_lock, tp, func);
        } else {
            checker_cond_.wait(checker_lock, func);
        }
        heap_update_flag_ = false;
    }

    uint64_t get_system_ns() {
        return std::chrono::steady_clock::now().time_since_epoch().count();
    }

    void set_heap_update_flag() {
        std::unique_lock lock(checker_mtx_);
        heap_update_flag_ = true;
        checker_cond_.notify_one();
    }

    void quit_and_wait() {
        mtx_tp_.lock();
        exit_flag_ = true;
        cond_.notify_all();
        mtx_tp_.unlock();

        //< 唤醒等待的检查器，准备退出
        set_heap_update_flag();

        //< 等待所有线程退出
        for (auto i = 0u; i < max_thread_num; ++i) {
            auto &thr = thread_pool_[i];
            if (thr.joinable()) {
                thr.join();
            }
        }
    }

   private:
    std::mutex mtx_heap_;
    MinHeap min_heap_;

    std::condition_variable checker_cond_;
    std::mutex checker_mtx_;
    bool heap_update_flag_ = false;
    //< 线程池
    std::condition_variable cond_;
    std::mutex mtx_tp_;
    std::array<std::thread, max_thread_num> thread_pool_;
    bool wakeup_flag_{false};
    uint8_t free_thread_num_ = 0;
    //< 系统退出
    bool exit_flag_{false};
};

/// @brief 定时器对外类
class Timer {
   public:
    /// @brief 构造一个定时器对象
    /// @param name 定时器名称
    /// @param func 定时器定期执行的任务
    /// @param interval_ms 定时器定期执行任务的周期,如果为0，则只执行一次， 单位为ms
    /// @param delay_ms 第一次延迟执行的时间，stop后重新start的话也会生效
    Timer(const char *name, const TimerFunc &func, unsigned interval_ms, unsigned delay_ms = 0) {
        handler_ = TimerManager::instance().add_timer(name, func, interval_ms, delay_ms);
    }

    /// @brief 销毁定时器对象
    ~Timer() { TimerManager::instance().stop(handler_); }

    /// @brief 开始定时器, 对于只执行一次的任务，start后会重新执行
    /// @return
    int start() { return TimerManager::instance().start(handler_); }

    /// @brief 停止定时器
    /// @return
    int stop() { return TimerManager::instance().stop(handler_); }

    /// @brief 设置周期任务间隔
    /// @param ms
    /// @return
    int set_interval(unsigned ms) { return TimerManager::instance().set_interval(handler_, ms); }

    /// @brief 获取当前周期任务间隔
    /// @return
    unsigned interval() const { return handler_->interval_ns / 1000 / 1000; }

    void dump() { handler_->dump(); }

   private:
    TimerHandler handler_;
};

}  // namespace stroll
