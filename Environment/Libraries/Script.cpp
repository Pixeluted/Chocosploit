//
// Created by Pixeluted on 18/02/2025.
//

#include "Script.hpp"

#include <codecvt>
#include <hex.h>
#include <optional>
#include <ossig.h>
#include <sha.h>

#include "base64.h"
#include "lapi.h"
#include "lgc.h"
#include "lmem.h"
#include "WinReg.hpp"
#include "../../ApplicationContext.hpp"
#include "../../Execution/BytecodeCompiler.hpp"
#include "../../Execution/InternalTaskScheduler.hpp"
#include "../../Execution/LuauSecurity.hpp"
#include "cpr/api.h"

int getgc(lua_State *L) {
    const bool includeTables = luaL_optboolean(L, 1, false);

    lua_newtable(L);
    lua_newtable(L);

    lua_pushstring(L, "kvs");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);

    typedef struct {
        lua_State *luaThread;
        bool includeTables;
        int itemsFound;
    } GCOContext;

    auto GCContext = GCOContext{L, includeTables, 0};

    const auto oldGCThreshold = L->global->GCthreshold;
    L->global->GCthreshold = SIZE_MAX;

    luaM_visitgco(L, &GCContext, [](void *ctx, lua_Page *page, GCObject *gcObj) -> bool {
        const auto context = static_cast<GCOContext *>(ctx);
        const auto luaThread = context->luaThread;

        if (isdead(luaThread->global, gcObj))
            return false;

        const auto gcObjectType = gcObj->gch.tt;
        if (gcObjectType == LUA_TFUNCTION || gcObjectType == LUA_TTHREAD || gcObjectType == LUA_TUSERDATA ||
            gcObjectType == LUA_TLIGHTUSERDATA ||
            gcObjectType == LUA_TBUFFER || gcObjectType == LUA_TTABLE && context->includeTables) {
            luaThread->top->value.gc = gcObj;
            luaThread->top->tt = gcObjectType;
            incr_top(luaThread);

            const auto newTableIndex = context->itemsFound++;
            lua_rawseti(luaThread, -2, newTableIndex);
        }

        return false;
    });

    L->global->GCthreshold = oldGCThreshold;

    return 1;
}

enum FilterType : uint8_t {
    Function,
    Table
};

struct FunctionFilterOptions {
    const char *Name = nullptr;
    bool IgnoreExecutor = true;
    const char *Hash = nullptr;
    int constantsTableRef = LUA_NOREF;
    int upvaluesTableRef = LUA_NOREF;

    ~FunctionFilterOptions() {
        const auto L = ApplicationContext::GetService<InternalTaskScheduler>()->luaStates->executorLuaState;
        if (L == nullptr)
            return;

        if (constantsTableRef != LUA_NOREF)
            lua_unref(L, constantsTableRef);
        if (upvaluesTableRef != LUA_NOREF)
            lua_unref(L, upvaluesTableRef);
    }
};

struct TableFilterOptions {
    int keysTableRef = LUA_NOREF;
    int valuesTableRef = LUA_NOREF;
    int keyValuePairTableRef = LUA_NOREF;
    int metatableRef = LUA_NOREF;

    ~TableFilterOptions() {
        const auto L = ApplicationContext::GetService<InternalTaskScheduler>()->luaStates->executorLuaState;
        if (L == nullptr)
            return;

        if (keysTableRef != LUA_NOREF)
            lua_unref(L, keysTableRef);
        if (valuesTableRef != LUA_NOREF)
            lua_unref(L, valuesTableRef);
        if (keyValuePairTableRef != LUA_NOREF)
            lua_unref(L, keyValuePairTableRef);
        if (metatableRef != LUA_NOREF)
            lua_unref(L, metatableRef);
    }
};

