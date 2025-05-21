//
// Created by Pixeluted on 18/02/2025.
//

#include "Debug.hpp"

#include "lapi.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#undef setupvalue

static LuaTable *getcurrenv(lua_State *L) {
    if (L->ci == L->base_ci) // no enclosing function?
        return L->gt; // use global table as environment
    else
        return curr_func(L)->env;
}

static LUAU_NOINLINE TValue *pseudo2addr(lua_State *L, int idx) {
    api_check(L, lua_ispseudo(idx));
    switch (idx) {
        // pseudo-indices
        case LUA_REGISTRYINDEX:
            return registry(L);
        case LUA_ENVIRONINDEX: {
            sethvalue(L, &L->global->pseudotemp, getcurrenv(L));
            return &L->global->pseudotemp;
        }
        case LUA_GLOBALSINDEX: {
            sethvalue(L, &L->global->pseudotemp, L->gt);
            return &L->global->pseudotemp;
        }
        default: {
            Closure *func = curr_func(L);
            idx = LUA_GLOBALSINDEX - idx;
            return (idx <= func->nupvalues) ? &func->c.upvals[idx - 1] : cast_to(TValue*, luaO_nilobject);
        }
    }
}

static LUAU_FORCEINLINE TValue *index2addr(lua_State *L, int idx) {
    if (idx > 0) {
        TValue *o = L->base + (idx - 1);
        api_check(L, idx <= L->ci->top - L->base);
        if (o >= L->top)
            return cast_to(TValue*, luaO_nilobject);
        else
            return o;
    } else if (idx > LUA_REGISTRYINDEX) {
        api_check(L, idx != 0 && -idx <= L->top - L->base);
        return L->top + idx;
    } else {
        return pseudo2addr(L, idx);
    }
}

int getinfo(lua_State *L) {
    luaL_trimstack(L, 1);
    luaL_argexpected(L, lua_isnumber(L, 1) || lua_isfunction(L, 1), 1, "function or level");

    auto infoLevel = 0;
    if (lua_isnumber(L, 1)) {
        infoLevel = lua_tointeger(L, 1);
        luaL_argcheck(L, infoLevel >= 0, 1, "level cannot be negative");
    } else if (lua_isfunction(L, 1)) {
        infoLevel = -lua_gettop(L);
    }

    lua_Debug debugInfo{};
    if (!lua_getinfo(L, infoLevel, "fulasnf", &debugInfo))
        luaL_argerrorL(L, 1, "invalid level");

    lua_newtable(L);

    lua_pushvalue(L, -2);
    lua_setfield(L, -2, "func");

    lua_pushstring(L, debugInfo.source);
    lua_setfield(L, -2, "source");

    lua_pushstring(L, debugInfo.short_src);
    lua_setfield(L, -2, "short_src");

    lua_pushstring(L, debugInfo.what);
    lua_setfield(L, -2, "what");

    lua_pushinteger(L, debugInfo.currentline);
    lua_setfield(L, -2, "currentline");

    lua_pushstring(L, debugInfo.name);
    lua_setfield(L, -2, "name");

    lua_pushinteger(L, debugInfo.nupvals);
    lua_setfield(L, -2, "nups");

    lua_pushinteger(L, debugInfo.nparams);
    lua_setfield(L, -2, "numparams");

    lua_pushinteger(L, debugInfo.isvararg);
    lua_setfield(L, -2, "is_vararg");

    return 1;
}

__forceinline void convert_level_or_function_to_closure(lua_State *L, const char *cFunctionErrorMessage,
                                                        const bool shouldErrorOnCFunction = true) {
    luaL_checkany(L, 1);

    if (lua_isnumber(L, 1)) {
        lua_Debug debugInfo{};
        const auto level = lua_tointeger(L, 1);

        if (level < 0 || level > 255)
            luaL_argerrorL(L, 1, "level out of bounds");

        if (!lua_getinfo(L, level, "f", &debugInfo))
            luaL_argerrorL(L, 1, "invalid level");
    } else if (lua_isfunction(L, 1)) {
        lua_pushvalue(L, 1);
    } else {
        luaL_argerrorL(L, 1, "level or function expected");
    }

    if (!lua_isfunction(L, -1))
        luaG_runerrorL(L, "There isn't function on stack");

    if (shouldErrorOnCFunction && lua_iscfunction(L, -1))
        luaL_argerrorL(L, 1, cFunctionErrorMessage);
}

