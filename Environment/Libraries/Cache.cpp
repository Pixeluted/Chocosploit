//
// Created by Pixeluted on 17/02/2025.
//

#include "Cache.hpp"

#include "../../OffsetsAndFunctions.hpp"

int cloneref(lua_State *L) {
    CheckInstance(L, 1);

    const auto userdata = lua_touserdata(L, 1);
    const auto rawInstance = lua_torawuserdata(L, 1);

    lua_pushlightuserdata(L, PushInstance);
    lua_rawget(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, rawInstance);
    lua_rawget(L, -2);

    lua_pushlightuserdata(L, rawInstance);
    lua_pushnil(L);
    lua_rawset(L, -4);

    PushInstance(L, (void **) userdata);

    lua_pushlightuserdata(L, rawInstance);
    lua_pushvalue(L, -3);
    lua_rawset(L, -5);

    return 1;
}

int invalidate(lua_State *L) {
    CheckInstance(L, 1);

    const auto rawInstance = lua_torawuserdata(L, 1);

    lua_pushlightuserdata(L, PushInstance);
    lua_gettable(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, rawInstance);
    lua_pushnil(L);
    lua_settable(L, -3);

    return 0;
}

int replace(lua_State* L) {
    CheckInstance(L, 1);
    CheckInstance(L, 2);

    const auto rawInstance = lua_torawuserdata(L, 1);

    lua_pushlightuserdata(L, PushInstance);
    lua_gettable(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, rawInstance);
    lua_pushvalue(L, 2);
    lua_settable(L, -3);

    return 0;
}

int iscached(lua_State* L) {
    CheckInstance(L, 1);

    const auto rawInstance = lua_torawuserdata(L, 1);

    lua_pushlightuserdata(L, PushInstance);
    lua_gettable(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, rawInstance);
    lua_gettable(L, -2);

    lua_pushboolean(L, !lua_isnil(L, -1));
    return 1;
}

int compareinstances(lua_State* L) {
    CheckInstance(L, 1);
    CheckInstance(L, 2);

    lua_pushboolean(L, *static_cast<const std::uintptr_t *>(lua_touserdata(L, 1)) ==
                               *static_cast<const std::uintptr_t *>(lua_touserdata(L, 2)));
    return 1;
}

std::vector<RegistrationType> Cache::GetRegistrationTypes() {
    return {
        RegisterIntoGlobals{},
        RegisterIntoTable{"cache"}
    };
}

luaL_Reg *Cache::GetFunctions() {
     const auto Cache_functions = new luaL_Reg[] {
         {"cloneref", cloneref},
         {"invalidate", invalidate},
         {"replace", replace},
         {"iscached", iscached},
         {"compareinstances", compareinstances},
         {nullptr, nullptr}
     };

     return Cache_functions;
}