bool tableContainsValues(lua_State *L, LuaTable *tableToCheck, LuaTable *tableToMatch) {
    lua_pushgc(L, tableToCheck);
    const auto tableToCheckIdx = lua_gettop(L);

    lua_pushgc(L, tableToMatch);
    const auto tableToMatchIdx = lua_gettop(L);

    bool doesMatch = true;

    lua_pushnil(L);
    while (lua_next(L, tableToMatchIdx) != 0) {
        bool isPresent = false;

        lua_pushnil(L);
        while (lua_next(L, tableToCheckIdx) != 0) {
            if (lua_equal(L, -1, -3)) {
                isPresent = true;
                lua_pop(L, 2); // Pop inner key and value
                break;
            }
            lua_pop(L, 1); // Pop inner value
        }

        if (!isPresent) {
            doesMatch = false;
            lua_pop(L, 2); // Pop key and value
            break;
        }

        lua_pop(L, 1); // Pop value
    }
    lua_pop(L, 2); // Pop the two pushed tables

    return doesMatch;
}

bool tableContainsKeys(lua_State *L, LuaTable *tableToCheck, LuaTable *tableToMatch) {
    lua_pushgc(L, tableToCheck);
    const auto tableToCheckIdx = lua_gettop(L);

    lua_pushgc(L, tableToMatch);
    const auto tableToMatchIdx = lua_gettop(L);

    bool doesMatch = true;

    lua_pushnil(L);
    while (lua_next(L, tableToMatchIdx) != 0) {
        bool isPresent = false;

        lua_pushnil(L);
        while (lua_next(L, tableToCheckIdx) != 0) {
            if (lua_equal(L, -2, -3)) {
                isPresent = true;
                lua_pop(L, 2); // Pop inner key and value
                break;
            }
            lua_pop(L, 1); // Pop inner value
        }

        if (!isPresent) {
            doesMatch = false;
            lua_pop(L, 2); // Pop key and value
            break;
        }

        lua_pop(L, 1); // Pop value
    }
    lua_pop(L, 2); // Pop the two pushed tables

    return doesMatch;
}

bool tableContainsKeyValuesPairs(lua_State *L, LuaTable *tableToCheck, LuaTable *tableToMatch) {
    lua_pushgc(L, tableToCheck);
    const auto tableToCheckIdx = lua_gettop(L);

    lua_pushgc(L, tableToMatch);
    const auto tableToMatchIdx = lua_gettop(L);

    bool doesMatch = true;

    lua_pushnil(L);
    while (lua_next(L, tableToMatchIdx) != 0) {
        bool isPresent = false;

        lua_pushnil(L);
        while (lua_next(L, tableToCheckIdx) != 0) {
            if (lua_equal(L, -2, -4) && lua_equal(L, -1, -3)) {
                isPresent = true;
                lua_pop(L, 2); // Pop inner key and value
                break;
            }
            lua_pop(L, 1); // Pop inner value
        }

        if (!isPresent) {
            doesMatch = false;
            lua_pop(L, 2); // Pop key and value
            break;
        }

        lua_pop(L, 1); // Pop value
    }
    lua_pop(L, 2); // Pop the two pushed tables

    return doesMatch;
}

