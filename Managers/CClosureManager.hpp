//
// Created by Pixeluted on 16/02/2025.
//
#pragma once

#include <memory>
#include <optional>
#include <regex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "lobject.h"
#include "../Logger.hpp"

template<typename T, ::lua_Type U>
struct ReferencedLuauObject {
    int luaRef;

    ReferencedLuauObject() { this->luaRef = LUA_REFNIL; }

    explicit ReferencedLuauObject(int ref) { this->luaRef = ref; }

    ReferencedLuauObject(lua_State *L, int idx) { this->luaRef = lua_ref(L, idx); }

    void UnreferenceObject(lua_State *L) {
        if (this->luaRef > LUA_REFNIL)
            lua_unref(L, this->luaRef);
    }

    std::optional<T> GetReferencedObject(lua_State *L) {
        try {
            if (this->luaRef <= LUA_REFNIL)
                return {};

            lua_getref(L, this->luaRef);
            if (lua_type(L, -1) != U) {
                lua_pop(L, 1);
                return {};
            }

            const auto ptr = lua_topointer(L, -1);
            lua_pop(L, 1);
            return reinterpret_cast<T>(const_cast<void *>(ptr));
        } catch (const std::exception &ex) {
            const auto logger = ApplicationContext::GetService<Logger>();
            logger->Log(Info, "Reference is invalid? Wtf");
            return {};
        }
    }
};

class CClosureManager final : public Service {
public:
    std::unordered_map<Closure *, ReferencedLuauObject<Closure *, LUA_TFUNCTION>> newCClosureMap;
    std::unordered_set<Closure *> executorCClosures;
    static std::regex errorChunkInfoPattern;

    static std::string CleanErrorMessage(const std::string &errorToClean);

    static int NewCClosureProxy(lua_State *L);

    static int NewCClosureCont(lua_State *L, int status);

    Closure *WrapClosure(lua_State *L, Closure *closureToWrap, std::string_view functionName);
};
