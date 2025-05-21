//
// Created by Pixeluted on 04/02/2025.
//
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

#include "lua.h"
#include "../ApplicationContext.hpp"
#include "../Roblox/TaskScheduler.hpp"
#include "../OffsetsAndFunctions.hpp"

struct YieldResult {
    bool isSuccess = false;
    int32_t nRet = 0;
    const char* errorMessage = nullptr;
};

struct YieldingRequest {
    bool isReady;
    std::function<YieldResult(lua_State*)> completionCallback;
    WeakThreadRef threadRef;
};

struct YieldFunctionSchedule {
    std::shared_ptr<YieldingRequest> associatedYieldRequest;
    std::function<void(std::shared_ptr<YieldingRequest>)> yieldFunction;
};

struct LuaStates {
    lua_State* executorLuaState = nullptr;
    lua_State* robloxLuaState = nullptr;
    lua_State* synchronizedDispatcherWorker = nullptr;

    void ClearStates() {
        executorLuaState = nullptr;
        robloxLuaState = nullptr;
        synchronizedDispatcherWorker = nullptr;
    }
};

struct SchedulerLists {
    std::list<std::string> scheduledScriptsToExecute;
    std::list<std::string> scheduledTeleportScripts;
    std::list<std::function<void(lua_State* L)>> synchronizedCodeDispatch;
    std::list<std::shared_ptr<YieldingRequest>> activeYields;
    std::queue<std::shared_ptr<YieldFunctionSchedule>> queuedYields;

    void ClearLists(const bool wasTeleport) {
        scheduledScriptsToExecute.clear();
        synchronizedCodeDispatch.clear();
        while (!queuedYields.empty()) {
            queuedYields.pop();
        }
        if (!wasTeleport) {
            scheduledTeleportScripts.clear();
        }
        activeYields.clear();
    }
};

constexpr int MAX_YIELD_THREADS = 3;
struct SchedulerState {
    using JobStep_t = uintptr_t(__fastcall*)(uintptr_t a, uintptr_t b, uintptr_t c);

    void* ourVFtable = nullptr;
    JobStep_t originalStepFunction = nullptr;
    int errorFuncRef = 0x0;
    int GRef = 0x0;
    int sharedRef = 0x0;
    int customRenvRef = 0x0;
    uintptr_t lastKnownDataModel = 0x0;
    uintptr_t currentScriptContext = 0x0;
    RobloxJob lastWHSJ{};
    bool isInitializedOnGame = false;
    bool lockedStep = true;
    std::atomic_int activeYieldThreads = 0;

    void ClearState() {
        errorFuncRef = 0;
        lastKnownDataModel = 0x0;
        currentScriptContext = 0x0;
        GRef = 0x0;
        sharedRef = 0x0;
        customRenvRef = 0x0;
        activeYieldThreads = 0x0;
        isInitializedOnGame = false;
    }
};

class InternalTaskScheduler final : public Service {
    std::mutex executeListMutex;

    void ProcessNextYield();
public:
    LuaStates* luaStates;
    SchedulerLists* scheduledItems;
    SchedulerState* currentState;

    InternalTaskScheduler();
    void ScheduleExecution(const std::string& scriptToExecute);
    void ScheduleYield(lua_State* L, const std::function<void(std::shared_ptr<YieldingRequest>)>& yieldFunc);
    void ScheduleLuaDispatch(const std::function<void(lua_State *L)>& dispatchCallback) const;
    void OnTeleport(bool wasTeleport);
    void Step();
    void IdleState(bool wasTeleport);
};



