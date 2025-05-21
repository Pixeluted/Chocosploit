//
// Created by Pixeluted on 16/02/2025.
//
#include <array>
#include <memory>

#include "../Execution/InternalTaskScheduler.hpp"
#include "../Execution/LuauSecurity.hpp"
#include "EnvironmentManager.hpp"

#include "ExploitObject.hpp"
#include "../Logger.hpp"
#include "../Managers/Render/RenderManager.hpp"
#include "Libraries/Cache.hpp"
#include "Libraries/Closures.hpp"
#include "Libraries/Crypt.hpp"
#include "Libraries/Debug.hpp"
#include "Libraries/Drawing.hpp"
#include "Libraries/Filesystem.hpp"
#include "Libraries/Globals.hpp"
#include "Libraries/Instances.hpp"
#include "Libraries/Metatable.hpp"
#include "Libraries/Misc.hpp"
#include "Libraries/Script.hpp"
#include "Libraries/Websockets.hpp"

static std::array<std::unique_ptr<Library>, 12> allLibs = {
    std::make_unique<Cache>(),
    std::make_unique<Closures>(),
    std::make_unique<Crypt>(),
    std::make_unique<Debug>(),
    std::make_unique<Drawing>(),
    std::make_unique<Filesystem>(),
    std::make_unique<Globals>(),
    std::make_unique<Instances>(),
    std::make_unique<Metatable>(),
    std::make_unique<Misc>(),
    std::make_unique<Script>(),
    std::make_unique<Websockets>()
};

static lua_CFunction __namecall_original;
static lua_CFunction __index_original;
static uint8_t instanceUserdataTag = 0;

int getRenv_Index(lua_State *L) {
    const std::string keyName = luaL_checkstring(L, 2);
    const auto internalTaskScheduler = ApplicationContext::GetService<InternalTaskScheduler>();

    if (keyName == "shared") {
        lua_getref(L, internalTaskScheduler->currentState->sharedRef);
        return 1;
    }

    if (keyName == "_G") {
        lua_getref(L, internalTaskScheduler->currentState->GRef);
        return 1;
    }

    const auto robloxGlobalState = internalTaskScheduler->luaStates->robloxLuaState;
    lua_pushvalue(robloxGlobalState, LUA_GLOBALSINDEX);
    lua_xmove(robloxGlobalState, L, 1);
    lua_getfield(L, -1, keyName.c_str());
    return 1;
}

int getrenv_Newindex(lua_State *L) {
    const auto keyName = luaL_checkstring(L, 2);
    luaL_checkany(L, 3);

    const auto internalTaskScheduler = ApplicationContext::GetService<InternalTaskScheduler>();
    const auto robloxGlobalState = internalTaskScheduler->luaStates->robloxLuaState;
    lua_pushvalue(robloxGlobalState, LUA_GLOBALSINDEX);
    lua_xmove(robloxGlobalState, L, 1);
    lua_pushvalue(L, 3);
    lua_setfield(L, -2, keyName);
    return 0;
}

int indexHook(lua_State *L) {
    if (!IsOurThread(L))
        return __index_original(L);

    if (IsInstance(L, 1) && lua_isstring(L, 2)) {
        const auto indexName = lua_tostring(L, 2);
        lua_pushcfunction(L, __index_original, "index");
        lua_pushvalue(L, 1);
        lua_pushstring(L, "ClassName");
        lua_call(L, 2, 1);
        const auto instanceClassName = lua_tostring(L, -1);
        lua_pop(L, 1);

        if (strcmp(instanceClassName, "DataModel") == 0 &&
            (strcmp(indexName, "HttpGet") == 0 || strcmp(indexName, "HttpGetAsync") == 0)) {
            lua_getglobal(L, "httpget");
            return 1;
        }

        if (strcmp(instanceClassName, "DataModel") == 0 && strcmp(indexName, "GetObjects") == 0) {
            lua_getglobal(L, "GetObjects");
            return 1;
        }
    }

    return __index_original(L);
}

