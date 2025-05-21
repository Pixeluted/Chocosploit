//
// Created by Pixeluted on 11/02/2025.
//
#include "Globals.hpp"

#include <Cpr/include/cpr/timeout.h>

#include "lobject.h"
#include "../../Execution/BytecodeCompiler.hpp"
#include "../../Execution/InternalTaskScheduler.hpp"
#include "../../Execution/LuauSecurity.hpp"
#include <cpr/cpr.h>

#include "lstate.h"
#include "lualib.h"
#include "Misc.hpp"

int loadstring(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const auto sourceCode = lua_tostring(L, 1);
    const auto chunkName = luaL_optstring(L, 2, EXECUTOR_CHUNK_NAME);

    const auto compiledBytecode = CompileBytecode(sourceCode);
    if (luau_load(L, chunkName, compiledBytecode.c_str(), compiledBytecode.length(), 0) != LUA_OK) {
        lua_pushnil(L);
        lua_pushvalue(L, -2);
        return 2;
    }

    const auto loadedClosure = lua_toclosure(L, -1);
    ElevateProto(loadedClosure, 8, true);

    lua_setsafeenv(L, LUA_GLOBALSINDEX, false);
    return 1;
}

int require(lua_State *L) {
    CheckClassName(L, 1, "ModuleScript");

    const auto originalIdentity = static_cast<LuauUserdata *>(L->userdata)->Identity;
    ElevateThread(L, 2, true, true);

    lua_getglobal(L, "getrenv");
    lua_call(L, 0, 1);
    lua_getfield(L, -1, "require");
    lua_remove(L, -2);
    lua_pushvalue(L, 1);

    const auto requireResults = lua_pcall(L, 1, 1, 0);
    ElevateThread(L, originalIdentity, true, true);
    if (requireResults != LUA_OK) {
        lua_error(L);
    }

    return 1;
}

int getgenv(lua_State *L) {
    const auto execLuaState = ApplicationContext::GetService<InternalTaskScheduler>()->luaStates->executorLuaState;
    lua_pushvalue(execLuaState, LUA_GLOBALSINDEX);
    lua_xmove(execLuaState, L, 1);
    return 1;
}

int getreg(lua_State *L) {
    lua_pushvalue(L, LUA_REGISTRYINDEX);
    return 1;
}

int getrenv(lua_State *L) {
    lua_getref(L, ApplicationContext::GetService<InternalTaskScheduler>()->currentState->customRenvRef);
    return 1;
}

int httpget(lua_State *L) {
    std::string finalUrl;
    if (lua_isstring(L, 1)) {
        finalUrl = lua_tostring(L, 1);
    } else if (lua_isstring(L, 2)) {
        finalUrl = lua_tostring(L, 2);
    } else {
        luaL_argerrorL(L, 1, "Expected URL");
    }

    if (!finalUrl.starts_with("http://") && !finalUrl.starts_with("https://")) {
        luaL_argerrorL(L, 1, "Url needs to start with http:// or https://");
    }

    ApplicationContext::GetService<InternalTaskScheduler>()->ScheduleYield(
        L, [finalUrl = std::move(finalUrl)](const std::shared_ptr<YieldingRequest> &request) {
            cpr::Response response = cpr::Get(cpr::Url{
                                                  finalUrl
                                              }, cpr::Header{
                                                  {"User-Agent", "Roblox/WinInet"},
                                                  {EXECUTOR_FINGERPRINT_HEADER, GetHWID()}
                                              });

            request->completionCallback = [response = std::move(response)](lua_State *L) -> YieldResult {
                lua_pushlstring(L, response.text.c_str(), response.text.size());
                return YieldResult{true, 1};
            };
            request->isReady = true;
        });

    return lua_yield(L, 1);
}

const std::array<std::string, 5> validMethods = {"get", "post", "put", "patch", "delete"};

