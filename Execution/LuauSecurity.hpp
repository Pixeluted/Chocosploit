//
// Created by Pixeluted on 07/02/2025.
//
#pragma once
#include <cstdint>
#include <memory>

#include "lobject.h"
#include "lua.h"

#define basicCapabilities 0x3ffffff00ull
#define ourCapability (1ull << 48ull)

struct LuauUserdata {
    char pad_0000[48]; //0x0000
    uint64_t Identity; //0x0030
    char pad_0038[16]; //0x0038
    uint64_t Capabilities; //0x0048
    std::weak_ptr<uintptr_t> Script; //0x0050
};

void ElevateThread(lua_State* L, uint64_t identity, bool addOurCapability, bool isInstant = false);
void ElevateProto(Closure* closure, uint64_t identity, bool addOurCapability);
bool IsOurThread(lua_State* L);
bool IsOurProto(const Proto* proto);