int getconstants(lua_State *L) {
    luaL_trimstack(L, 1);
    convert_level_or_function_to_closure(L, "Cannot get constants from C closure");

    // Do not touch this unless if you want to die
    const auto closure = lua_toclosure(L, -1);
    lua_createtable(L, closure->l.p->sizek, 0);

    for (int i = 0; i < closure->l.p->sizek; i++) {
        auto &&constant = &closure->l.p->k[i];

        if (constant->tt != LUA_TFUNCTION && constant->tt != LUA_TTABLE) {
            setobj(L, L->top, constant);
            incr_top(L);
        } else {
            lua_pushnil(L);
        }

        lua_rawseti(L, -2, (i + 1));
    }

    return 1;
}

int getconstant(lua_State *L) {
    luaL_trimstack(L, 2);
    luaL_checktype(L, 2, LUA_TNUMBER);
    convert_level_or_function_to_closure(L, "Cannot get constants from C closure");

    const auto constantIndex = lua_tointeger(L, 2);
    const auto closure = lua_toclosure(L, -1);

    luaL_argcheck(L, constantIndex > 0, 2, "index cannot be negative");
    luaL_argcheck(L, constantIndex <= closure->l.p->sizek, 2, "index out of range");

    const auto &constant = closure->l.p->k[constantIndex - 1];
    setobj(L, L->top, &constant);
    incr_top(L);

    return 1;
}

int setconstant(lua_State *L) {
    luaL_trimstack(L, 3);
    luaL_checktype(L, 2, LUA_TNUMBER);
    luaL_argexpected(L, lua_isnumber(L, 3) || lua_isboolean(L, 3) || lua_isstring(L, 3), 3,
                     "number or boolean or string");
    convert_level_or_function_to_closure(L, "Cannot set constants on a C closure");

    const auto constantIndex = lua_tointeger(L, 2);
    const auto closure = lua_toclosure(L, -1);

    luaL_argcheck(L, constantIndex > 0, 2, "index cannot be negative");
    luaL_argcheck(L, constantIndex <= closure->l.p->sizek, 3, "index out of range");

    setobj(L, &closure->l.p->k[constantIndex - 1], index2addr(L, 3))

    return 0;
}

int getupvalues(lua_State *L) {
    luaL_trimstack(L, 1);
    convert_level_or_function_to_closure(L, "Cannot get upvalues on C Closures", true);

    const auto closure = lua_toclosure(L, -1);
    lua_newtable(L);

    lua_pushrawclosure(L, closure);
    for (int i = 0; i < closure->nupvalues;) {
        lua_getupvalue(L, -1, ++i);
        lua_rawseti(L, -3, i);
    }

    lua_pop(L, 1);
    return 1;
}

int getupvalue(lua_State *L) {
    luaL_trimstack(L, 2);
    luaL_checktype(L, 2, LUA_TNUMBER);
    convert_level_or_function_to_closure(L, "Cannot get upvalue on C Closures", true);

    const auto upvalueIndex = lua_tointeger(L, 2);
    const auto closure = lua_toclosure(L, -1);

    luaL_argcheck(L, upvalueIndex > 0, 2, "index cannot be negative");
    luaL_argcheck(L, upvalueIndex <= closure->nupvalues, 2, "index out of range");

    lua_pushrawclosure(L, closure);
    lua_getupvalue(L, -1, upvalueIndex);

    lua_remove(L, -2);
    return 1;
}

int setupvalue(lua_State *L) {
    luaL_trimstack(L, 3);
    luaL_checktype(L, 2, LUA_TNUMBER);
    luaL_checkany(L, 3);
    convert_level_or_function_to_closure(L, "Cannot set upvalue on C Closure", true);

    const auto closure = lua_toclosure(L, -1);
    const auto upvalueIndex = lua_tointeger(L, 2);
    const auto objToSet = index2addr(L, 3);

    luaL_argcheck(L, upvalueIndex > 0, 2, "index cannot be negative");
    luaL_argcheck(L, upvalueIndex <= closure->nupvalues, 2, "index out of range");

    setobj(L, &closure->l.uprefs[upvalueIndex - 1], objToSet);
    return 0;
}

int getprotos(lua_State *L) {
    luaL_trimstack(L, 1);
    convert_level_or_function_to_closure(L, "Cannot get protos on C Closure");

    const auto closure = lua_toclosure(L, -1);
    Proto *originalProto = closure->l.p;

    lua_newtable(L);
    for (int i = 0; i < originalProto->sizep;) {
        const auto currentProto = originalProto->p[i];
        lua_pushrawclosure(L, luaF_newLclosure(L, currentProto->nups, closure->env, currentProto));
        lua_rawseti(L, -2, ++i);
    }

    return 1;
}

