//
// Created by Pixeluted on 17/02/2025.
//

#include "Misc.hpp"

#include <format>
#include <hex.h>
#include <sha.h>
#include <string>

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "../../Logger.hpp"
#include "../../OffsetsAndFunctions.hpp"
#include "../../Execution/InternalTaskScheduler.hpp"
#include "../../Execution/LuauSecurity.hpp"
#include "../../Managers/Render/RenderManager.hpp"

int setfflag(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checkany(L, 2);

    std::string fflagName = lua_tostring(L, 1);
    std::string newValue;
    if (lua_isboolean(L, 2)) {
        newValue = lua_toboolean(L, 2) ? "True" : "False";
    } else if (lua_isnumber(L, 2)) {
        newValue = std::to_string(lua_tointeger(L, 2));
    } else if (lua_isstring(L, 2)) {
        newValue = lua_tostring(L, 2);
    } else {
        luaL_argerrorL(L, 2, "Unsupported value type!");
    }

    const auto fflagDataBank = *reinterpret_cast<uintptr_t *>(FFlogDataBank);
    const auto wasSuccess = SetFFlag(fflagDataBank, &fflagName, &newValue, 0x7F, 0, 0);

    lua_pushboolean(L, wasSuccess);
    return 1;
}

int getfflag(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);

    std::string fflagName = lua_tostring(L, 1);
    std::string receivedValue;

    const auto wasSuccess = GetFFlag(*reinterpret_cast<uintptr_t *>(FFlogDataBank), &fflagName, &receivedValue, false);
    if (!wasSuccess)
        luaL_argerrorL(L, 1, "Unknown FFlag");

    lua_pushstring(L, receivedValue.c_str());
    return 1;
}

int info_pr(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);

    Printf(ChocoSploit::FunctionDefinitions::Info, lua_tostring(L, 1));
    return 0;
}

int setidentity(lua_State *L) {
    luaL_checknumber(L, 1);
    const auto newIdentity = lua_tointeger(L, 1);
    //if (newIdentity < 0 || newIdentity > 8)
    //    luaL_argerrorL(L, 1, "Identity must be between 0 and 8");

    ElevateThread(L, newIdentity, true, true);

    return 0;
}

int getidentity(lua_State *L) {
    const auto threadUserdata = static_cast<LuauUserdata *>(L->userdata);
    lua_pushnumber(L, threadUserdata->Identity);
    return 1;
}

int setclipboard(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    size_t stringLength;
    const auto stringToCopy = lua_tolstring(L, 1, &stringLength);
    stringLength += 1;

    const auto globalMemory = GlobalAlloc(GMEM_FIXED, stringLength);
    memcpy(globalMemory, stringToCopy, stringLength);

    if (globalMemory == nullptr)
        luaG_runerrorL(L, "Unknown error!");

    if (!OpenClipboard(nullptr))
        luaG_runerrorL(L, "Unknown error!");

    if (!EmptyClipboard())
        luaG_runerrorL(L, "Unknown error!");

    if (!SetClipboardData(CF_TEXT, globalMemory))
        luaG_runerrorL(L, "Unknown error!");

    if (!CloseClipboard())
        luaG_runerrorL(L, "Unknown error!");

    return 0;
}

int copyaddress(lua_State *L) {
    if (!IsInstance(L, 1) && !IsUData(L, 1, "RBXScriptSignal") && !IsUData(L, 1, "RBXScriptConnection") && !lua_isthread(L, 1))
        luaL_argerrorL(L, 1, "Expected Instance, RBXScriptSignal, thread or RBXScriptConnection");

    void* finalAddress;
    if (lua_isthread(L, 1)) {
        finalAddress = lua_tothread(L, 1);
    } else {
        finalAddress = lua_torawuserdata(L, 1);
    }

    const auto stringToCopy = std::format("0x{:x}", reinterpret_cast<uintptr_t>(finalAddress));

    lua_propergetglobal(L, "setclipboard");
    lua_pushstring(L, stringToCopy.c_str());
    lua_call(L, 1, 0);

    return 0;
}

int isrbxactive(lua_State *L) {
    lua_pushboolean(L, GetForegroundWindow() == FindWindowA(nullptr, "Roblox"));
    return 1;
}

int identifyexecutor(lua_State *L) {
    lua_pushstring(L, EXECUTOR_NAME);
    lua_pushstring(L, "V1");
    return 2;
}

int gethwid(lua_State *L) {
    const auto output = GetHWID();

    lua_pushlstring(L, output.c_str(), output.length());
    return 1;
}

int test(lua_State *L) {
    luaG_runerrorL(L, "You're a jew.");
}

int gettenv(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTHREAD);

    const auto theThread = thvalue(luaA_toobject(L, 1));
    lua_pushvalue(theThread, LUA_GLOBALSINDEX);
    lua_xmove(theThread, L, 1);
    return 1;
}

int write_to_internal_log(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const auto logType = static_cast<LogType>(luaL_optinteger(L, 2, 0));
    if (logType < 0 || logType > 2)
        luaL_argerrorL(L, 2, "Good try nigger");

    ApplicationContext::GetService<Logger>()->WriteToLog(logType, lua_tostring(L, 1));
    return 0;
}

std::vector<RegistrationType> Misc::GetRegistrationTypes() {
    return {
        RegisterIntoGlobals{}
    };
}

luaL_Reg *Misc::GetFunctions() {
     const auto Misc_functions = new luaL_Reg[] {
         {"setfflag", setfflag},
         {"getfflag", getfflag},
         {"info_pr", info_pr},
         {"setidentity", setidentity},
         {"getidentity", getidentity},
         {"setclipboard", setclipboard},
         {"copyaddress", copyaddress},
         {"isrbxactive", isrbxactive},
         {"identifyexecutor", identifyexecutor},
         {"gethwid", gethwid},
         {"test", test},
         {"gettenv", gettenv},
         {"write_to_internal_log", write_to_internal_log},
         {nullptr, nullptr}
     };

     return Misc_functions;
}
