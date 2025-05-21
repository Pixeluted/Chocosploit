//
// Created by Pixeluted on 16/02/2025.
//
#pragma once

#include <stdexcept>
#include <variant>
#include <vector>

#include "lualib.h"

// Registers all functions returned from GetFunctions into the global environment of executor
struct RegisterIntoGlobals {
};
// Registers all functions returned from GetFunctions into a new table called how specified in this struct
struct RegisterIntoTable {
    const char *tableName;
};

using RegistrationType = std::variant<RegisterIntoGlobals, RegisterIntoTable>;

class Library {
public:
    virtual ~Library() = default;

    virtual luaL_Reg *GetFunctions() {
        throw std::runtime_error("Not implemented");
    };

    virtual std::vector<RegistrationType> GetRegistrationTypes() {
        throw std::runtime_error("Not implemented");
    };
};

bool IsInstance(lua_State *L, int index);

void CheckInstance(lua_State *L, int index) ;

bool IsClassName(lua_State *L, int index, const char *expectedClassName);

void CheckClassName(lua_State *L, int index, const char *expectedClassName);

void CheckClassNames(lua_State *L, int index, const std::vector<const char *> &expectedClassNames);

template <typename... Args>
__forceinline void CheckClassNames(lua_State *L, const int index, Args... args) {
    const std::vector<const char*> classNames = {args...};
    CheckClassNames(L, index, classNames);
}

bool IsA(lua_State *L, int index, const char *expectedBaseClass);

void CheckIsA(lua_State *L, int index, const char *expectedBaseClass);

bool IsUData(lua_State *L, int index, const char* expectedMetatable);

void PushEnvironment(lua_State *L);

#define EXECUTOR_NAME "UnnamedExecutor"
#define EXECUTOR_CHUNK_NAME "=UnnamedExecutor"

#define EXECUTOR_USER_AGENT "Seliware/1.0"
#define EXECUTOR_FINGERPRINT_HEADER "Seliware-Fingerprint"

#define INIT_SCRIPT R"(
getgenv().getscripts = newcclosure(function()
    local allInstances = getinstances()
    local foundScripts = {}

    for _, v in next, allInstances do
        if v.ClassName == "LocalScript" or v.ClassName == "ModuleScript" or (v.ClassName == "Script" and v.RunContext == Enum.RunContext.Client) then
            table.insert(foundScripts, v)
        end
    end

    return foundScripts
end, "getscripts")


getgenv().getrunningscripts = newcclosure(function()
    local allRunningScripts = {}
    for _,v in next, getscripts() do
        if (v.ClassName == "LocalScript" or (v.ClassName == "Script" and v.RunContext == Enum.RunContext.Client)) and pcall(getsenv, v) then
            table.insert(allRunningScripts, v)
        end
    end

    return allRunningScripts
end, "getrunningscripts")

getgenv().firesignal = newcclosure(function(con, ...)
    for _, connection in next, getconnections(con) do
        if connection.Function or connection.IsWaitConnection then
            connection:Fire(...)
        end
    end
end, "firesignal")

local clonedUI = Instance.new("ScreenGui", game:GetService("CoreGui"))
clonedUI.Name = "ChocoSploit"
getgenv().gethui = newcclosure(function()
    return clonedUI
end, "gethui")

local cryptTbl = crypt
setreadonly(cryptTbl, false)
getgenv().base64decode = cryptTbl.base64_decode
cryptTbl.base64decode = cryptTbl.base64_decode
getgenv().base64encode = cryptTbl.base64_encode
cryptTbl.base64encode = cryptTbl.base64_encode
setreadonly(cryptTbl, true)

local websocketTbl = WebSocket
setreadonly(websocketTbl, false)
websocketTbl.new = websocketTbl.connect
setreadonly(websocketTbl, true)

local drawingTbl = Drawing
setreadonly(drawingTbl, false)
drawingTbl.new = new_drawing
drawingTbl.Fonts = {
    ["UI"] = 0,
    ["System"] = 1,
    ["Plex"] = 2,
    ["Monospace"] = 3
}
setreadonly(drawingTbl.Fonts, true)
setreadonly(drawingTbl, true)

getgenv().setthreadidentity = setidentity
getgenv().set_thread_identity = setidentity
getgenv().getthreadidentity = getidentity
getgenv().get_thread_identity = getidentity
getgenv().checkclosure = isexecutorclosure
getgenv().setContext = setthreadidentity
getgenv().getContext = getthreadidentity
getgenv().getcallbackmember = getcallbackvalue
getgenv().syn = {} -- Compability for scripts
getgenv().info = info_pr

getgenv().isnetworkowner = newcclosure(function(part)
    if typeof(part) ~= "Instance" or not part:IsA("BasePart") then error("Expected BasePart") end
    return gethiddenproperty(part, "NetworkOwnerV3") >= 4
end, "isnetworkowner")

getgenv().GetObjects = newcclosure(function(assetId)
    local oldIdentity = getthreadidentity()
    setthreadidentity(8)
    local objects = { game:GetService("InsertService"):LoadLocalAsset(assetId) }
    setthreadidentity(oldIdentity)
    return objects
end, "GetObjects")
)"