int getproto(lua_State *L) {
    luaL_trimstack(L, 3);
    const auto isActiveProto = luaL_optboolean(L, 3, false);
    luaL_checktype(L, 2, LUA_TNUMBER);
    convert_level_or_function_to_closure(L, "Cannot get proto on C Closure");

    const auto protoIndex = lua_tointeger(L, 2);
    const auto closure = lua_toclosure(L, -1);

    luaL_argcheck(L, protoIndex > 0, 2, "index cannot be negative");
    luaL_argcheck(L, protoIndex <= closure->l.p->sizep, 2, "index out of range");

    auto proto = closure->l.p->p[protoIndex - 1];
    if (isActiveProto) {
        lua_newtable(L);
        struct LookupContext {
            lua_State *L;
            int count;
            Closure *closure;
        } context{L, 0, closure};

        luaM_visitgco(L, &context, [](void *contextPointer, lua_Page *page, GCObject *gcObject) -> bool {
            const auto context = static_cast<LookupContext *>(contextPointer);
            if (isdead(context->L->global, gcObject))
                return false;

            if (gcObject->gch.tt == LUA_TFUNCTION) {
                const auto closure = (Closure *) gcObject;
                if (!closure->isC && closure->l.p == context->closure->l.p
                    ->p[context->count]) {
                    setclvalue(context->L, context->L->top, closure);
                    incr_top(context->L);
                    lua_rawseti(context->L, -2, ++context->count);
                }
            }

            return false;
        });

        return 1;
    }

    lua_pushrawclosure(L, luaF_newLclosure(L, proto->nups, closure->env, proto));
    return 1;
}

int getstack(lua_State *L) {
    luaL_checktype(L, 1, LUA_TNUMBER);
    luaL_trimstack(L, 2);

    const auto level = lua_tointeger(L, 1);
    const auto index = (lua_isnoneornil(L, 2)) ? -1 : luaL_checkinteger(L, 2);

    if (level >= (L->ci - L->base_ci) || level < 0)
        luaL_argerrorL(L, 1, "level out of range");

    if (index < -2 || index > 255)
        luaL_argerrorL(L, 2, "index out of range");

    const auto frame = L->ci - level;
    if (!frame->func || !ttisfunction(frame->func))
        luaL_argerrorL(L, 1, "invalid function in frame");

    if (clvalue(frame->func)->isC)
        luaL_argerrorL(L, 1, "level cannot point to a c closure");

    if (!frame->top || !frame->base || frame->top < frame->base)
        luaL_error(L, "invalid frame pointers");

    const size_t stackFrameSize = frame->top - frame->base;

    if (index == -1) {
        lua_newtable(L);
        for (int i = 0; i < stackFrameSize; i++) {
            setobj2s(L, L->top, &frame->base[i]);
            incr_top(L);

            lua_rawseti(L, -2, i + 1);
        }
    } else {
        if (index < 1 || index > stackFrameSize)
            luaL_argerrorL(L, 2, "index out of range");

        setobj2s(L, L->top, &frame->base[index - 1]);
        incr_top(L);
    }

    return 1;
}

int setstack(lua_State *L) {
    luaL_trimstack(L, 3);
    luaL_checktype(L, 1, LUA_TNUMBER);
    luaL_checktype(L, 2, LUA_TNUMBER);
    luaL_checkany(L, 3);

    const auto level = lua_tointeger(L, 1);
    const auto index = lua_tointeger(L, 2);

    if (level >= L->ci - L->base_ci || level < 0)
        luaL_argerrorL(L, 1, "level out of range");

    const auto frame = L->ci - level;
    const size_t top = frame->top - frame->base;

    if (clvalue(frame->func)->isC)
        luaL_argerrorL(L, 1, "level cannot point to a c closure");

    if (index < 1 || index > top)
        luaL_argerrorL(L, 2, "index out of range");

    if (frame->base[index - 1].tt != lua_type(L, 3))
        luaL_argerrorL(L, 3, "type of the value on the stack is different than the object you are setting it to");

    setobj2s(L, &frame->base[index -1], luaA_toobject(L, 3));
    return 0;
}


std::vector<RegistrationType> Debug::GetRegistrationTypes() {
    return {
        RegisterIntoGlobals{},
        RegisterIntoTable{"debug"}
    };
}

luaL_Reg *Debug::GetFunctions() {
     const auto Debug_functions = new luaL_Reg[] {
         {"getinfo", getinfo},
         {"getconstants", getconstants},
         {"getconstant", getconstant},
         {"setconstant", setconstant},
         {"getupvalues", getupvalues},
         {"getupvalue", getupvalue},
         {"setupvalue", setupvalue},
         {"getprotos", getprotos},
         {"getproto", getproto},
         {"getstack", getstack},
         {"setstack", setstack},
         {nullptr, nullptr}
     };

     return Debug_functions;
}
