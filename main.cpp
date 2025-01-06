#include "pt_extend.hpp"
#include <iostream>
#include <format>
#include <chrono>
#include <thread>

struct Queue {
    int wpos{};
    int rpos{};
    int items[10]{};

    bool IsFull() {
        return (wpos + 1) % 10 == rpos;
    }

    void Enqueue(int item) {
        items[wpos] = item;
        wpos = (wpos + 1) % 10;
    }

    int Dequeue() {
        int item = items[rpos];
        rpos = (rpos + 1) % 10;
        return item;
    }

    bool IsEmpty() {
        return wpos == rpos;
    }
};
Queue q;

void NestNestedFunc(void*) {
    pt_extend_begin();
    std::cout << "[NestNestedFunc]: begin\n";

    std::cout << "[NestNestedFunc]: delay\n";
    pt_extend_delay(1000);

    std::cout << "[NestNestedFunc]: end\n";
    pt_extend_end();
}

void NestedFunc(void*) {
    pt_extend_begin();
    std::cout << "[NestedFunc]: begin\n";

    std::cout << "[NestedFunc]: delay\n";
    pt_extend_delay(1000);

    static PtCallContext ptFunc;
    std::cout << "[NestedFunc]: call Nested nested Func\n";
    pt_extend_nest_call(ptFunc, NestNestedFunc, nullptr);

    std::cout << "[NestedFunc]: delay 2\n";
    pt_extend_delay(1000);

    std::cout << "[NestedFunc]: end\n";
    pt_extend_end();
}

void Nested(void*) {
    pt_extend_begin();
    std::cout << "[Nested]: begin\n";

    std::cout << "[Nested]: delay\n";
    pt_extend_delay(1000);

    std::cout << "[Nested]: call NestedFunc\n";
    static PtCallContext ptFunc;
    pt_extend_co_call(ptFunc, NestedFunc, nullptr);

    std::cout << "[Nested]: delay 2\n";
    pt_extend_delay(1000);

    std::cout << "[Nested]: end\n";
    pt_extend_end();
}

void SysTick() {
    auto start = std::chrono::steady_clock::now();
    for (;;) {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (duration > 0) {
            pt_extend::TimerTick(duration);
            start = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main() {
    std::jthread t1{SysTick};
    t1.detach();

    // pt_extend::AddDynamicTask("F1", F1, 0);
    // pt_extend::AddStaticTask(p2, "F2", F2, 1);
    // pt_extend::AddStaticTask(p3, "F3", F3, 2);

    // pt_extend::SuspendTask(p3);

    pt_extend::AddDynamicTask("Nested", Nested, 0);

    pt_extend::RunSchedulerNoPriority();
}