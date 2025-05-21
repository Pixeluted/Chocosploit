//
// Created by Pixeluted on 02/02/2025.
//

#define RELEASE_DLL 0

#include <format>
#include <thread>

#include "Logger.hpp"
#include "Communication.hpp"
#include "Managers/FileSystemManager.hpp"
#include "Managers/CClosureManager.hpp"
#include "Execution/InternalTaskScheduler.hpp"
#include "Roblox/TaskScheduler.hpp"
#include "oxorany_include.h"
#include "Managers/Render/RenderManager.hpp"

void entry(uintptr_t dllBase) {
#if RELEASE_DLL == 1
    dllBase = dllBase ^ oxorany(0x8A3FC9D71E5B46F2);
#endif
    const auto communication = ApplicationContext::AddService<Communication>();
    while (!communication->IsConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    const auto logger = ApplicationContext::AddService<Logger>();
    logger->Log(Info, "DLL Loaded!");

    const auto ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS *>(
        dllBase + reinterpret_cast<IMAGE_DOS_HEADER *>(dllBase)->e_lfanew);
    const auto imageSize = ntHeaders->OptionalHeader.SizeOfImage;
    const auto cfgBitmap = *reinterpret_cast<uintptr_t *>(CfgBitmapPointer);
    for (auto currentAddress = dllBase; currentAddress < dllBase + imageSize; currentAddress += 0x10000) {
        const auto pageBitAddress = reinterpret_cast<int *>(cfgBitmap + (currentAddress >> 0x13));
        *pageBitAddress = 0xFF;
    }
    logger->Log(Info, "Successfully bypassed CFG");

#if RELEASE_DLL == 1
    memset(reinterpret_cast<void *>(dllBase), 0xDEADBEEF, 0x1000);
#endif

    if (ApplicationContext::AddService<TaskScheduler>() == nullptr) return;
    if (ApplicationContext::AddService<FileSystemManager>() == nullptr) return;
    if (ApplicationContext::AddService<CClosureManager>() == nullptr) return;
    if (ApplicationContext::AddService<RenderManager>() == nullptr) return;
    const auto taskScheduler = ApplicationContext::AddService<InternalTaskScheduler>();

    taskScheduler->IdleState(false);
}

BOOL WINAPI DllMain(
    HINSTANCE dllHandle,
    DWORD callReason,
    LPVOID reserved
) {
    switch (callReason) {
        case DLL_PROCESS_ATTACH:
            std::thread(entry, reinterpret_cast<uintptr_t>(dllHandle)).detach();
            break;
        default:
            break;
    }

    return TRUE;
}