int namecallHook(lua_State *L) {
    if (!IsOurThread(L))
        return __namecall_original(L);

    auto namecallName = lua_namecallatom(L, nullptr);
    if (IsInstance(L, 1)) {
        lua_pushcfunction(L, __index_original, "index");
        lua_pushvalue(L, 1);
        lua_pushstring(L, "ClassName");
        lua_call(L, 2, 1);
        const auto instanceClassName = lua_tostring(L, -1);
        lua_pop(L, 1);

        if (strcmp(instanceClassName, "DataModel") == 0 &&
            (strcmp(namecallName, "HttpGet") == 0 || strcmp(namecallName, "HttpGetAsync") == 0)) {
            lua_getglobal(L, "httpget");
            lua_pushvalue(L, 2);
            const auto err = lua_pcall(L, 1, 1, 0);
            if (lua_type(L, -1) == LUA_TSTRING && strcmp(
                    lua_tostring(L, -1), "attempt to yield across metamethod/C-call boundary") == 0) {
                return lua_yield(L, 1);
            }

            if (err == LUA_ERRRUN || err == LUA_ERRMEM || err == LUA_ERRERR) {
                lua_error(L);
            }

            return 1;
        }

        if (strcmp(instanceClassName, "DataModel") == 0 && strcmp(namecallName, "GetObjects") == 0) {
            lua_getglobal(L, "GetObjects");
            lua_pushvalue(L, 2);
            const auto err = lua_pcall(L, 1, 1, 0);
            if (lua_type(L, -1) == LUA_TSTRING && strcmp(
                    lua_tostring(L, -1), "attempt to yield across metamethod/C-call boundary") == 0) {
                return lua_yield(L, 1);
            }

            if (err == LUA_ERRRUN || err == LUA_ERRMEM || err == LUA_ERRERR) {
                lua_error(L);
            }

            return 1;
        }
    }

    return __namecall_original(L);
}

void PushEnvironment(lua_State *L) {
    for (const auto &library: allLibs) {
        const auto registrationTypes = library->GetRegistrationTypes();
        if (registrationTypes.empty())
            continue;
        const auto functionsToRegister = library->GetFunctions();

        for (const auto regType: registrationTypes) {
            if (std::holds_alternative<RegisterIntoGlobals>(regType)) {
                lua_pushvalue(L, LUA_GLOBALSINDEX);
                luaL_register(L, nullptr, functionsToRegister);
                lua_pop(L, 1);
            } else if (std::holds_alternative<RegisterIntoTable>(regType)) {
                const auto [tableName] = std::get<RegisterIntoTable>(regType);
                lua_getglobal(L, tableName);
                const auto definedAlready = !lua_isnil(L, -1);
                lua_pop(L, 1);

                if (definedAlready) {
                    lua_getglobal(L, tableName);

                    lua_newtable(L);
                    lua_pushnil(L);
                    while (lua_next(L, -3) != 0) {
                        lua_pushvalue(L, -2);
                        lua_insert(L, -2);
                        lua_settable(L, -4);
                    }

                    luaL_register(L, nullptr, functionsToRegister);
                    lua_setreadonly(L, -1, true);
                    lua_setglobal(L, tableName);
                } else {
                    lua_newtable(L);
                    luaL_register(L, nullptr, functionsToRegister);
                    lua_setreadonly(L, -1, true);
                    lua_setglobal(L, tableName);
                }
            }
        }
    }

    lua_getglobal(L, "game");
    instanceUserdataTag = lua_userdatatag(L, -1);
    lua_pop(L, 1);

    lua_getglobal(L, "game");
    lua_getmetatable(L, -1);
    lua_rawgetfield(L, -1, "__index");
    auto __index = (Closure *) (lua_topointer(L, -1));
    __index_original = __index->c.f;
    __index->c.f = indexHook;

    lua_pop(L, 3);

    lua_getglobal(L, "game");
    lua_getmetatable(L, -1);
    lua_rawgetfield(L, -1, "__namecall");

    auto __namecall = (Closure *) (lua_topointer(L, -1));
    __namecall_original = __namecall->c.f;
    __namecall->c.f = namecallHook;

    lua_pop(L, 3);

    lua_newtable(L);
    lua_newtable(L);

    lua_pushcclosure(L, getRenv_Index, "Renv_Index", 0);
    lua_setfield(L, -2, "__index");

    lua_pushcclosure(L, getrenv_Newindex, "Renv_NewIndex", 0);
    lua_setfield(L, -2, "__newindex");

    lua_setmetatable(L, -2);
    const auto internalTaskScheduler = ApplicationContext::GetService<InternalTaskScheduler>();
    internalTaskScheduler->currentState->customRenvRef = lua_ref(L, -1);
    lua_pop(L, 1); // Pop the custom renv

    InitializeExploitObjects(L);
    internalTaskScheduler->ScheduleExecution(INIT_SCRIPT);
}

