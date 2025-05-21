//
// Created by Pixeluted on 16/02/2025.
//

#include "CClosureManager.hpp"
#include "lgc.h"
#include "lstate.h"
#include "lualib.h"
#include "../Logger.hpp"
#include <format>
#include <ranges>

std::string &strip(std::string &str) {
    str.erase(str.begin(),
              std::ranges::find_if(str, [](unsigned char ch) {
                  return !std::isspace(ch);
              }));

    str.erase(std::ranges::find_if(str | std::views::reverse, [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), str.end());

    return str;
}

std::regex CClosureManager::errorChunkInfoPattern = std::regex(R"(^([^:]+):(\d+):)");

std::string CClosureManager::CleanErrorMessage(const std::string &errorToClean) {
    auto cleanedErrorMessage = std::regex_replace(errorToClean, errorChunkInfoPattern, "");
    const auto strippedErrorMessage = strip(cleanedErrorMessage);
    return strippedErrorMessage;
}

int CClosureManager::NewCClosureProxy(lua_State *L) {
    const auto argsCount = lua_gettop(L);
    const auto newCClosureMap = ApplicationContext::GetService<CClosureManager>()->newCClosureMap;
    const auto callingClosure = clvalue(L->ci->func);
    if (!newCClosureMap.contains(callingClosure))
        return 0;

    const auto logger = ApplicationContext::GetService<Logger>();
    auto originalClosure = newCClosureMap.at(callingClosure);

    auto closure = originalClosure.GetReferencedObject(L);
    if (!closure.has_value()) {
        logger->Log(Info,
                    "NewCClosureProxy -> Something exploded, and your ref is no longer valid. Blame the nigger of senS.");
        luaL_error(L, "call resolution failed");
    }

    lua_pushrawclosure(L, closure.value());
    lua_insert(L, 1);

    L->ci->flags |= LUA_CALLINFO_HANDLE;
    L->baseCcalls++;
    const auto callResults = lua_pcall(L, argsCount, LUA_MULTRET, 0);
    L->baseCcalls--;

    if (callResults == LUA_OK && (L->status == LUA_YIELD || L->status == LUA_BREAK)) {
        return lua_yield(L, 0);
    }

    if (callResults != LUA_OK && L->status == LUA_YIELD && (strcmp(
            luaL_optstring(L, -1, ""),
            "attempt to yield across metamethod/C-call boundary") == 0 || strcmp(
            luaL_optstring(L, -1, ""), "thread is not yieldable") == 0)) {
        return lua_yield(L, 0);
    }

    if (callResults == LUA_ERRRUN) {
        if (!lua_isstring(L, -1)) {
            logger->Log(Warn,
                        "newwcclosure proxy just errored. but for some reason the rofeds didn't give us an error message. Nemi istg...");

            lua_error(L);
        }

        const auto cleanedErrorMessage = CleanErrorMessage(lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_pushstring(L, cleanedErrorMessage.c_str());
        lua_error(L);
    }

    return lua_gettop(L);
}

int CClosureManager::NewCClosureCont(lua_State *L, int status) {
    if (status == LUA_OK)
        return lua_gettop(L);


    const auto cleanedErrorMessage = CleanErrorMessage(lua_tostring(L, -1));
    lua_pop(L, 1);
    lua_pushstring(L, cleanedErrorMessage.c_str());
    lua_error(L);
}


Closure *CClosureManager::WrapClosure(lua_State *L, Closure *closureToWrap, const std::string_view functionName) {
    lua_pushrawclosure(L, closureToWrap);
    lua_ref(L, -1);

    lua_pushcclosurek(L, NewCClosureProxy, functionName.data(), 0, NewCClosureCont);
    const auto wrappedClosure = lua_toclosure(L, -1);
    newCClosureMap[wrappedClosure] = {L, -2};

    lua_pop(L, 2);
    return wrappedClosure;
}
