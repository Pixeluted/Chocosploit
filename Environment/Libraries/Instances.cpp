//
// Created by Pixeluted on 16/02/2025.
//

#include "ldebug.h"
#include "Instances.hpp"

#include <format>

#include "../../Logger.hpp"
#include "../Roblox/Instance.hpp"
#include "../../Execution/InternalTaskScheduler.hpp"
#include "../ExploitObjects/Connection.hpp"

int getinstances(lua_State *L) {
    luaL_trimstack(L, 0);
    int currentIndex = 1;

    lua_newtable(L);
    lua_pushlightuserdata(L, PushInstance);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushnil(L);

    while (lua_next(L, -2) != 0) {
        if (lua_type(L, -1) == LUA_TUSERDATA) {
            auto rawInstance = *static_cast<void **>(lua_touserdata(L, -1));

            PushInstance(L, &rawInstance);
            lua_rawseti(L, 1, currentIndex++);
        }

        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    return 1;
}

int getnilinstances(lua_State *L) {
    int currentIndex = 1;
    lua_pushcfunction(L, getinstances, "getnilinstances");
    lua_call(L, 0, 1);

    lua_newtable(L);
    lua_pushnil(L);
    while (lua_next(L, -3) != 0) {
        lua_getfield(L, -1, "Parent");
        if (lua_isnil(L, -1)) {
            auto rawInstance = *static_cast<void **>(lua_touserdata(L, -2));

            PushInstance(L, &rawInstance);
            lua_rawseti(L, -5, currentIndex++);
        }

        lua_pop(L, 2);
    }

    lua_remove(L, -2);
    return 1;
}

int getloadedmodules(lua_State *L) {
    int currentIndex = 1;
    lua_pushcfunction(L, getinstances, "getloadedmodules");
    lua_call(L, 0, 1);

    lua_newtable(L);
    lua_pushnil(L);
    while (lua_next(L, -3) != 0) {
        lua_getfield(L, -1, "ClassName");
        if (strcmp(lua_tostring(L, -1), "ModuleScript") == 0) {
            auto rawInstance = *static_cast<void **>(lua_touserdata(L, -2));
            const auto vmStateMap = reinterpret_cast<uintptr_t *>(
                reinterpret_cast<uintptr_t>(rawInstance) + ChocoSploit::StructOffsets::ModuleScript::VmStateMap);
            if (vmStateMap == nullptr) {
                lua_pop(L, 2);
                continue;
            }

            auto mainThread = L->global->mainthread;
            uintptr_t Z;
            const auto receivedData = GetModuleVmState(vmStateMap, &Z, &mainThread);
            const auto isLoaded = *reinterpret_cast<int32_t *>(
                *reinterpret_cast<uintptr_t *>(receivedData) +
                ChocoSploit::StructOffsets::ModuleScript::VmState::LoadedStatus);

            if (isLoaded != 3) {
                lua_pop(L, 2);
                continue;
            }

            PushInstance(L, &rawInstance);
            lua_rawseti(L, -5, currentIndex++);
        }

        lua_pop(L, 2);
    }

    lua_remove(L, -2);
    return 1;
}

int firetouchinterest(lua_State *L) {
    CheckIsA(L, 1, "BasePart");
    CheckIsA(L, 2, "BasePart");

    bool finalToggle;
    if (lua_isboolean(L, 3)) {
        finalToggle = lua_toboolean(L, 3);
    } else if (lua_isnumber(L, 3)) {
        const auto numberProvided = lua_tointeger(L, 3);
        if (numberProvided != 1 && numberProvided != 0)
            luaL_argerrorL(L, 3, "Expected either 1 or 0");
        finalToggle = numberProvided == 1;
    } else {
        luaL_argerror(L, 3, "Expected boolean or number (1 for true, 0 for false)");
    }

    const auto part1 = *static_cast<uintptr_t *>(lua_touserdata(L, 1));
    const auto part2 = *static_cast<uintptr_t *>(lua_touserdata(L, 2));
    const auto primitive1 = *reinterpret_cast<uintptr_t *>(part1 + ChocoSploit::StructOffsets::BasePart::Primitive);
    const auto primitive2 = *reinterpret_cast<uintptr_t *>(part2 + ChocoSploit::StructOffsets::BasePart::Primitive);
    if (!primitive1)
        luaG_runerrorL(L, "Jarvis unlink the module from PEB");
    if (!primitive2)
        luaG_runerrorL(L, "Jarvis unlink the module from PEB");
    const auto overlap = *reinterpret_cast<uintptr_t *>(
        primitive2 + ChocoSploit::StructOffsets::BasePart::PrimitiveStructure::Overlap);

    ApplicationContext::GetService<InternalTaskScheduler>()->ScheduleYield(
        L, [overlap, primitive1, primitive2, finalToggle](const std::shared_ptr<YieldingRequest> &request) {
            MakePartsTouch(overlap, primitive1, primitive2, finalToggle, true);

            request->completionCallback = [](lua_State *L) -> YieldResult {
                return YieldResult{true, 0};
            };
            request->isReady = true;
        });

    return lua_yield(L, 0);
}

int fireproximityprompt(lua_State *L) {
    CheckClassName(L, 1, "ProximityPrompt");

    const auto rawInstance = lua_torawuserdata(L, 1);
    FireProximityPrompt(reinterpret_cast<uintptr_t>(rawInstance));
    return 0;
}

int fireclickdetector(lua_State *L) {
    CheckClassName(L, 1, "ClickDetector");

    const auto rawInstance = lua_torawuserdata(L, 1);
    const auto distance = luaL_optnumber(L, 2, 0);
    const auto type = luaL_optstring(L, 3, "MouseClick");
    if (strcmp(type, "MouseClick") != 0 && strcmp(type, "RightMouseClick") != 0 && strcmp(type, "MouseHoverEnter") != 0
        && strcmp(type, "MouseHoverLeave") != 0)
        luaL_argerrorL(
            L, 3, "Invalid Event! Valid events are: MouseClick, RightMouseClick, MouseHoverEnter, MouseHoverLeave");

    lua_getglobal(L, "game");
    lua_getfield(L, -1, "GetService");
    lua_pushvalue(L, -2);
    lua_pushstring(L, "Players");
    lua_call(L, 2, 1);
    lua_remove(L, -2);
    lua_getfield(L, -1, "LocalPlayer");
    const auto rawLocalPlayer = lua_torawuserdata(L, -1);
    lua_pop(L, 1);

    if (strcmp(type, "MouseClick") == 0) {
        FireClickDetector_MouseClick(reinterpret_cast<uintptr_t>(rawInstance), distance,
                                     reinterpret_cast<uintptr_t>(rawLocalPlayer));
    } else if (strcmp(type, "MouseHoverEnter") == 0) {
        FireClickDetector_HoverEnter(reinterpret_cast<uintptr_t>(rawInstance),
                                     reinterpret_cast<uintptr_t>(rawLocalPlayer));
    } else if (strcmp(type, "MouseHoverLeave") == 0) {
        FireClickDetector_HoverLeave(reinterpret_cast<uintptr_t>(rawInstance),
                                     reinterpret_cast<uintptr_t>(rawLocalPlayer));
    } else if (strcmp(type, "RightMouseClick") == 0) {
        FireClickDetector_RightMouseClick(reinterpret_cast<uintptr_t>(rawInstance), distance,
                                          reinterpret_cast<uintptr_t>(rawLocalPlayer));
    } else {
        luaG_runerrorL(L, "nigga...");
    }

    return 0;
}

int getcallbackvalue(lua_State *L) {
    CheckInstance(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);

    const auto rawInstance = reinterpret_cast<uintptr_t>(lua_torawuserdata(L, 1));
    int Atom;
    lua_tostringatom(L, 2, &Atom);

    auto propertyName = reinterpret_cast<uintptr_t *>(KTable)[Atom];
    if (propertyName == 0 || IsBadReadPtr(reinterpret_cast<void *>(propertyName), 0x10))
        luaL_argerrorL(L, 2, "Invalid property!");

    const auto instanceClassDescriptor = *reinterpret_cast<uintptr_t *>(
        rawInstance + ChocoSploit::StructOffsets::Instance::InstanceClassDescriptor);
    const auto Property = GetPropertyFromName(
        instanceClassDescriptor + ChocoSploit::StructOffsets::Instance::ClassDescriptor::PropertyDescriptor,
        &propertyName);
    if (Property == 0 || IsBadReadPtr(reinterpret_cast<void *>(Property), 0x10))
        luaL_argerrorL(L, 2, "Invalid property!");
    if (const auto PropertyType = *reinterpret_cast<int *>(Property + 0x8); PropertyType != 4)
        luaL_argerrorL(L, 2, "Property is not callback property!");

    const auto callbackStructureStart = rawInstance + *reinterpret_cast<uintptr_t *>(
                                            *reinterpret_cast<uintptr_t *>(Property) + 0x80);
    const auto hasCallback = *reinterpret_cast<uintptr_t *>(callbackStructureStart + 0x38);
    if (hasCallback == 0) {
        lua_pushnil(L);
        return 1;
    }

    const auto callbackStructure = *reinterpret_cast<uintptr_t *>(callbackStructureStart + 0x18);
    if (callbackStructure == 0) {
        lua_pushnil(L);
        return 1;
    }

    const auto ObjectRefs = *reinterpret_cast<uintptr_t *>(callbackStructure + 0x38);
    if (ObjectRefs == 0) {
        lua_pushnil(L);
        return 1;
    }

    const auto ObjectRef = *reinterpret_cast<uintptr_t *>(ObjectRefs + 0x28);
    const auto RefId = *reinterpret_cast<int *>(ObjectRef + 0x14);

    lua_getref(L, RefId);
    return 1;
}

/*
struct ClassDescriptor {
    char pad_0000[8];
    std::string* ClassName;
};

struct Type {
    char pad_0000[8];
    std::string* TypeName;
    char pad_0010[32];
    ChocoSploit::StructOffsets::Instance::ClassDescriptor::TypeStructure::PropertyType TypeId;
};

struct EventArgument {
    std::string *argumentName{};
    Type* valueType{};
    ClassDescriptor* classDescriptor{};
    uintptr_t defaultValue{};
    uintptr_t something{};
};

void replicatesignal(lua_State *L) {
    const auto theSignal = **static_cast<uintptr_t **>(luaL_checkudata(L, 1, "RBXScriptSignal"));

    const auto argsStart = *reinterpret_cast<uintptr_t *>(theSignal + 0x40);
    const auto argsEnd = *reinterpret_cast<uintptr_t *>(theSignal + 0x48);
    if (argsStart == 0 || argsEnd == 0)
        luaL_argerrorL(L, 1, "Nigga there are no args!");

    std::vector<EventArgument *> allArguments{};
    for (uintptr_t current = argsStart; argsEnd >= current;) {
        if (const auto currentValue = *reinterpret_cast<uintptr_t *>(current); currentValue != 0 && !IsBadReadPtr(
                                                                                   reinterpret_cast<void *>(
                                                                                       currentValue), 0x1)) {
            const auto currentArg = reinterpret_cast<EventArgument *>(current);
            allArguments.push_back(currentArg);
            current += 0x68;
        } else {
            current += sizeof(void *);
        }
    }

    lua_createtable(L, allArguments.size(), 0);
    int currentIndex = 0;
    for (const auto eventArg: allArguments) {
        lua_newtable(L);

        lua_pushlstring(L, eventArg->argumentName->c_str(), eventArg->argumentName->length());
        lua_setfield(L, -2, "Name");

        lua_pushlstring(L, eventArg->valueType->TypeName->c_str(), eventArg->valueType->TypeName->length());
        lua_setfield(L, -2, "Type");

        if (eventArg->valueType->TypeId == ChocoSploit::StructOffsets::Instance::ClassDescriptor::TypeStructure::Instance) {
            lua_pushlstring(L, eventArg->classDescriptor->ClassName->c_str(), eventArg->classDescriptor->ClassName->length());
            lua_setfield(L, -2, "ClassName");
        }

        lua_rawseti(L, -2, ++currentIndex);
    }

    return 1;
}
*/

int isscriptable(lua_State *L) {
    CheckInstance(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);

    const auto rawInstance = reinterpret_cast<uintptr_t>(lua_torawuserdata(L, 1));
    const std::string propertyName = lua_tostring(L, 2);
    const auto allProperties = GetInstanceProperties(rawInstance);
    if (!allProperties.contains(propertyName))
        luaG_runerrorL(L, "This property doesn't exist");

    const auto desiredProperty = allProperties.at(propertyName);

    lua_pushboolean(L, IS_SCRIPTABLE_PROPERTY(desiredProperty));
    return 1;
}

int setscriptable(lua_State *L) {
    CheckInstance(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checktype(L, 3, LUA_TBOOLEAN);

    const auto rawInstance = reinterpret_cast<uintptr_t>(lua_torawuserdata(L, 1));
    const std::string propertyName = lua_tostring(L, 2);
    const auto shouldBeScriptable = lua_toboolean(L, 3);
    const auto allProperties = GetInstanceProperties(rawInstance);
    if (!allProperties.contains(propertyName))
        luaG_runerrorL(L, "This property doesn't exist");

    const auto desiredProperty = allProperties.at(propertyName);
    const auto currentCapabilities = GetPropertyCapabilities(desiredProperty);
    const auto isCurrentlyScriptable = IS_SCRIPTABLE_PROPERTY(desiredProperty);

    if (isCurrentlyScriptable && shouldBeScriptable) return 0;
    if (!isCurrentlyScriptable && !shouldBeScriptable) return 0;

    int newCapabilities = currentCapabilities;
    if (isCurrentlyScriptable && !shouldBeScriptable) {
        newCapabilities &= ~(1ull << 5ull);
    } else if (!isCurrentlyScriptable && shouldBeScriptable) {
        newCapabilities |= (1ull << 5ull);
    }

    SetPropertyCapabilities(desiredProperty, newCapabilities);
    return 0;
}

int gethiddenproperty(lua_State *L) {
    CheckInstance(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);

    const auto rawInstance = reinterpret_cast<uintptr_t>(lua_torawuserdata(L, 1));
    const std::string propertyName = lua_tostring(L, 2);
    const auto allProperties = GetInstanceProperties(rawInstance);
    if (!allProperties.contains(propertyName))
        luaG_runerrorL(L, "This property doesn't exist");

    const auto desiredProperty = allProperties.at(propertyName);
    /**
     * First handle special types likes BinaryString and SystemAddress, then use normal __index
     */
    const auto propertyTypeAddress = reinterpret_cast<
        ChocoSploit::StructOffsets::Instance::ClassDescriptor::TypeStructure::PropertyType *>(
        *reinterpret_cast<uintptr_t *>(desiredProperty + ChocoSploit::StructOffsets::Instance::ClassDescriptor::Type)
        + ChocoSploit::StructOffsets::Instance::ClassDescriptor::TypeStructure::typeId);
    const auto propertyType = *propertyTypeAddress;
    if (propertyType == ChocoSploit::StructOffsets::Instance::ClassDescriptor::TypeStructure::BinaryString) {
        // If type is BinaryString then set the type to normal string and it will work fine!
        *propertyTypeAddress = ChocoSploit::StructOffsets::Instance::ClassDescriptor::TypeStructure::String;
    }

    const auto wasScriptable = IS_SCRIPTABLE_PROPERTY(desiredProperty);
    if (propertyType == ChocoSploit::StructOffsets::Instance::ClassDescriptor::TypeStructure::SharedString) {
        const auto getSetDescriptor = *reinterpret_cast<uintptr_t *>(
            desiredProperty + ChocoSploit::StructOffsets::Instance::ClassDescriptor::TypeGetSetDescriptor);
        const auto descriptorVFtable = *reinterpret_cast<uintptr_t *>(getSetDescriptor);

        using getSharedString = uintptr_t(__fastcall*)(uintptr_t getSetDescriptor, void *sharedString,
                                                       uintptr_t theInstance);
        const auto getFunc = *reinterpret_cast<getSharedString *>(
            descriptorVFtable +
            ChocoSploit::StructOffsets::Instance::ClassDescriptor::GetSetDescriptor::getVFtableFunc);

        const auto receivedSharedString = malloc(0x30);
        getFunc(getSetDescriptor, receivedSharedString, rawInstance);
        const auto theRealString = *static_cast<uintptr_t *>(receivedSharedString);
        const auto stringData = *reinterpret_cast<const char **>(theRealString + 0x10);
        const auto stringLength = *reinterpret_cast<int *>(theRealString + 0x20);

        lua_pushlstring(L, stringData, stringLength);
        lua_pushboolean(L, !wasScriptable);
        free(receivedSharedString);
        return 2;
    } else if (propertyType == ChocoSploit::StructOffsets::Instance::ClassDescriptor::TypeStructure::SystemAddress) {
        const auto getSetDescriptor = *reinterpret_cast<uintptr_t *>(
            desiredProperty + ChocoSploit::StructOffsets::Instance::ClassDescriptor::TypeGetSetDescriptor);
        const auto descriptorVFtable = *reinterpret_cast<uintptr_t *>(getSetDescriptor);

        using getSystemAddress = uintptr_t(__fastcall*)(uintptr_t getSetDescriptor, int *systemAddress,
                                                        uintptr_t theInstance);

        auto remotePeerId = 0;
        const auto getFunc = *reinterpret_cast<getSystemAddress *>(
            descriptorVFtable +
            ChocoSploit::StructOffsets::Instance::ClassDescriptor::GetSetDescriptor::getVFtableFunc);
        getFunc(getSetDescriptor, &remotePeerId, rawInstance);

        lua_pushinteger(L, remotePeerId);
        lua_pushboolean(L, !wasScriptable);
        return 2;
    }

    lua_getglobal(L, "setscriptable");
    lua_pushvalue(L, 1);
    lua_pushstring(L, propertyName.c_str());
    lua_pushboolean(L, true);
    lua_call(L, 3, 0);

    lua_pushvalue(L, 1);
    luaL_getmetafield(L, -1, "__index");
    lua_remove(L, -2); // Pop the object we got the mt from
    lua_pushvalue(L, 1);
    lua_pushstring(L, propertyName.c_str());
    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        const auto errorMessage = lua_tostring(L, -1);

        lua_getglobal(L, "setscriptable");
        lua_pushvalue(L, 1);
        lua_pushstring(L, propertyName.c_str());
        lua_pushboolean(L, wasScriptable);
        lua_call(L, 3, 0);

        *propertyTypeAddress = propertyType;

        lua_pushstring(L, errorMessage);
        lua_error(L);
    }

    lua_getglobal(L, "setscriptable");
    lua_pushvalue(L, 1);
    lua_pushstring(L, propertyName.c_str());
    lua_pushboolean(L, wasScriptable);
    lua_call(L, 3, 0);

    *propertyTypeAddress = propertyType;

    lua_pushvalue(L, -1);
    lua_pushboolean(L, !wasScriptable);
    return 2;
}

int sethiddenproperty(lua_State *L) {
    CheckInstance(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checkany(L, 3);

    const auto rawInstance = reinterpret_cast<uintptr_t>(lua_torawuserdata(L, 1));
    const std::string propertyName = lua_tostring(L, 2);
    const auto allProperties = GetInstanceProperties(rawInstance);
    if (!allProperties.contains(propertyName))
        luaG_runerrorL(L, "This property doesn't exist");

    const auto desiredProperty = allProperties.at(propertyName);
    const auto wasScriptable = IS_SCRIPTABLE_PROPERTY(desiredProperty);

    lua_getglobal(L, "setscriptable");
    lua_pushvalue(L, 1);
    lua_pushstring(L, propertyName.c_str());
    lua_pushboolean(L, true);
    lua_call(L, 3, 0);

    lua_pushvalue(L, 1);
    luaL_getmetafield(L, -1, "__newindex");
    lua_remove(L, -2); // Pop the object we got the mt from
    lua_pushvalue(L, 1);
    lua_pushstring(L, propertyName.c_str());
    lua_pushvalue(L, 3);
    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        const auto errorMessage = lua_tostring(L, -1);

        lua_getglobal(L, "setscriptable");
        lua_pushvalue(L, 1);
        lua_pushstring(L, propertyName.c_str());
        lua_pushboolean(L, wasScriptable);
        lua_call(L, 3, 0);

        lua_pushstring(L, errorMessage);
        lua_error(L);
    }

    lua_getglobal(L, "setscriptable");
    lua_pushvalue(L, 1);
    lua_pushstring(L, propertyName.c_str());
    lua_pushboolean(L, wasScriptable);
    lua_call(L, 3, 0);

    lua_pushboolean(L, !wasScriptable);
    return 1;
}

int getconnections(lua_State *L) {
    if (!IsUData(L, 1, "RBXScriptSignal"))
        luaL_argerrorL(L, 1, "Expected RBXScriptSignal");

    lua_getfield(L, 1, "Connect");
    lua_pushvalue(L, 1);
    lua_pushcfunction(L, [](lua_State* L) { return 0; }, "");
    lua_call(L, 2, 1);

    const auto ourConnection = reinterpret_cast<uintptr_t>(lua_torawuserdata(L, -1));
    if (ourConnection == 0) {
        lua_getfield(L, -1, "Disconnect");
        lua_pushvalue(L, -2);
        lua_call(L, 1, 0);

        luaL_argerrorL(L, 1, "Unsupported connection");
    }

    auto nextConnection = *reinterpret_cast<uintptr_t *>(ourConnection + ChocoSploit::StructOffsets::Signal::next);

    lua_getfield(L, -1, "Disconnect");
    lua_pushvalue(L, -2);
    lua_call(L, 1, 0);

    lua_newtable(L);
    int currentIndex = 0;
    while (nextConnection != 0) {
        // Skip invalid connections
        ApplicationContext::GetService<Logger>()->WriteToLog(
            Info, std::format("Check Value: {}", *reinterpret_cast<uint8_t *>(nextConnection)));
        if (*reinterpret_cast<uint8_t *>(nextConnection) != 1) {
            nextConnection = *reinterpret_cast<uintptr_t *>(
                nextConnection + ChocoSploit::StructOffsets::Signal::next);
            continue;
        }

        NewConnection(L, nextConnection);
        lua_rawseti(L, -2, ++currentIndex);
        nextConnection = *reinterpret_cast<uintptr_t *>(nextConnection + ChocoSploit::StructOffsets::Signal::next);
    }

    return 1;
}

std::vector<RegistrationType> Instances::GetRegistrationTypes() {
    return {
        RegisterIntoGlobals{}
    };
}

luaL_Reg *Instances::GetFunctions() {
     const auto Instances_functions = new luaL_Reg[] {
         {"getinstances", getinstances},
         {"getnilinstances", getnilinstances},
         {"getloadedmodules", getloadedmodules},
         {"firetouchinterest", firetouchinterest},
         {"fireproximityprompt", fireproximityprompt},
         {"fireclickdetector", fireclickdetector},
         {"getcallbackvalue", getcallbackvalue},
         {"isscriptable", isscriptable},
         {"setscriptable", setscriptable},
         {"gethiddenproperty", gethiddenproperty},
         {"sethiddenproperty", sethiddenproperty},
         {"getconnections", getconnections},
         {nullptr, nullptr}
     };

     return Instances_functions;
}
