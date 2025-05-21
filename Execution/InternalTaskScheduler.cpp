//
// Created by Pixeluted on 04/02/2025.
//

#include "InternalTaskScheduler.hpp"

#include <format>
#include <fstream>
#include <ranges>
#include <thread>
#include <utility>

#include "BytecodeCompiler.hpp"
#include "ldo.h"
#include "lgc.h"
#include "lobject.h"
#include "lstate.h"
#include "lua.h"
#include "lualib.h"
#include "LuauSecurity.hpp"
#include "../Managers/CClosureManager.hpp"
#include "../OffsetsAndFunctions.hpp"
#include "../Environment/EnvironmentManager.hpp"
#include "../Managers/FileSystemManager.hpp"
#include "../Managers/Render/RenderManager.hpp"

InternalTaskScheduler::InternalTaskScheduler() {
    this->currentState = new SchedulerState;
    this->scheduledItems = new SchedulerLists;
    this->luaStates = new LuaStates;
    isInitialized = true;
}

void InternalTaskScheduler::OnTeleport(const bool wasTeleport) {
    currentState->lockedStep = true;
    const auto logger = ApplicationContext::GetService<Logger>();
    if (!wasTeleport) {
        logger->Log(Info, "User has left the game!");
    } else {
        logger->Log(Info, "User has teleported between games!");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    this->IdleState(wasTeleport);
}

uintptr_t __fastcall stepHook(uintptr_t a, uintptr_t b, uintptr_t c) {
    const auto internalTaskScheduler = ApplicationContext::GetService<InternalTaskScheduler>();
    auto currentSchedulerState = internalTaskScheduler->currentState;
    if (!currentSchedulerState->originalStepFunction)
        return 0;
    if (currentSchedulerState->lockedStep)
        return currentSchedulerState->originalStepFunction(a, b, c);

    if (!currentSchedulerState->isInitializedOnGame) {
        const auto scriptContext = *reinterpret_cast<uintptr_t *>(
            currentSchedulerState->lastWHSJ.jobAddress +
            ChocoSploit::StructOffsets::TaskScheduler::Job::WaitingHybridScriptsJob::ScriptContext);
        currentSchedulerState->currentScriptContext = scriptContext;

        const auto sharedGlobals = *reinterpret_cast<uintptr_t *>(
            scriptContext + ChocoSploit::StructOffsets::ScriptContext::SharedGlobals);
        if (sharedGlobals == 0)
            MessageBoxA(nullptr, "SHARED GLOBALS POINTER IS NULLPTR", "FATAL", MB_ICONERROR | MB_TOPMOST);
        const auto weakObjectRef = *reinterpret_cast<uintptr_t *>(sharedGlobals + 0x8);
        const auto sharedRef = *reinterpret_cast<int *>(*reinterpret_cast<uintptr_t *>(weakObjectRef + 0x20) + 0x14);
        const auto GRef = *reinterpret_cast<int *>(
            *reinterpret_cast<uintptr_t *>(*reinterpret_cast<uintptr_t *>(weakObjectRef + 0x18) + 0x20) + 0x14);
        currentSchedulerState->GRef = GRef;
        currentSchedulerState->sharedRef = sharedRef;

        uintptr_t Z = 0;
        const auto encryptedGlobalState = GetGlobalState(
            scriptContext + ChocoSploit::StructOffsets::ScriptContext::GlobalStateMap, &Z, &Z);
        const auto decryptedGlobalState = reinterpret_cast<lua_State *>(
            DecryptGlobalState(encryptedGlobalState + ChocoSploit::StructOffsets::ScriptContext::VmState::LuaState));

        ApplicationContext::GetService<Logger>()->Log(Info, std::format("Got global lua state: 0x{:x}",
                                                                        reinterpret_cast<uintptr_t>(
                                                                            decryptedGlobalState)));

        const auto executorLuaState = lua_newthread(decryptedGlobalState);
        lua_ref(decryptedGlobalState, -1);
        lua_pop(decryptedGlobalState, 1);
        luaL_sandboxthread(executorLuaState);
        ElevateThread(executorLuaState, 8, true);

        lua_createtable(executorLuaState, 0, 0);
        lua_setglobal(executorLuaState, "_G");

        lua_createtable(executorLuaState, 0, 0);
        lua_setglobal(executorLuaState, "shared");

        lua_getglobal(executorLuaState, "error");
        currentSchedulerState->errorFuncRef = lua_ref(executorLuaState, -1);
        lua_pop(executorLuaState, 1);

        PushEnvironment(executorLuaState);

        auto currentLuaStates = internalTaskScheduler->luaStates;
        currentLuaStates->executorLuaState = executorLuaState;
        currentLuaStates->robloxLuaState = decryptedGlobalState;

        const auto synchronizedDispatcherWorker = lua_newthread(executorLuaState);
        lua_ref(executorLuaState, -1);
        luaL_sandboxthread(synchronizedDispatcherWorker);
        ElevateThread(synchronizedDispatcherWorker, 8, true);

        lua_pushvalue(synchronizedDispatcherWorker, LUA_GLOBALSINDEX);
        lua_setglobal(synchronizedDispatcherWorker, "_ENV");

        currentLuaStates->synchronizedDispatcherWorker = synchronizedDispatcherWorker;

        auto& scriptsList = internalTaskScheduler->scheduledItems->scheduledTeleportScripts;
        for (auto current = scriptsList.begin(); current != scriptsList.end();) {
            internalTaskScheduler->ScheduleExecution(*current);
            current = scriptsList.erase(current);
        }

        for (const auto filesystemManager = ApplicationContext::GetService<FileSystemManager>(); const auto &
             fileToExecute: fs::directory_iterator(filesystemManager->autoexecDirectory)) {
            std::ifstream file(fileToExecute.path(), std::ios::binary | std::ios::ate);
            const auto fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            std::string fileContent;
            fileContent.resize(fileSize);
            file.read(fileContent.data(), fileSize);

            internalTaskScheduler->ScheduleExecution(fileContent);
        }

        currentSchedulerState->isInitializedOnGame = true;
    } else {
        internalTaskScheduler->Step();
    }

    return currentSchedulerState->originalStepFunction(a, b, c);
}

void InternalTaskScheduler::IdleState(const bool wasTeleport) {
    ApplicationContext::GetService<RenderManager>()->ClearAllObjects();
    this->luaStates->ClearStates();
    this->scheduledItems->ClearLists(wasTeleport);
    this->currentState->ClearState();
    if (!TaskScheduler::IsInGame()) {
        while (!TaskScheduler::IsInGame()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    const auto cClosureManager = ApplicationContext::GetService<CClosureManager>();
    cClosureManager->newCClosureMap.clear();
    cClosureManager->executorCClosures.clear();

    const auto logger = ApplicationContext::GetService<Logger>();
    const auto gameWaitingHybridScriptsJobOpt = ApplicationContext::GetService<TaskScheduler>()->FindGameWhsj(
        currentState->lastWHSJ.jobAddress);
    if (!gameWaitingHybridScriptsJobOpt.has_value()) {
        logger->Log(Error,
                    "Cannot initialize: Failed to find game's WaitingHybridScriptsJob! Trying again in 5 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return this->IdleState(wasTeleport);
    }
    currentState->lastWHSJ = gameWaitingHybridScriptsJobOpt.value();

    if (currentState->ourVFtable == nullptr) {
        const auto jobVFtable = *reinterpret_cast<uintptr_t **>(currentState->lastWHSJ.jobAddress);
        const auto newVFtable = new uintptr_t[7];
        memcpy(newVFtable, jobVFtable, sizeof(uintptr_t) * 7);

        currentState->originalStepFunction = reinterpret_cast<SchedulerState::JobStep_t>(jobVFtable[6]);
        newVFtable[6] = reinterpret_cast<uintptr_t>(&stepHook);

        currentState->ourVFtable = newVFtable;
    }

    currentState->lockedStep = false;
    *reinterpret_cast<uintptr_t **>(currentState->lastWHSJ.jobAddress) = static_cast<uintptr_t *>(currentState->
        ourVFtable);
    currentState->lastKnownDataModel = *reinterpret_cast<uintptr_t *>(DatamodelPointer + 0x8);
    logger->Log(Info, "Hooked WaitingHybridScriptsJob step!");

    while (true) {
        const auto currentDataModel = *reinterpret_cast<uintptr_t *>(DatamodelPointer + 0x8);
        if (currentDataModel != currentState->lastKnownDataModel && TaskScheduler::IsInGame()) {
            std::thread(&InternalTaskScheduler::OnTeleport, this, true).detach();
            break;
        } else if (currentDataModel != currentState->lastKnownDataModel && !TaskScheduler::IsInGame()) {
            std::thread(&InternalTaskScheduler::OnTeleport, this, false).detach();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void InternalTaskScheduler::ScheduleExecution(const std::string &scriptToExecute) {
    std::lock_guard queueLock(executeListMutex);
    scheduledItems->scheduledScriptsToExecute.push_back(scriptToExecute);
}

void InternalTaskScheduler::ScheduleLuaDispatch(const std::function<void(lua_State *L)> &dispatchCallback) const {
    scheduledItems->synchronizedCodeDispatch.emplace_back(dispatchCallback);
}

void InternalTaskScheduler::ProcessNextYield() {
    if (scheduledItems->queuedYields.empty())
        return;
    const auto yieldToProcess = scheduledItems->queuedYields.front();
    scheduledItems->queuedYields.pop();
    ++currentState->activeYieldThreads;

    std::thread([this, yieldToProcess] {
        try {
            yieldToProcess->yieldFunction(yieldToProcess->associatedYieldRequest);
        } catch (...) {
            MessageBoxA(nullptr, "Unexpected error occurred in yield!", "Yield Error", MB_ICONERROR);
        }

        --currentState->activeYieldThreads;
        if (currentState->activeYieldThreads < MAX_YIELD_THREADS) {
            ProcessNextYield();
        }
    }).detach();
}

void InternalTaskScheduler::ScheduleYield(lua_State *L,
                                          const std::function<void(std::shared_ptr<YieldingRequest>)> &yieldFunc) {
    lua_pushthread(L);
    const auto threadRef = lua_ref(L, -1);
    lua_pop(L, 1);

    const auto yieldRequest = std::make_shared<YieldingRequest>(YieldingRequest{
        false, nullptr, {0, L, threadRef, 0}
    });
    const auto yieldFunctionSchedule = std::make_shared<YieldFunctionSchedule>(YieldFunctionSchedule{
        yieldRequest, yieldFunc
    });
    scheduledItems->activeYields.push_back(yieldRequest);
    scheduledItems->queuedYields.push(yieldFunctionSchedule);

    if (currentState->activeYieldThreads < MAX_YIELD_THREADS) {
        ProcessNextYield();
    }
}


void InternalTaskScheduler::Step() {
    if (!executeListMutex.try_lock())
        return;

    auto &executeList = scheduledItems->scheduledScriptsToExecute;
    for (auto current = executeList.begin(); current != executeList.end();) {
        const auto newLuaThread = lua_newthread(luaStates->executorLuaState);
        luaL_sandboxthread(newLuaThread);

        lua_pushvalue(newLuaThread, LUA_GLOBALSINDEX);
        lua_setglobal(newLuaThread, "_ENV");

        ElevateThread(newLuaThread, 8, true);
        const auto compiledBytecode = CompileBytecode(*current);
        if (luau_load(newLuaThread, EXECUTOR_CHUNK_NAME, compiledBytecode.c_str(), compiledBytecode.length(), 0) !=
            LUA_OK) {
            Printf(ChocoSploit::FunctionDefinitions::Error, lua_tostring(newLuaThread, -1));
            current = executeList.erase(current);
            lua_pop(luaStates->executorLuaState, 1);
            continue;
        }

        const auto loadedClosure = lua_toclosure(newLuaThread, -1);
        ElevateProto(loadedClosure, 8, true);

        Task_Defer(newLuaThread);
        lua_pop(luaStates->executorLuaState, 1);

        current = executeList.erase(current);
    }

    auto &yieldingList = scheduledItems->activeYields;
    for (auto current = yieldingList.begin(); current != yieldingList.end();) {
        const auto &yieldRequest = *current;
        if (!yieldRequest->isReady) {
            ++current;
            continue;
        }

        const auto L = yieldRequest->threadRef.thread;
        const auto yieldingResults = yieldRequest->completionCallback(L);
        if (!yieldingResults.isSuccess) {
            lua_settop(L, 0);
            lua_getref(L, currentState->errorFuncRef);
            lua_pushstring(L, yieldingResults.errorMessage);
            Task_Defer(L);
            current = yieldingList.erase(current);
            continue;
        }

        auto threadRef = &yieldRequest->threadRef;
        int64_t results[0x2]{0};

        resumeThread(
            reinterpret_cast<void *>(currentState->currentScriptContext +
                                     ChocoSploit::StructOffsets::ScriptContext::ResumeFaceOffset), results,
            &threadRef,
            yieldingResults.nRet,
            false,
            "");

        current = yieldingList.erase(current);
    }

    auto &codeDispatchList = scheduledItems->synchronizedCodeDispatch;
    for (auto current = codeDispatchList.begin(); current != codeDispatchList.end();) {
        auto &currentDispatch = *current;
        currentDispatch(luaStates->synchronizedDispatcherWorker);
        lua_resetthread(luaStates->synchronizedDispatcherWorker);
        current = codeDispatchList.erase(current);
    }

    executeListMutex.unlock();
}
