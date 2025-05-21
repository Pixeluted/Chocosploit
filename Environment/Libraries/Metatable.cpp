//
// Created by Pixeluted on 17/02/2025.
//

#include "Metatable.hpp"
#include "lstring.h"

int getrawmetatable(lua_State* L)
{
    luaL_checkany(L, 1);

    if (!lua_getmetatable(L, 1))
        lua_pushnil(L);

    return 1;
}

int getnamecallmethod(lua_State* L)
{
    const auto namecall = lua_namecallatom(L, nullptr);

    if (namecall == nullptr)
        lua_pushnil(L);
    else
        lua_pushstring(L, namecall);

    return 1;
}

int setnamecallmethod(lua_State* L)
{
    luaL_checkstring(L, 1);

    if (L->namecall != nullptr)
        L->namecall = luaS_new(L, lua_tostring(L, 1));

    return 0;
}

int setrawmetatable(lua_State* L)
{
    luaL_argexpected(L, lua_istable(L, 1) || lua_isuserdata(L, 1), 1, "table or userdata");
    luaL_argexpected(L, lua_istable(L, 2) || lua_isnil(L, 2), 2, "table or nil");
    luaL_trimstack(L, 2);

    lua_setmetatable(L, 1);
    return 0;
}

int setreadonly(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    luaL_trimstack(L, 2);

    const auto isReadonly = lua_toboolean(L, 2);
    lua_setreadonly(L, 1, isReadonly);

    return 0;
}

int isreadonly(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushboolean(L, lua_getreadonly(L, 1));
    return 1;
}


std::vector<RegistrationType> Metatable::GetRegistrationTypes() {
    return {
        RegisterIntoGlobals{}
    };
}

luaL_Reg *Metatable::GetFunctions() {
     const auto Metatable_functions = new luaL_Reg[] {
         {"getrawmetatable", getrawmetatable},
         {"getnamecallmethod", getnamecallmethod},
         {"setnamecallmethod", setnamecallmethod},
         {"setrawmetatable", setrawmetatable},
         {"setreadonly", setreadonly},
         {"isreadonly", isreadonly},
         {nullptr, nullptr}
     };

     return Metatable_functions;
}
