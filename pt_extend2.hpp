/*
 * ProtoThread Extend 2 
 * Use pt Array as call stack
*/

#pragma once
#include <cstdint>
#include <string_view>
#include "pt.h"

/* 启动动态分配 */
#define PT_EXTEND_ENABLE_DYNAMIC_ALLOC 0
/* 任务Tick计时 */
#define PT_EXTEND_COUNT_TASK_TICKS 0
/* 启用协程嵌套 */
#define PT_EXTEND_NEST_SUPPORT 1

struct PtExtend {
    PtExtend* next_{};
    PtExtend* prev_{};

    pt pt_ = pt_init();
    int32_t delay_{};

#if PT_EXTEND_COUNT_TASK_TICKS
    uint32_t taskTicks_{}; /* task ticks in 1 second */
    uint32_t taskTicksReal_{};
#endif
    struct {
        uint8_t suspend : 1;
        uint8_t dynamic : 1;
        uint8_t dynamicStack : 1;
    } flags;

    void(*taskCode_)(void*);
    void* userData_;
    std::string_view name_;

#if PT_EXTEND_NEST_SUPPORT
    pt* ptCallStack = nullptr;
#endif
};

namespace pt_extend {

/* config */
static constexpr int kTickRate = 1000;
static constexpr int Ms2Ticks(int ms) { return ms * kTickRate / 1000; }
static constexpr int Ticks2Ms(int ticks) { return ticks * 1000 / kTickRate; }

void RemoveFromReadyAddToWaitList(PtExtend* pt);
void RemoveFromWaitListAndAddToReady(PtExtend* pt);
void RemoveFromReadyList(PtExtend* pt);

PtExtend* GetCurrentTask();
pt* GetCurrentCallPt();

#if PT_EXTEND_ENABLE_DYNAMIC_ALLOC
void DynamicDeleteCurrent();
#endif
void SetCurrentTask(PtExtend& pt);

/* public */
void TimerTick(uint32_t tickPlus);
/* 可以使用pt_extend_wait直接等待普通变量 */
void RunSchedulerNoPriority();

#if PT_EXTEND_NEST_SUPPORT
void AddStaticTask(PtExtend& staticTCB, std::string_view name, void(*code)(void* userData), pt* ptCallStack, void* userData = nullptr);
#else
void AddStaticTask(PtExtend& staticTCB, std::string_view name, void(*code)(void* userData), void* userData = nullptr);
#endif
#if PT_EXTEND_ENABLE_DYNAMIC_ALLOC
#if PT_EXTEND_NEST_SUPPORT
PtExtend* AddDynamicTask(std::string_view name, void(*code)(void* userData), pt* ptCallStack, void* userData = nullptr);
PtExtend* AddDynamicTask(std::string_view name, void(*code)(void* userData), uint32_t stackDepth, void* userData = nullptr);
#else
PtExtend* AddDynamicTask(std::string_view name, void(*code)(void* userData), void* userData = nullptr);
#endif
#endif

void SuspendTask(PtExtend& pt);
void ResumeTask(PtExtend& pt);

#if PT_EXTEND_COUNT_TASK_TICKS
void PrintTaskTicks();
#endif

#if PT_EXTEND_NEST_SUPPORT
/* 协程函数嵌套 */
extern uint32_t nestingLevel;
#endif

}

// --------------------------------------------------------------------------------
// 阻止重复label
// --------------------------------------------------------------------------------
#define _pt_extend_line3(name, line) _pt_##line##name##line
#define _pt_extend_line2(name, line) _pt_extend_line3(name, line)
#define _pt_extend_line(name) _pt_extend_line2(name, __LINE__)

#define _pt_extend_unduplicate_label(ptt, st)\
    do {\
        (ptt)->status = (st);\
        _pt_extend_line(label) : (ptt)->label = &&_pt_extend_line(label);\
    } while (0)

#define _pt_extend_unduplicate_end(pt) _pt_extend_unduplicate_label(pt, PT_STATUS_FINISHED)

#define _pt_extend_unduplicate_yield(pt)\
    do {\
        _pt_extend_unduplicate_label(pt, PT_STATUS_YIELDED);\
        if (pt_status(pt) == PT_STATUS_YIELDED) {\
        (pt)->status = PT_STATUS_BLOCKED;\
        return;\
        }\
    } while (0)

#define _pt_extend_unduplicate_wait(pt, cond)\
    do {\
        _pt_extend_unduplicate_label(pt, PT_STATUS_BLOCKED);\
        if (!(cond)) {\
        return;\
        }\
    } while (0)

// --------------------------------------------------------------------------------
// API
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// Delay
// --------------------------------------------------------------------------------
/* 协程延时 */
#define pt_extend_co_delay(ms)\
    do {\
        pt_extend::GetCurrentTask()->delay_ = (pt_extend::Ms2Ticks((ms)));\
        pt_extend::RemoveFromReadyAddToWaitList(pt_extend::GetCurrentTask());\
        pt_label(&pt_extend::GetCurrentTask()->pt_, PT_STATUS_YIELDED); \
        if (pt_status(&pt_extend::GetCurrentTask()->pt_) == PT_STATUS_YIELDED) {\
            return;\
        }\
    } while (0)

