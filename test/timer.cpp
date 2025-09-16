/**
 * @file timer.cpp
 * @author stroll (116356647@qq.com)
 * @brief timer 测试程序
 * @version 0.1
 * @date 2025-09-12
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "utils/timer.hpp"
#include "utils/logger.hpp"

using namespace stroll;

MinHeap heap;

void test_min_heap_up() {

    auto node = std::make_shared<TimerNode>();
    node->next_tp = 1000;
    heap.push_and_sort(node);
    heap.dump();

    node = std::make_shared<TimerNode>();
    node->next_tp = 2000;
    heap.push_and_sort(node);
    heap.dump();

    node = std::make_shared<TimerNode>();
    node->next_tp = 1500;
    heap.push_and_sort(node);
    heap.dump();

    node = std::make_shared<TimerNode>();
    node->next_tp = 1300;
    heap.push_and_sort(node);
    heap.dump();

    node = std::make_shared<TimerNode>();
    node->next_tp = 3000;
    heap.push_and_sort(node);
    heap.dump();

    node = std::make_shared<TimerNode>();
    node->next_tp = 100;
    heap.push_and_sort(node);
    heap.dump();
}

void test_min_heap_down() {
    sl_info("\n");
    heap.update_top(1200);
    heap.dump();

    printf("%s - %d\n", __func__, __LINE__);
    heap.update_top(1500);
    heap.dump();

    printf("%s - %d\n", __func__, __LINE__);
    heap.update_top(4500);
    heap.dump();

    printf("%s - %d\n", __func__, __LINE__);
    heap.update_top(3500);
    heap.dump();
}

void test_min_heap_place() {
    printf("%s - %d\n", __func__, __LINE__);
    auto h = heap.at(0);

    heap.update_place(h, -1);
    heap.dump();

    printf("%s - %d\n", __func__, __LINE__);
    h = heap.at(2);
    heap.update_place(h, -1);
    heap.dump();

    printf("%s - %d\n", __func__, __LINE__);
    heap.update_place(h, 1000);
    heap.dump();
}

void test_timer() {
    auto func = []() { sl_info("test func\n"); };

    auto timer = new Timer("test func", func, 5000);
    getchar();
    timer->start();

    auto func_other = []() { sl_info("other func\n"); };
    auto timer_other = new Timer("other func", func_other, 3000, 2000);
    timer_other->start();
    getchar();

    sl_info("timer->set_interval(1000) and timer_other->stop\n");
    timer_other->stop();
    timer->set_interval(1000);
    getchar();
    sl_info("timer_other->start()\n");
    timer_other->start();
    timer_other->set_interval(10 * 1000);
    getchar();
}

int main() {
    test_timer();

    return 0;
}
