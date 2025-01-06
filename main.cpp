#include "pt_extend.hpp"
#include <iostream>
#include <format>
#include <chrono>
#include <thread>

static void Resume(void* userData) {
    pt_extend_begin();
    std::cout << "[Resume]: begin\n";

    std::cout << "[Resume]: delay\n";
    pt_extend_delay(1000);

    std::cout << "[Resume]: resume\n";
    pt_extend::ResumeTask(*reinterpret_cast<PtExtend*>(userData));

    std::cout << "[Resume]: end\n";
    pt_extend_end();
}

static bool condition = false;
static void ResumeCondition(void* userData) {
    pt_extend_begin();
    std::cout << "[ResumeCondition]: begin\n";

    std::cout << "[ResumeCondition]: delay\n";
    pt_extend_delay(1000);

    std::cout << "[ResumeCondition]: resume\n";
    condition = true;

    std::cout << "[ResumeCondition]: end\n";
    pt_extend_end();
}

void NestNestedFunc(void*) {
    pt_extend_begin();
    std::cout << "[NestNestedFunc]: begin\n";

    std::cout << "[NestNestedFunc]: yeild\n";
    pt_extend_yeild();

    std::cout << "[NestNestedFunc]: suspend\n";
    pt_extend::AddDynamicTask("Resume", Resume, 0, pt_extend::GetCurrentTask());
    pt_extend_suspend_self();
    std::cout << "[NestNestedFunc]: resume from resume\n";

    std::cout << "[NestNestedFunc]: wait test\n";
    pt_extend::AddDynamicTask("ResumeCondition", ResumeCondition, 0, nullptr);
    pt_extend_wait(condition);
    std::cout << "[NestNestedFunc]: resume from wait\n";

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
    std::cout << std::endl;

    std::jthread t1{SysTick};
    t1.detach();

    pt_extend::AddDynamicTask("Nested", Nested, 0);

    pt_extend::RunSchedulerNoPriority();
}