int request(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    auto getRequiredString = [L](const char *field, const bool allowDefault = false, const std::string& defaultValue = "") -> std::string {
        lua_getfield(L, 1, field);
        if (lua_isnil(L, -1) && !allowDefault) {
            luaL_argerrorL(L, 1, std::format("Options must have a {}", field).c_str());
        } else if (lua_isnil(L, -1) && allowDefault) {
            return defaultValue;
        }

        if (!lua_isstring(L, -1))
            luaL_argerrorL(L, 1, std::format("{} must be a string", field).c_str());

        size_t stringLength = 0;
        const auto rawString = lua_tolstring(L, -1, &stringLength);
        lua_pop(L, 1);
        return std::string{rawString, stringLength};
    };

    const auto Url = getRequiredString("Url");

    if (!Url.starts_with("http://") && !Url.starts_with("https://"))
        luaL_argerrorL(L, 1, "Url needs to start with http:// or https://");

    auto Method = getRequiredString("Method", true, "GET");
    std::ranges::transform(Method, Method.begin(), ::tolower);

    if (std::ranges::find(validMethods, Method) == validMethods.end())
        luaL_argerrorL(L, 1, "Method must be one of these 'GET', 'POST', 'PUT', 'PATCH', 'DELETE");

    auto Headers = std::map<std::string, std::string, cpr::CaseInsensitiveCompare>();
    Headers["User-Agent"] = EXECUTOR_USER_AGENT;
    Headers[EXECUTOR_FINGERPRINT_HEADER] = GetHWID();

    lua_getfield(L, 1, "Headers");
    if (!lua_isnil(L, -1)) {
        if (!lua_istable(L, -1))
            luaL_argerrorL(L, 1, "Headers must be a table");

        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (!lua_isstring(L, -2) || !lua_isstring(L, -1))
                luaL_argerrorL(L, 1, "Header key and value must be strings");

            Headers[lua_tostring(L, -2)] = lua_tostring(L, -1);
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
    }

    cpr::Cookies cookies;
    lua_getfield(L, 1, "Cookies");
    if (!lua_isnil(L, -1)) {
        if (!lua_istable(L, -1))
            luaL_argerrorL(L, 1, "Cookies must be a table");

        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (!lua_isstring(L, -2) || !lua_isstring(L, -1))
                luaL_argerrorL(L, 1, "Cookie key and value must be strings");

            cookies.emplace_back(cpr::Cookie{lua_tostring(L, -2), lua_tostring(L, -1)});
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
    }

    std::string Body;
    lua_getfield(L, 1, "Body");
    if (!lua_isnil(L, -1)) {
        if (!lua_isstring(L, -1))
            luaL_argerrorL(L, 1, "Body must be a string");

        size_t bodyLength = 0;
        const auto rawData = lua_tolstring(L, -1, &bodyLength);
        Body = std::string_view{rawData, bodyLength};
        lua_pop(L, 1);
    }

    ApplicationContext::GetService<InternalTaskScheduler>()->ScheduleYield(
        L, [Url, Method, Headers, Body, cookies](const std::shared_ptr<YieldingRequest> &yieldingRequest) {
            cpr::Session requestSession;
            requestSession.SetUrl(Url);
            requestSession.SetHeader(cpr::Header{Headers});
            requestSession.SetCookies(cookies);
            requestSession.SetBody(Body);

            cpr::Response response;
            if (Method == "get") {
                response = requestSession.Get();
            } else if (Method == "post") {
                response = requestSession.Post();
            } else if (Method == "put") {
                response = requestSession.Put();
            } else if (Method == "patch") {
                response = requestSession.Patch();
            } else if (Method == "delete") {
                response = requestSession.Delete();
            }

            yieldingRequest->completionCallback = [response](lua_State *L) -> YieldResult {
                lua_newtable(L);

                lua_pushboolean(L, response.status_code >= 200 && response.status_code <= 299);
                lua_setfield(L, -2, "Success");

                lua_pushinteger(L, response.status_code);
                lua_setfield(L, -2, "StatusCode");

                lua_pushlstring(L, response.text.c_str(), response.text.size());
                lua_setfield(L, -2, "Body");

                lua_pushstring(L, response.status_line.c_str());
                lua_setfield(L, -2, "StatusMessage");

                lua_newtable(L);
                for (const auto &header: response.header) {
                    lua_pushstring(L, header.second.c_str());
                    lua_setfield(L, -2, header.first.c_str());
                }
                lua_setfield(L, -2, "Headers");

                lua_newtable(L);
                for (const auto &cookie: response.cookies) {
                    if (cookie.GetName() == ".ROBLOSECURITY")
                        continue;

                    lua_pushstring(L, cookie.GetValue().c_str());
                    lua_setfield(L, -2, cookie.GetName().c_str());
                }
                lua_setfield(L, -2, "Cookies");

                return YieldResult{true, 1};
            };
            yieldingRequest->isReady = true;
        });

    return lua_yield(L, 1);
}

int queue_on_teleport(lua_State *L) {
    luaL_checkstring(L, 1);
    const auto scriptToQueue = std::string(lua_tostring(L, 1));
    ApplicationContext::GetService<InternalTaskScheduler>()->scheduledItems->scheduledTeleportScripts.
            push_back(scriptToQueue);

    return 0;
}

std::vector<RegistrationType> Globals::GetRegistrationTypes() {
    return {
        RegisterIntoGlobals{}
    };
}

luaL_Reg *Globals::GetFunctions() {
     const auto Globals_functions = new luaL_Reg[] {
         {"loadstring", loadstring},
         {"require", require},
         {"getgenv", getgenv},
         {"getreg", getreg},
         {"getrenv", getrenv},
         {"httpget", httpget},
         {"request", request},
         {"queue_on_teleport", queue_on_teleport},
         {nullptr, nullptr}
     };

     return Globals_functions;
}