#if PT_EXTEND_NEST_SUPPORT
/* 协程嵌套延时 */
#define pt_extend_nest_delay(ms)\
    do {\
        pt_extend::GetCurrentTask()->delay_ = (pt_extend::Ms2Ticks((ms)));\
        pt_extend::RemoveFromReadyAddToWaitList(pt_extend::GetCurrentTask());\
        pt_extend::GetCurrentTask()->pt_.status = PT_STATUS_YIELDED;\
        _pt_extend_unduplicate_label(pt_extend::GetCurrentCallPt(), PT_STATUS_BLOCKED);\
        if (pt_status(&pt_extend::GetCurrentTask()->pt_) == PT_STATUS_YIELDED) {\
            return;\
        }\
    } while(0)

/* 通用延时 */
#define pt_extend_delay(ms)\
    if (pt_extend::nestingLevel != 0) {\
        pt_extend_nest_delay(ms);\
    }\
    else {\
        pt_extend_co_delay(ms);\
    }
#else
#define pt_extend_delay(ms) pt_extend_co_delay(ms)
#endif

// --------------------------------------------------------------------------------
// Begin
// --------------------------------------------------------------------------------
/* 协程/协程嵌套函数开始 */
#define pt_extend_begin()\
    do {\
        pt_begin(pt_extend::GetCurrentCallPt());\
    } while (0)

// --------------------------------------------------------------------------------
// End
// --------------------------------------------------------------------------------
/* 静态创建的协程函数结束 */
#define pt_extend_co_static_end()\
    do {\
        pt_extend::RemoveFromReadyList(pt_extend::GetCurrentTask());\
        pt_end(&pt_extend::GetCurrentTask()->pt_);\
    } while (0)

/* 动态创建的协程函数结束 */
#if PT_EXTEND_ENABLE_DYNAMIC_ALLOC
#define pt_extend_co_dynamic_end()\
    do {\
        pt_extend::RemoveFromReadyList(pt_extend::GetCurrentTask());\
        pt_extend::DynamicDeleteCurrent();\
    } while (0)
#endif

/* 协程函数结束 */
#if PT_EXTEND_ENABLE_DYNAMIC_ALLOC
#define pt_extend_co_end()\
    do {\
        if (pt_extend::GetCurrentTask()->flags.dynamic) {\
            pt_extend_co_dynamic_end();\
        } else {\
            pt_extend_co_static_end();\
        }\
    } while (0)
#else
#define pt_extend_co_end()\
    pt_extend_co_static_end()
#endif

#if PT_EXTEND_NEST_SUPPORT
/* 协程嵌套函数结束 */
#define pt_extend_nest_end() _pt_extend_unduplicate_end(pt_extend::GetCurrentCallPt());

/* 通用结束 */
#define pt_extend_end()\
    if (pt_extend::nestingLevel != 0) {\
        pt_extend_nest_end();\
    }\
    else {\
        pt_extend_co_end();\
    }
#else
#define pt_extend_end() pt_extend_co_end()
#endif

// --------------------------------------------------------------------------------
// Yield
// --------------------------------------------------------------------------------
/* 协程/嵌套yield */
#define pt_extend_yeild() pt_yield(pt_extend::GetCurrentCallPt());

// --------------------------------------------------------------------------------
// Wait
// --------------------------------------------------------------------------------
/* 协程/嵌套等待 */
#define pt_extend_wait(cond) pt_wait(pt_extend::GetCurrentCallPt(), cond);

// --------------------------------------------------------------------------------
// Suspend
// --------------------------------------------------------------------------------
/* 协程挂起 */
#define pt_extend_suspend_self()\
    do {\
        pt_extend::SuspendTask(*pt_extend::GetCurrentTask());\
        pt_extend_yeild();\
    } while (0)

// --------------------------------------------------------------------------------
// Call
// --------------------------------------------------------------------------------
#if PT_EXTEND_NEST_SUPPORT
/* 协程/协程函数调用协程函数 */
#define pt_extend_call_begin()\
    do {\
        pt_label(pt_extend::GetCurrentCallPt(), PT_STATUS_BLOCKED);\
        ++pt_extend::nestingLevel;\
    } while(0)

#define pt_extend_call_end()\
    do {\
        if (pt_status(pt_extend::GetCurrentCallPt()) != PT_STATUS_FINISHED) {\
            --pt_extend::nestingLevel;\
            return;\
        }\
        --pt_extend::nestingLevel;\
    } while(0)

#define pt_extend_call(func, ...)\
    pt_extend_call_begin();\
    func(__VA_ARGS__);\
    pt_extend_call_end();
#endif