bool IsInstance(lua_State *L, const int index) {
    return lua_userdatatag(L, index) == instanceUserdataTag;
}

void CheckInstance(lua_State *L, const int index) {
    if (!IsInstance(L, index))
        luaL_argerrorL(L, index, std::format("Expected Instance, got {}", luaL_typename(L, index)).c_str());
}

bool IsClassName(lua_State *L, const int index, const char *expectedClassName) {
    if (!IsInstance(L, index))
        return false;

    lua_pushcfunction(L, __index_original, "index");
    lua_pushvalue(L, 1);
    lua_pushstring(L, "ClassName");
    lua_call(L, 2, 1);
    const auto className = lua_tostring(L, -1);
    lua_pop(L, 1);

    return strcmp(className, expectedClassName) == 0;
}

void CheckClassName(lua_State *L, const int index, const char *expectedClassName) {
    if (!IsClassName(L, index, expectedClassName))
        luaL_argerrorL(
            L, index, std::format("Expected {}, got {}", expectedClassName, luaL_typename(L, index)).c_str());
}

void CheckClassNames(lua_State *L, const int index, const std::vector<const char *> &expectedClassNames) {
    for (const auto &className: expectedClassNames) {
        if (IsClassName(L, index, className))
            return;
    }

    std::string expectedMsg;
    for (size_t i = 0; i < expectedClassNames.size(); ++i) {
        expectedMsg += expectedClassNames[i];
        if (i < expectedClassNames.size() - 2)
            expectedMsg += ", ";
        else if (i == expectedClassNames.size() - 2)
            expectedMsg += " or ";
    }

    luaL_argerror(L, index, std::format("Expected {}, got {}", expectedMsg, luaL_typename(L, index)).c_str());
}

bool IsA(lua_State *L, const int index, const char *expectedBaseClass) {
    if (!IsInstance(L, index))
        return false;

    lua_getfield(L, index, "IsA");
    lua_pushvalue(L, index);
    lua_pushstring(L, expectedBaseClass);
    lua_call(L, 2, 1);
    const auto results = lua_toboolean(L, -1);
    lua_pop(L, 1);

    return results;
}

void CheckIsA(lua_State *L, const int index, const char *expectedBaseClass) {
    if (!IsA(L, index, expectedBaseClass))
        luaL_argerrorL(
            L, index, std::format("Expected {}, got {}", expectedBaseClass, luaL_typename(L, index)).c_str());
}

// Thanks LuaU for this nice function
bool IsUData(lua_State *L, const int index, const char *expectedMetatable) {
    const void *p = lua_touserdata(L, index);
    if (p != nullptr) {
        if (lua_getmetatable(L, index)) {
            lua_getfield(L, LUA_REGISTRYINDEX, expectedMetatable);
            if (lua_rawequal(L, -1, -2)) {
                lua_pop(L, 2);
                return true;
            } else {
                lua_pop(L, 2);
                return false;
            }
        }
    }
    return false;
}
