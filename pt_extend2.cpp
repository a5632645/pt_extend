#include "pt_extend2.hpp"
#include <format>
#include <iostream>
#include <atomic>

namespace pt_extend {

// --------------------------------------------------------------------------------
// RefList
// --------------------------------------------------------------------------------
struct RefList {
    PtExtend* head_;
    PtExtend* tail_;
};

static void AddToListEnd(RefList& list, PtExtend* pt) {
    pt->prev_ = list.tail_;
    pt->next_ = nullptr;
    if (list.tail_) {
        list.tail_->next_ = pt;
    } else {
        list.head_ = pt;
    }
    list.tail_ = pt;
}

static void RemoveFromList(RefList& list, PtExtend* pt) {
    auto* prev = pt->prev_;
    auto* next = pt->next_;
    if (prev) {
        prev->next_ = next;
    } else {
        list.head_ = next;
    }
    if (next) {
        next->prev_ = prev;
    } else {
        list.tail_ = prev;
    }
    pt->prev_ = nullptr;
    pt->next_ = nullptr;
}

// --------------------------------------------------------------------------------
// Detail List
// --------------------------------------------------------------------------------
static RefList waitList = {nullptr, nullptr};
static RefList readyList = {nullptr, nullptr};
void RemoveFromReadyAddToWaitList(PtExtend* pt) {
    RemoveFromList(readyList, pt);
    AddToListEnd(waitList, pt);
}

static void AddToReadyList(PtExtend* pt) {
#if PT_EXTEND_ENABLE_PRIORITY
    auto* pti = readyList.head_;
    while (pti != nullptr && pti->prioty_ > pt->prioty_) {
        pti = pti->next_;
    }
    if (pti == nullptr) {
        AddToListEnd(readyList, pt);
    }
    else {
        pt->next_ = pti;
        if (pti->prev_) {
            pti->prev_->next_ = pt;
        }
        pt->prev_ = pti->prev_;
        pti->prev_ = pt;
        if (readyList.head_ == pti) {
            readyList.head_ = pt;
        }
    }
#else
    AddToListEnd(readyList, pt);
#endif
}

void RemoveFromWaitListAndAddToReady(PtExtend* pt) {
    RemoveFromList(waitList, pt);
    AddToReadyList(pt);
}

void RemoveFromReadyList(PtExtend* pt) {
    RemoveFromList(readyList, pt);
}

// --------------------------------------------------------------------------------
// Task
// --------------------------------------------------------------------------------
#if PT_EXTEND_NEST_SUPPORT
void AddStaticTask(PtExtend& staticTCB, std::string_view name, void (*code)(void* userData), uint32_t prioty, pt* ptCallStack, void* userData) {
#if PT_EXTEND_ENABLE_PRIORITY
    staticTCB.prioty_ = prioty;
#endif
    staticTCB.taskCode_ = code;
    staticTCB.userData_ = userData;
    staticTCB.flags.dynamic = 0;
    staticTCB.flags.suspend = 0;
    staticTCB.flags.dynamicStack = 0;
    staticTCB.name_ = name;
    staticTCB.ptCallStack = ptCallStack;
    AddToReadyList(&staticTCB);
}

#if PT_EXTEND_ENABLE_DYNAMIC_TASK
PtExtend* AddDynamicTask(std::string_view name, void (*code)(void* userData), uint32_t prioty, pt* ptCallStack, void* userData) {
    auto* pt = new(std::nothrow) PtExtend;
    if (!pt) {
        return nullptr;
    }
#if PT_EXTEND_ENABLE_PRIORITY
    pt->prioty_ = prioty;
#endif
    pt->taskCode_ = code;
    pt->userData_ = userData;
    pt->flags.dynamic = 1;
    pt->flags.suspend = 0;
    pt->flags.dynamicStack = 0;
    pt->name_ = name;
    pt->ptCallStack = ptCallStack;
    AddToReadyList(pt);
    return pt;
}

PtExtend* AddDynamicTask(std::string_view name, void (*code)(void *userData), uint32_t prioty, uint32_t stackDepth, void *userData) {
    auto* stack = new(std::nothrow) pt[stackDepth];
    if (stack == nullptr) {
        return nullptr;
    }
    for (uint32_t i = 0; i < stackDepth; i++) {
        stack[i] = pt_init();
    }

    auto* pt = AddDynamicTask(name, code, prioty, stack, userData);
    if (pt == nullptr) {
        delete[] stack;
        return nullptr;
    }
    pt->flags.dynamicStack = 1;
    return pt;
}

#endif
#else
void AddStaticTask(PtExtend& staticTCB, std::string_view name, void (*code)(void* userData), uint32_t prioty, void* userData) {
#if PT_EXTEND_ENABLE_PRIORITY
    staticTCB.prioty_ = prioty;
#endif
    staticTCB.taskCode_ = code;
    staticTCB.userData_ = userData;
    staticTCB.flags.dynamic = 0;
    staticTCB.flags.suspend = 0;
    staticTCB.name_ = name;
    AddToReadyList(&staticTCB);
}

#if PT_EXTEND_ENABLE_DYNAMIC_TASK
PtExtend* AddDynamicTask(std::string_view name, void (*code)(void* userData), uint32_t prioty, void* userData) {
    auto* pt = new(std::nothrow) PtExtend;
    if (!pt) {
        return nullptr;
    }
#if PT_EXTEND_ENABLE_PRIORITY
    pt->prioty_ = prioty;
#endif
    pt->taskCode_ = code;
    pt->userData_ = userData;
    pt->flags.dynamic = 1;
    pt->flags.suspend = 0;
    pt->name_ = name;
    AddToReadyList(pt);
    return pt;
}
#endif
#endif

void SuspendTask(PtExtend& pt) {
    pt.flags.suspend = 1;
}

void ResumeTask(PtExtend& pt) {
    pt.flags.suspend = 0;
}

// --------------------------------------------------------------------------------
// Delay
// --------------------------------------------------------------------------------
static std::atomic<uint32_t> tickEscape = 0;
void TimerTick(uint32_t tickPlus) {
    tickEscape += tickPlus;
}

// --------------------------------------------------------------------------------
// Idle
// --------------------------------------------------------------------------------
#if PT_EXTEND_COUNT_TASK_TICKS
static constexpr uint32_t kTicksPerSecond = Ms2Ticks(1000);
uint32_t clearTickCounter = 0;
#endif
void IdleTask(void*) {
    if (tickEscape <= 0) {
        return;
    }

#if PT_EXTEND_COUNT_TASK_TICKS
    clearTickCounter += tickEscape;
    if (clearTickCounter >= kTicksPerSecond) {
        auto* t = readyList.head_;
        while (t) {
            t->taskTicksReal_ = t->taskTicks_;
            t->taskTicks_ = 0;
            t = t->next_;
        }
        clearTickCounter = 0;

        t = waitList.head_;
        while (t) {
            t->taskTicksReal_ = t->taskTicks_;
            t->taskTicks_ = 0;
            t = t->next_;
        }
    }
#endif

    auto* pt = waitList.head_;
    while (pt) {
        auto* next = pt->next_;
        pt->delay_ -= tickEscape;
        if (pt->delay_ <= 0) {
            RemoveFromWaitListAndAddToReady(pt);
            pt->pt_.status = PT_STATUS_BLOCKED;
        }
        pt = next;
    }

    tickEscape = 0;
}
static PtExtend ptIdle = {
    .taskCode_ = &IdleTask
};

// --------------------------------------------------------------------------------
// Scheduler
// --------------------------------------------------------------------------------
static PtExtend* pCurrentTask = nullptr;
uint32_t nestingLevel = 0;

#if PT_EXTEND_ENABLE_PRIORITY
PtExtend& GetPriotyTask() {
    if (waitList.head_ == nullptr) {
        tickEscape = 0;
    }
    if (tickEscape > 0 || readyList.head_ == nullptr) {
        return ptIdle;
    }
    return *readyList.head_;
}
#endif

PtExtend *GetCurrentTask() {
    return pCurrentTask;
}

#if PT_EXTEND_ENABLE_DYNAMIC_TASK
void DynamicDeleteCurrent() {
    #if PT_EXTEND_NEST_SUPPORT
    if (pCurrentTask->flags.dynamicStack) {
        delete pCurrentTask->ptCallStack;
    }
    #endif
    delete pCurrentTask;
    pCurrentTask = nullptr;
}
#endif

void SetCurrentTask(PtExtend& pt) {
    pCurrentTask = &pt;
}

#if PT_EXTEND_ENABLE_PRIORITY
void RunScheduler() {
    for (;;) {
        auto& task = GetPriotyTask();
        pCurrentTask = &task;
        task.taskCode_();
    }
}
#endif

void RunSchedulerNoPriority() {
    for (;;) {
        if (waitList.head_ == nullptr) {
            tickEscape = 0;
        }

        if (tickEscape > 0 || readyList.head_ == nullptr) {
            pCurrentTask = &ptIdle;
            ptIdle.taskCode_(nullptr);
        }

        pCurrentTask = readyList.head_;
        while (pCurrentTask) {
            auto* next = pCurrentTask->next_;
            if (!pCurrentTask->flags.suspend) {
#if PT_EXTEND_COUNT_TASK_TICKS
                uint32_t tickBegin = tickEscape;
#endif
                pCurrentTask->taskCode_(pCurrentTask->userData_);
#if PT_EXTEND_COUNT_TASK_TICKS
                uint32_t tickEnd = tickEscape;
                if (pCurrentTask != nullptr) {
                    pCurrentTask->taskTicks_ += tickEnd - tickBegin;
                }
#endif
            }
            pCurrentTask = next;
        }
    }
}

#if PT_EXTEND_COUNT_TASK_TICKS
void PrintTaskTicks() {
    std::cout << "########################################\n";

    auto* pt = readyList.head_;
    while (pt) {
        std::cout << std::format("# name: {}, ticks: {}\n", pt->name_, pt->taskTicksReal_);
        pt = pt->next_;
    }

    pt = waitList.head_;
    while (pt) {
        std::cout << std::format("# name: {}, ticks: {}\n", pt->name_, pt->taskTicksReal_);
        pt = pt->next_;
    }

    std::cout << "########################################\n";
}
#endif

pt* GetCurrentCallPt() {
#if PT_EXTEND_NEST_SUPPORT
    if (nestingLevel == 0) {
        return &pCurrentTask->pt_;
    }
    return &pCurrentTask->ptCallStack[nestingLevel - 1];
#else
    return &pCurrentTask->pt_;
#endif
}

}