int filtergc(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TTABLE);
    const auto filterTypeString = lua_tostring(L, 1);
    if (strcmp(filterTypeString, "function") != 0 && strcmp(filterTypeString, "table") != 0)
        luaL_argerrorL(L, 1, "Invalid filter type, expected function or table");
    const auto filterType = strcmp(filterTypeString, "function") == 0 ? Function : Table;
    const auto returnOne = luaL_optboolean(L, 3, false);

    if (filterType == Function) {
        auto filterOptions = FunctionFilterOptions{};

        lua_pushvalue(L, 2);

        lua_getfield(L, -1, "Name");
        if (!lua_isnil(L, -1)) {
            if (!lua_isstring(L, -1))
                luaL_argerrorL(L, 2, "Name must be a string");
            filterOptions.Name = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "IgnoreExecutor");
        if (!lua_isnil(L, -1)) {
            if (!lua_isboolean(L, -1))
                luaL_argerrorL(L, 2, "IgnoreExecutor must be a boolean");
            filterOptions.IgnoreExecutor = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "Hash");
        if (!lua_isnil(L, -1)) {
            if (!lua_isstring(L, -1))
                luaL_argerrorL(L, 2, "Hash must be a string");
            filterOptions.Hash = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "Constants");
        if (!lua_isnil(L, -1)) {
            if (!lua_istable(L, -1))
                luaL_argerrorL(L, 2, "Constants must be a table");
            filterOptions.constantsTableRef = lua_ref(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "Upvalues");
        if (!lua_isnil(L, -1)) {
            if (!lua_istable(L, -1))
                luaL_argerrorL(L, 2, "Upvalues must be a table");
            filterOptions.upvaluesTableRef = lua_ref(L, -1);
        }
        lua_pop(L, 1);

        lua_pop(L, 1); // Pop the cloned value

        if (!returnOne) {
            lua_newtable(L);
        }
        const auto resultsTableIdx = lua_gettop(L);
        int resultsIndex = 0;

        lua_pushcfunction(L, getgc, "getgc");
        lua_pushboolean(L, false);
        lua_call(L, 1, 1);

        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            const auto givenObject = luaA_toobject(L, -1);
            if (givenObject->tt != LUA_TFUNCTION) {
                lua_pop(L, 1);
                continue;
            }

            const auto obtainedClosure = clvalue(givenObject);
            if (filterOptions.IgnoreExecutor) {
                lua_propergetglobal(L, "isexecutorclosure");
                lua_pushvalue(L, -2);
                lua_call(L, 1, 1);
                if (lua_toboolean(L, -1) == true) {
                    lua_pop(L, 2);
                    continue;
                }
                lua_pop(L, 1);
            }

            if (obtainedClosure->isC) {
                // Skip all c closures if hash, constants or upvalues are checked!
                if (filterOptions.Hash || filterOptions.constantsTableRef != LUA_NOREF || filterOptions.upvaluesTableRef
                    != LUA_NOREF) {
                    lua_pop(L, 1);
                    continue;
                }

                if (filterOptions.Name) {
                    if (obtainedClosure->c.debugname.get() == nullptr ||
                        strcmp(obtainedClosure->c.debugname.get(), filterOptions.Name) != 0) {
                        lua_pop(L, 1);
                        continue;
                    }
                }
            } else {
                if (filterOptions.Name) {
                    if (obtainedClosure->l.p->debugname.get() == nullptr || strcmp(
                            obtainedClosure->l.p->debugname.get()->data, filterOptions.Name) != 0) {
                        lua_pop(L, 1);
                        continue;
                    }
                }

                if (filterOptions.Hash) {
                    lua_propergetglobal(L, "getfunctionhash");
                    lua_pushvalue(L, -2);
                    lua_call(L, 1, 1);
                    if (strcmp(lua_tostring(L, -1), filterOptions.Hash) != 0) {
                        lua_pop(L, 2);
                        continue;
                    }
                    lua_pop(L, 1);
                }

                if (filterOptions.constantsTableRef != LUA_NOREF) {
                    lua_propergetglobal(L, "getconstants");
                    lua_pushvalue(L, -2);
                    lua_call(L, 1, 1);
                    const auto tableToCheck = hvalue(luaA_toobject(L, -1));
                    lua_getref(L, filterOptions.constantsTableRef);
                    const auto tableToMatch = hvalue(luaA_toobject(L, -1));

                    if (!tableContainsValues(L, tableToCheck, tableToMatch)) {
                        lua_pop(L, 3);
                        continue;
                    }

                    lua_pop(L, 2);
                }

                if (filterOptions.upvaluesTableRef != LUA_NOREF) {
                    lua_propergetglobal(L, "getupvalues");
                    lua_pushvalue(L, -2);
                    lua_call(L, 1, 1);
                    const auto tableToCheck = hvalue(luaA_toobject(L, -1));
                    lua_getref(L, filterOptions.upvaluesTableRef);
                    const auto tableToMatch = hvalue(luaA_toobject(L, -1));

                    if (!tableContainsValues(L, tableToCheck, tableToMatch)) {
                        lua_pop(L, 3);
                        continue;
                    }

                    lua_pop(L, 2);
                }
            }

            // If the object reached here, it means it passed all checks
            if (returnOne)
                return 1;
            lua_rawseti(L, resultsTableIdx, ++resultsIndex); // This will pop the value for us
        }
        lua_pop(L, 1);

        // If we got here with return one it means nothing was found, so just return nil
        if (returnOne) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushvalue(L, resultsTableIdx);
        return 1;
    } else {
        auto filterOptions = TableFilterOptions{};

        lua_pushvalue(L, 2);

        lua_getfield(L, -1, "Keys");
        if (!lua_isnil(L, -1)) {
            if (!lua_istable(L, -1))
                luaL_argerrorL(L, 2, "Keys must be a table");
            filterOptions.keysTableRef = lua_ref(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "Values");
        if (!lua_isnil(L, -1)) {
            if (!lua_istable(L, -1))
                luaL_argerrorL(L, 2, "Values must be a table");
            filterOptions.valuesTableRef = lua_ref(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "KeyValuePairs");
        if (!lua_isnil(L, -1)) {
            if (!lua_istable(L, -1))
                luaL_argerrorL(L, 2, "KeyValuePairs must be a table");
            filterOptions.keyValuePairTableRef = lua_ref(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "Metatable");
        if (!lua_isnil(L, -1)) {
            if (!lua_istable(L, -1))
                luaL_argerrorL(L, 2, "Metatable must be a table");
            filterOptions.metatableRef = lua_ref(L, -1);
        }
        lua_pop(L, 1);

        lua_pop(L, 1); // Pop the cloned value

        if (!returnOne) {
            lua_newtable(L);
        }
        const auto resultsTableIdx = lua_gettop(L);
        int resultsIndex = 0;

        lua_pushcfunction(L, getgc, "getgc");
        lua_pushboolean(L, true);
        lua_call(L, 1, 1);

        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            const auto receivedObject = luaA_toobject(L, -1);
            if (receivedObject->tt != LUA_TTABLE) {
                lua_pop(L, 1);
                continue;
            }

            const auto receivedTable = hvalue(receivedObject);

            if (filterOptions.metatableRef != LUA_NOREF) {
                if (receivedTable->metatable == nullptr) {
                    lua_pop(L, 1);
                    continue;
                }

                lua_getref(L, filterOptions.metatableRef);
                lua_pushgc(L, receivedTable->metatable);
                if (!lua_equal(L, -1, -2)) {
                    lua_pop(L, 3);
                    continue;
                }
                lua_pop(L, 2);
            }

            if (filterOptions.valuesTableRef != LUA_NOREF) {
                lua_getref(L, filterOptions.valuesTableRef);
                const auto tableToMatch = hvalue(luaA_toobject(L, -1));
                if (receivedTable == tableToMatch) {
                    // Ignore tables passed to us
                    lua_pop(L, 2);
                    continue;
                }

                if (!tableContainsValues(L, receivedTable, tableToMatch)) {
                    lua_pop(L, 2); // Pop to match and value
                    continue;
                }
                lua_pop(L, 1);
            }

            if (filterOptions.keysTableRef != LUA_NOREF) {
                lua_getref(L, filterOptions.keysTableRef);
                const auto tableToMatch = hvalue(luaA_toobject(L, -1));
                if (!tableContainsKeys(L, receivedTable, tableToMatch)) {
                    lua_pop(L, 2); // Pop to match and value
                    continue;
                }
                lua_pop(L, 1);
            }

            if (filterOptions.keyValuePairTableRef != LUA_NOREF) {
                lua_getref(L, filterOptions.keyValuePairTableRef);
                const auto tableToMatch = hvalue(luaA_toobject(L, -1));
                if (receivedTable == tableToMatch) {
                    // Ignore tables passed to us
                    lua_pop(L, 2);
                    continue;
                }

                if (!tableContainsKeyValuesPairs(L, receivedTable, tableToMatch)) {
                    lua_pop(L, 2); // Pop to match and value
                    continue;
                }
                lua_pop(L, 1);
            }

            // If the object reached here, it means it passed all checks
            if (returnOne)
                return 1;
            lua_rawseti(L, resultsTableIdx, ++resultsIndex); // This will pop the value for us
        }
        lua_pop(L, 1); // Pop the table

        // If we got here with return one it means nothing was found, so just return nil
        if (returnOne) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushvalue(L, resultsTableIdx);
        return 1;
    }

    luaG_runerrorL(L, "not implemented");
}

enum BytecodeType : uint8_t {
    LocalScriptBytecode,
    ModuleScriptBytecode
};

std::optional<std::string> getScriptBytecode(const uintptr_t rawInstance, const BytecodeType bytecodeType) {
    uintptr_t protectedString = 0;
    if (bytecodeType == LocalScriptBytecode)
        protectedString = *reinterpret_cast<uintptr_t *>(
            rawInstance + ChocoSploit::StructOffsets::LocalScript::ProtectedBytecode);
    else if (bytecodeType == ModuleScriptBytecode)
        protectedString = *reinterpret_cast<uintptr_t *>(
            rawInstance + ChocoSploit::StructOffsets::ModuleScript::ProtectedBytecode);
    else
        throw std::runtime_error("Invalid bytecode type!");

    if (!protectedString)
        return std::nullopt;

    const auto bytecodeSize = *reinterpret_cast<int32_t *>(protectedString + 0x20);
    std::string compressedBytecode(*reinterpret_cast<const char **>(protectedString + 0x10), bytecodeSize);
    if (!compressedBytecode.data())
        return std::nullopt;

    return compressedBytecode;
}

int getscriptbytecode(lua_State *L) {
    CheckClassNames(L, 1, "LocalScript", "ModuleScript", "Script");

    if (IsClassName(L, 1, "Script")) {
        lua_getglobal(L, "Enum");
        lua_getfield(L, -1, "RunContext");
        lua_getfield(L, -1, "Client");
        lua_pushvalue(L, 1);
        lua_getfield(L, -1, "RunContext");
        if (!lua_equal(L, -1, -3)) {
            luaL_argerrorL(L, 1, "Expected Script with RunContext set to Client");
        }

        lua_pop(L, 5);
    }

    const auto rawInstance = *static_cast<uintptr_t *>(lua_touserdata(L, 1));
    const auto bytecodeType = IsClassName(L, 1, "LocalScript") || IsClassName(L, 1, "Script")
                                  ? LocalScriptBytecode
                                  : ModuleScriptBytecode;

    const auto bytecodeResultsOpt = getScriptBytecode(rawInstance, bytecodeType);
    if (!bytecodeResultsOpt.has_value())
        luaG_runerrorL(L, "Failed to get bytecode!");
    const auto &rawBytecode = bytecodeResultsOpt.value();
    if (rawBytecode.empty()) {
        lua_pushnil(L);
        return 1;
    }

    const auto decompressedBytecodeOpt = DecompressBytecode(rawBytecode);
    if (!decompressedBytecodeOpt.has_value())
        luaG_runerrorL(L, "Failed to decompress bytecode!");
    const auto &decompressedBytecode = decompressedBytecodeOpt.value();

    lua_pushlstring(L, decompressedBytecode.c_str(), decompressedBytecode.size());
    return 1;
}

int getscripthash(lua_State *L) {
    CheckClassNames(L, 1, "LocalScript", "ModuleScript", "Script");

    if (IsClassName(L, 1, "Script")) {
        lua_getglobal(L, "Enum");
        lua_getfield(L, -1, "RunContext");
        lua_getfield(L, -1, "Client");
        lua_pushvalue(L, 1);
        lua_getfield(L, -1, "RunContext");
        if (!lua_equal(L, -1, -3)) {
            luaL_argerrorL(L, 1, "Expected Script with RunContext set to Client");
        }

        lua_pop(L, 5);
    }

    const auto rawInstance = *static_cast<uintptr_t *>(lua_touserdata(L, 1));
    const auto bytecodeType = IsClassName(L, 1, "LocalScript") || IsClassName(L, 1, "Script")
                                  ? LocalScriptBytecode
                                  : ModuleScriptBytecode;

    const auto bytecodeResultsOpt = getScriptBytecode(rawInstance, bytecodeType);
    if (!bytecodeResultsOpt.has_value())
        luaG_runerrorL(L, "Failed to get bytecode!");
    const auto &receivedBytecode = bytecodeResultsOpt.value();
    if (receivedBytecode.empty()) {
        lua_pushnil(L);
        return 1;
    }

    CryptoPP::SHA384 hash;
    byte digest[CryptoPP::SHA384::DIGESTSIZE];
    hash.CalculateDigest(digest, (byte *) receivedBytecode.c_str(), receivedBytecode.size());

    CryptoPP::HexEncoder encoder;
    std::string finalHash;
    encoder.Attach(new CryptoPP::StringSink(finalHash));
    encoder.Put(digest, sizeof(digest));
    encoder.MessageEnd();

    lua_pushstring(L, finalHash.c_str());
    return 1;
}

int getsenv(lua_State *L) {
    CheckClassNames(L, 1, "LocalScript", "ModuleScript", "Script");

    if (IsClassName(L, 1, "Script")) {
        lua_getglobal(L, "Enum");
        lua_getfield(L, -1, "RunContext");
        lua_getfield(L, -1, "Client");
        lua_pushvalue(L, 1);
        lua_getfield(L, -1, "RunContext");
        if (!lua_equal(L, -1, -3)) {
            luaL_argerrorL(L, 1, "Expected Script with RunContext set to Client");
        }

        lua_pop(L, 5);
    }

    const auto rawInstance = *static_cast<uintptr_t *>(lua_touserdata(L, 1));
    if (IsClassName(L, 1, "LocalScript") || IsClassName(L, 1, "Script")) {
        auto shouldGcScanBeUsed = false;
        lua_pushvalue(L, 1);
        lua_getfield(L, -1, "GetActor");
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        lua_remove(L, -2); // Remove the instance from stack
        if (!lua_isnil(L, -1)) {
            shouldGcScanBeUsed = true;
        } else {
            lua_pushvalue(L, 1);
            lua_getfield(L, -1, "IsDescendantOf");
            lua_pushvalue(L, 1);
            lua_pushnil(L);
            lua_call(L, 2, 1);
            lua_remove(L, -2); // Remove instance from stack
            if (lua_toboolean(L, -1) == true) {
                shouldGcScanBeUsed = true;
            }
        }

        if (shouldGcScanBeUsed) {
            // Script is under actor, scan the GC for thread, if it's not found it's really running under actor, if it's found then it's fake actor script
            // Or the script is in nil and sometimes that can nill out threadNode too, so scan for it too
            typedef struct {
                lua_State *currentLuaState;
                lua_State *foundLuaThread;
                uintptr_t scriptInstance;
            } GCOContext;

            auto GCCContext = GCOContext{L, nullptr, rawInstance};
            const auto oldGCThreshold = L->global->GCthreshold;
            L->global->GCthreshold = SIZE_MAX;

            luaM_visitgco(L, &GCCContext, [](void *ctx, lua_Page *page, GCObject *gcObj) -> bool {
                const auto context = static_cast<GCOContext *>(ctx);
                const auto luaThread = context->currentLuaState;

                if (isdead(luaThread->global, gcObj))
                    return false;

                if (gcObj->gch.tt == LUA_TTHREAD) {
                    const auto foundLuaThread = &gcObj->th;
                    const auto luaUserdata = static_cast<LuauUserdata *>(foundLuaThread->userdata);
                    if (luaUserdata->Script.expired())
                        return false;

                    if (reinterpret_cast<uintptr_t>(luaUserdata->Script.lock().get()) == context->scriptInstance) {
                        context->foundLuaThread = foundLuaThread;
                        return false;
                    }
                }

                return false;
            });

            L->global->GCthreshold = oldGCThreshold;
            if (GCCContext.foundLuaThread) {
                L->top->tt = LUA_TTABLE;
                L->top->value.gc = reinterpret_cast<GCObject *>(GCCContext.foundLuaThread->gt);
                incr_top(L);
                return 1;
            } else {
                luaG_runerrorL(L, "This script isn't running");
            }
        }

        const auto threadNodeStuff = *reinterpret_cast<uintptr_t *>(
            rawInstance + ChocoSploit::StructOffsets::LocalScript::ThreadNodeList);
        if (threadNodeStuff == 0 || IsBadReadPtr(reinterpret_cast<void *>(threadNodeStuff), 0x10))
            luaG_runerrorL(L, "This script isn't running");

        for (uintptr_t currentNode = *reinterpret_cast<uintptr_t *>(threadNodeStuff + 0x8); currentNode != 0;
             currentNode = *reinterpret_cast<uintptr_t *>(
                 currentNode + ChocoSploit::StructOffsets::LocalScript::ThreadNode::next)) {
            const auto liveLuaRef = *reinterpret_cast<uintptr_t *>(
                currentNode + ChocoSploit::StructOffsets::LocalScript::ThreadNode::liveLuaRef);
            if (liveLuaRef == 0 || IsBadReadPtr(reinterpret_cast<void *>(liveLuaRef), 0x10)) continue;
            const auto luaStateAddress = *reinterpret_cast<uintptr_t *>(liveLuaRef + 0x8);
            if (luaStateAddress == 0 || IsBadReadPtr(reinterpret_cast<void *>(luaStateAddress), 0x10)) continue;
            const auto runningLuaState = reinterpret_cast<lua_State *>(luaStateAddress);
            const auto associatedScript = static_cast<LuauUserdata *>(runningLuaState->userdata)->Script;
            if (reinterpret_cast<uintptr_t>(associatedScript.lock().get()) != rawInstance) continue;
            if (runningLuaState->global->mainthread != L->global->mainthread) {
                lua_pushnil(L);
                return 1;
            }

            L->top->tt = LUA_TTABLE;
            L->top->value.gc = reinterpret_cast<GCObject *>(runningLuaState->gt);
            incr_top(L);

            return 1;
        }

        luaG_runerrorL(L, "This script isn't running");
    } else {
        const auto vmStateMap = reinterpret_cast<uintptr_t *>(
            rawInstance + ChocoSploit::StructOffsets::ModuleScript::VmStateMap);
        if (vmStateMap == nullptr)
            luaG_runerrorL(L, "This module wasn't required!");

        auto mainThread = L->global->mainthread;
        uintptr_t receivedVmState;
        const auto additionalData = GetModuleVmState(vmStateMap, &receivedVmState, &mainThread);
        if (receivedVmState == 0)
            luaG_runerrorL(L, "This module wasn't required!");

        const auto isLoadedStatus = *reinterpret_cast<int32_t *>(
            *reinterpret_cast<uintptr_t *>(additionalData) +
            ChocoSploit::StructOffsets::ModuleScript::VmState::LoadedStatus);
        if (isLoadedStatus != 3)
            luaG_runerrorL(L, "This module wasn't required!");
        const auto moduleLuaState = *reinterpret_cast<lua_State **>(
            receivedVmState + ChocoSploit::StructOffsets::ModuleScript::VmState::LuaState);

        lua_pushvalue(moduleLuaState, LUA_GLOBALSINDEX);
        lua_xmove(moduleLuaState, L, 1);
        return 1;
    }

    return 0;
}

int getscriptclosure(lua_State *L) {
    CheckClassNames(L, 1, "LocalScript", "ModuleScript", "Script");

    const auto rawInstance = *static_cast<uintptr_t *>(lua_touserdata(L, 1));
    const auto bytecodeType = IsClassName(L, 1, "LocalScript") || IsClassName(L, 1, "Script")
                                  ? LocalScriptBytecode
                                  : ModuleScriptBytecode;
    const auto bytecodeResultsOpt = getScriptBytecode(rawInstance, bytecodeType);
    if (!bytecodeResultsOpt)
        luaG_runerrorL(L, "Failed to get bytecode!");
    auto bytecode = bytecodeResultsOpt.value();
    if (LuaVM_Load(L, &bytecode, "", 0) != LUA_OK) {
        lua_error(L);
    }

    const auto loadedClosure = lua_toclosure(L, -1);
    ElevateProto(loadedClosure, 2, true);

    lua_newtable(L);
    lua_pushinstance(L, &lua_torawuserdata(L, 1));
    lua_setfield(L, -2, "script");

    lua_newtable(L);
    lua_pushgc(L, L->global->mainthread->gt);
    lua_setfield(L, -2, "__index");
    lua_pushstring(L, "The metatable is locked");
    lua_setfield(L, -2, "__metatable");
    lua_setreadonly(L, -1, true);

    lua_setmetatable(L, -2);
    lua_setfenv(L, -2);

    return 1;
}

std::string ws2s(const std::wstring &wstr) {
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.to_bytes(wstr);
}

int decompile(lua_State *L) {
    getscriptbytecode(L);
    if (lua_isnil(L, -1)) {
        lua_pushstring(L, "-- Bytecode is empty");
        return 1;
    }

    size_t bytecodeLength;
    const auto scriptBytecode = lua_tolstring(L, -1, &bytecodeLength);

    winreg::RegKey exploitSettings;
    if (exploitSettings.TryOpen(HKEY_CURRENT_USER, L"Software\\Chocosploit", KEY_READ).Failed())
        luaG_runerrorL(L, "Failed to read settings");
    const auto accountIdResults = exploitSettings.TryGetStringValue(L"accountId");
    if (!accountIdResults.IsValid())
        luaG_runerrorL(L, "Failed to read settings");
    const auto accountId = accountIdResults.GetValue();

    ApplicationContext::GetService<InternalTaskScheduler>()->ScheduleYield(
        L, [scriptBytecode, bytecodeLength, accountId](const std::shared_ptr<YieldingRequest> &request) {
            std::string finalBytecode;
            CryptoPP::Base64Encoder encoder{new CryptoPP::StringSink(finalBytecode), false};
            encoder.Put((byte *) scriptBytecode, bytecodeLength);
            encoder.MessageEnd();

            const auto decompilationResults = cpr::Post(
                cpr::Url{std::format("https://chocosploit.dev/utility/decompile?accountId={}", ws2s(accountId))},
                cpr::Body{finalBytecode}, cpr::Header{{"Content-Type", "text/plain"}});

            request->completionCallback = [decompilationResults](lua_State *L) -> YieldResult {
                lua_pushlstring(L, decompilationResults.text.c_str(), decompilationResults.text.length());
                return {true, 1};
            };
            request->isReady = true;
        });

    return lua_yield(L, 1);
}

int getscriptfromthread(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTHREAD);

    const auto niggaThread = thvalue(luaA_toobject(L, 1));
    const auto niggaUserdata = static_cast<LuauUserdata*>(niggaThread->userdata);

    if (niggaUserdata->Script.expired() || niggaUserdata->Script.lock() == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    PushInstance(L, (void**)&niggaUserdata->Script);
    return 1;
}

std::vector<RegistrationType> Script::GetRegistrationTypes() {
    return {
        RegisterIntoGlobals{}
    };
}

luaL_Reg *Script::GetFunctions() {
     const auto Script_functions = new luaL_Reg[] {
         {"getgc", getgc},
         {"filtergc", filtergc},
         {"getscriptbytecode", getscriptbytecode},
         {"getscripthash", getscripthash},
         {"getsenv", getsenv},
         {"getscriptclosure", getscriptclosure},
         {"decompile", decompile},
         {"getscriptfromthread", getscriptfromthread},
         {nullptr, nullptr}
     };

     return Script_functions;
}
