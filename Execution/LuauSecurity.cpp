//
// Created by Pixeluted on 07/02/2025.
//

#include "LuauSecurity.hpp"

#include <format>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "lstate.h"
#include "../Logger.hpp"

static std::unordered_map<std::string, std::pair<uint64_t, bool> > allCapabilities = {
    {"Plugin", {0x1, false}},
    {"LocalUser", {0x2, false}},
    {"WritePlayer", {0x4, false}},
    {"RobloxScript", {0x8, false}},
    {"RobloxEngine", {0x10, false}},
    {"NotAccessible", {0x20, false}},
    {"RunClientScript", {0x8, true}},
    {"RunServerScript", {0x9, true}},
    {"AccessOutsideWrite", {0xb, true}},
    {"Unassigned", {0xf, true}},
    {"AssetRequire", {0x10, true}},
    {"LoadString", {0x11, true}},
    {"ScriptGlobals", {0x12, true}},
    {"CreateInstances", {0x13, true}},
    {"Basic", {0x14, true}},
    {"Audio", {0x15, true}},
    {"DataStore", {0x16, true}},
    {"Network", {0x17, true}},
    {"Physics", {0x18, true}},
    {"UI", {0x19, true}},
    {"CSG", {0x1a, true}},
    {"Chat", {0x1b, true}},
    {"Animation", {0x1c, true}},
    {"Avatar", {0x1d, true}},
    {"Input", {0x1e, true}},
    {"Environment", {0x1f, true}},
    {"RemoteEvent", {0x20, true}},
    {"LegacySound", {0x21, true}},
    {"Players", {0x22, true}},
    {"CapabilityControl", {0x23, true}},
    {"PluginOrOpenCloud", {0x3d, true}},
    {"Assistant", {0x3e, true}}
};

static std::unordered_map<uint8_t, std::list<std::string> > identityCapabilities = {
    {
        3,
        {
            "RunServerScript", "Plugin", "LocalUser", "RobloxScript", "RunClientScript", "AccessOutsideWrite",
            "Avatar",
            "RemoteEvent", "Environment", "Input", "LegacySound", "Players",
            "CapabilityControl"
        }
    },
    {
        2, {
            "CSG", "Chat", "Animation", "RemoteEvent", "Avatar", "LegacySound", "Players",
            "CapabilityControl"
        }
    },
    {
        4, {
            "Plugin", "LocalUser", "RemoteEvent", "Avatar", "LegacySound", "Players",
            "CapabilityControl"
        }
    },
    {
        6,
        {
            "RunServerScript", "Plugin", "LocalUser", "Avatar", "RobloxScript", "RunClientScript",
            "AccessOutsideWrite",
            "Input", "Environment", "RemoteEvent", "PluginOrOpenCloud", "LegacySound",
        }
    },
    {
        8,
        {
            "Plugin",
            "LocalUser",
            "WritePlayer",
            "RobloxScript",
            "RobloxEngine",
            "NotAccessible",
            "RunClientScript",
            "RunServerScript",
            "AccessOutsideWrite",
            "Unassigned",
            "AssetRequire",
            "LoadString",
            "ScriptGlobals",
            "CreateInstances",
            "Basic",
            "Audio",
            "DataStore",
            "Network",
            "Physics",
            "UI",
            "CSG",
            "Chat",
            "Animation",
            "Avatar",
            "Input",
            "Environment",
            "RemoteEvent",
            "PluginOrOpenCloud",
            "Assistant",
            "LegacySound",
            "Players",
            "CapabilityControl"
        }
    }
};


uint64_t identityToCapabilities(const uint64_t identity, const bool addOurCapability = false) {
    uint64_t finalCapabilities = basicCapabilities | (addOurCapability ? ourCapability : 0);
    if (!identityCapabilities.contains(identity))
        return finalCapabilities;

    const auto &identityCaps = identityCapabilities.at(identity);
    for (const auto &capabilityName: identityCaps) {
        if (!allCapabilities.contains(capabilityName))
            continue;

        const auto [capabilityValue, usingBitTest] = allCapabilities.at(capabilityName);
        finalCapabilities |= usingBitTest ? (1ull << capabilityValue) : capabilityValue;
    }

    return finalCapabilities;
}

void ElevateThread(lua_State *L, const uint64_t identity, const bool addOurCapability, const bool isInstant) {
    const auto capabilitiesToApply = identityToCapabilities(identity, addOurCapability);
    const auto threadUserdata = static_cast<LuauUserdata *>(L->userdata);

    threadUserdata->Identity = identity;
    threadUserdata->Capabilities = capabilitiesToApply;

    if (isInstant) {
        const auto identityStructure = reinterpret_cast<uintptr_t>(GetIdentityStructure(
            *reinterpret_cast<uintptr_t *>(IdentityPointer)));

        *reinterpret_cast<int64_t *>(identityStructure) = identity;
        *reinterpret_cast<int64_t *>(identityStructure + 0x20) = capabilitiesToApply;
    }
}

bool IsOurThread(lua_State *L) {
    if (L == nullptr)
        return false;
    const auto threadUserdata = static_cast<LuauUserdata*>(L->userdata);
    return threadUserdata != nullptr &&
           threadUserdata->Script.lock() == nullptr && (
               threadUserdata->Capabilities & ourCapability) == ourCapability;
}

bool IsOurProtoInternal(const Proto *proto, std::unordered_set<const Proto *> &visited) {
    if (!proto || !proto->userdata || visited.contains(proto)) {
        return false;
    }
    visited.insert(proto);

    const auto *capsPtr = static_cast<const uint64_t *>(proto->userdata);
    if ((*capsPtr & ourCapability) == ourCapability) {
        return true;
    }

    if (proto->p && proto->sizep > 0) {
        for (int i = 0; i < proto->sizep; i++) {
            const auto currentProto = proto->p[i];
            if (IsOurProtoInternal(currentProto, visited)) {
                return true;
            }
        }
    }

    return false;
}

bool IsOurProto(const Proto *proto) {
    std::unordered_set<const Proto *> visited;
    return IsOurProtoInternal(proto, visited);
}

void ElevateProtoInternal(Proto *proto, uint64_t *capabilityToSet) {
    proto->userdata = capabilityToSet;

    Proto **protoProtos = proto->p;
    for (int i = 0; i < proto->sizep; i++) {
        ElevateProtoInternal(protoProtos[i], capabilityToSet);
    }
}

void ElevateProto(Closure *closure, const uint64_t identity, const bool addOurCapability) {
    const auto capabilitiesToApply = identityToCapabilities(identity, addOurCapability);
    auto capabilitiesInMemory = new uint64_t(capabilitiesToApply);
    ElevateProtoInternal(closure->l.p, capabilitiesInMemory);
}
