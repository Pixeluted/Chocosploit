//
// Created by Pixeluted on 16/02/2025.
//

#include "Closures.hpp"

#include <filters.h>
#include <format>
#include <hex.h>
#include <iomanip>
#include <sha.h>
#include <sstream>

#include "lapi.h"
#include "lfunc.h"
#include "lgc.h"
#include "lstate.h"
#include "ltable.h"
#include "../../Logger.hpp"
#include "../../Execution/BytecodeCompiler.hpp"
#include "../../Execution/LuauSecurity.hpp"
#include "../../Managers/CClosureManager.hpp"

int checkcaller(lua_State *L) {
    lua_pushboolean(L, IsOurThread(L));
    return 1;
}

int newcclosure(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    const auto closureToWrap = lua_toclosure(L, 1);

    const auto wrappedClosure = ApplicationContext::GetService<CClosureManager>()->WrapClosure(
        L, closureToWrap, luaL_optstring(L, 2, ""));
    lua_pushrawclosure(L, wrappedClosure);

    return 1;
}

int newlclosure(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);

    lua_newtable(L);
    lua_newtable(L);

    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);
    lua_setmetatable(L, -2);

    lua_pushvalue(L, 1);
    lua_setfield(L, -2, "NEW_L_CLOSURE");
    const auto code = "return NEW_L_CLOSURE(...)";
    auto compiledBytecode = CompileBytecode(code);
    luau_load(L, std::format("{}_newlclosurewrapper", EXECUTOR_CHUNK_NAME).c_str(), compiledBytecode.c_str(),
              compiledBytecode.length(), -1);
    lua_getfenv(L, 1);
    lua_setfenv(L, -3);

    const auto loadedClosure = lua_toclosure(L, -1);
    ElevateProto(loadedClosure, 8, true);

    return 1;
}

int isexecutorclosure(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);

    const auto closureToCheck = lua_toclosure(L, 1);
    if (closureToCheck->isC) {
        lua_pushboolean(
            L, ApplicationContext::GetService<CClosureManager>()->executorCClosures.contains(closureToCheck));
        return 1;
    }

    lua_pushboolean(L, IsOurProto(closureToCheck->l.p));
    return 1;
}

int iscclosure(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushboolean(L, lua_iscfunction(L, 1));
    return 1;
}

int islclosure(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushboolean(L, !lua_iscfunction(L, 1));
    return 1;
}

int clonefunction(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);

    if (!lua_iscfunction(L, 1)) {
        lua_clonefunction(L, 1);
        lua_getfenv(L, -2);
        lua_setfenv(L, -3);
        return 1;
    }

    const auto originalClosure = lua_toclosure(L, 1);
    const auto clonedClosure = luaF_newCclosure(L, originalClosure->nupvalues, originalClosure->env);
    lua_pushrawclosure(L, clonedClosure);
    lua_ref(L, -1);
    lua_pop(L, 1);

    if (originalClosure->c.debugname != nullptr) {
        const auto originalDebugName = static_cast<const char *>(originalClosure->c.debugname);
        clonedClosure->c.debugname = originalDebugName;
    }

    for (int i = 0; i < originalClosure->nupvalues; i++)
        setobj2n(L, &clonedClosure->c.upvals[i], &originalClosure->c.upvals[i])

    const auto originalF = static_cast<lua_CFunction>(originalClosure->c.f);
    const auto originalCont = static_cast<lua_Continuation>(originalClosure->c.cont);
    clonedClosure->c.f = originalF;
    clonedClosure->c.cont = originalCont;

    auto newCClosureHandler = ApplicationContext::GetService<CClosureManager>();
    auto newCClosureHandlerMap = newCClosureHandler->newCClosureMap;
    if (newCClosureHandlerMap.
        contains(originalClosure)) {
        // This has to be like that, or it will not work!
        newCClosureHandler->newCClosureMap[clonedClosure] = newCClosureHandlerMap.at(originalClosure);
    }

    lua_pushrawclosure(L, clonedClosure);
    return 1;
}

int getcallingscript(lua_State *L) {
    const auto userdata = static_cast<LuauUserdata *>(L->userdata);
    const auto callingScript = userdata->Script;

    if (callingScript.expired() || callingScript.lock() == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    PushInstance(L, (void **) &callingScript);
    return 1;
}

int hookfunction(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_getglobal(L, "clonefunction");
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    const auto originalClonedFunction = lua_toclosure(L, -1);
    const auto functionToHook = lua_toclosure(L, 1);
    const auto functionToHookWith = lua_toclosure(L, 2);

    const auto logger = ApplicationContext::GetService<Logger>();

    const auto newCClosureHandler = ApplicationContext::GetService<CClosureManager>();
    // NC -> NC
    if (newCClosureHandler->newCClosureMap.contains(functionToHook) && newCClosureHandler->newCClosureMap.contains(
            functionToHookWith)) {
        //logger->Log(Info, "NC -> NC");
        //Sleep(500);
        newCClosureHandler->newCClosureMap[originalClonedFunction] = newCClosureHandler->newCClosureMap[functionToHook];
        newCClosureHandler->newCClosureMap[functionToHook] = newCClosureHandler->newCClosureMap[functionToHookWith];
        lua_pushrawclosure(L, originalClonedFunction);
        return 1;
    }

    // NC -> ANY
    if ((functionToHook->isC && newCClosureHandler->newCClosureMap.contains(functionToHook))) {
        //logger->Log(Info, "NC -> ANY");
        //Sleep(500);
        if (!functionToHookWith->isC) {
            lua_getglobal(L, "newcclosure");
            lua_pushvalue(L, 2);
            lua_call(L, 1, 1);
        } else {
            lua_pushvalue(L, 2);
            lua_ref(L, -1);
        }

        newCClosureHandler->newCClosureMap[originalClonedFunction] = newCClosureHandler->newCClosureMap[functionToHook];
        newCClosureHandler->newCClosureMap[functionToHook] = {L, -1};

        lua_pushrawclosure(L, originalClonedFunction);
        return 1;
    }

    // C -> L/C/NC
    if (functionToHook->isC) {
        //logger->Log(Info, "C -> L/C/NC");
        //Sleep(500);
        if (functionToHookWith->isC && newCClosureHandler->newCClosureMap.contains(functionToHookWith)) {
            lua_pushvalue(L, 2);
        } else {
            lua_getglobal(L, "newcclosure");
            lua_pushvalue(L, 2);
            lua_call(L, 1, 1);
        }

        const auto finalHookWith = lua_toclosure(L, -1);

        const lua_CFunction hookWithF = finalHookWith->c.f;
        const lua_Continuation hookWithCont = finalHookWith->c.cont;
        newCClosureHandler->newCClosureMap[functionToHook] = {L, -1};

        functionToHook->c.f = [](lua_State *L) { return 0; };
        functionToHook->c.cont = [](lua_State *L, int status) { return 0; };
        functionToHook->env = finalHookWith->env;
        functionToHook->stacksize = finalHookWith->stacksize;
        functionToHook->preload = finalHookWith->preload;

        for (int i = 0; i < functionToHook->nupvalues; i++)
            setobj2n(L, &functionToHook->c.upvals[i], &finalHookWith->c.upvals[i]);

        functionToHook->nupvalues = finalHookWith->nupvalues;
        functionToHook->c.f = hookWithF;
        functionToHook->c.cont = hookWithCont;

        lua_pushrawclosure(L, originalClonedFunction);
        return 1;
    }

    // L-> L/C/NC
    if (!functionToHook->isC) {
        //logger->Log(Info, "L -> L/C/NC");
        //Sleep(500);
        if (functionToHookWith->isC || functionToHookWith->nupvalues > functionToHook->nupvalues) {
            lua_getglobal(L, "newlclosure");
            lua_pushvalue(L, 2);
            lua_call(L, 1, 1);
        } else {
            lua_pushvalue(L, 2);
        }
        lua_ref(L, -1);

        const auto finalHookWith = lua_toclosure(L, -1);

        Proto *hookWithProto = finalHookWith->l.p;
        functionToHook->env = finalHookWith->env;
        functionToHook->stacksize = finalHookWith->stacksize;
        functionToHook->preload = finalHookWith->preload;

        for (int i = 0; i < finalHookWith->nupvalues; i++) {
            setobj2n(L, &functionToHook->l.uprefs[i], &finalHookWith->l.uprefs[i]);
        }

        functionToHook->nupvalues = finalHookWith->nupvalues;
        functionToHook->l.p = hookWithProto;

        lua_pushrawclosure(L, originalClonedFunction);
        lua_ref(L, -2);
        return 1;
    }

    luaG_runerrorL(L, "unsupported hook!");
}

int hookmetamethod(lua_State *L) {
    luaL_checkany(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    bool isReadonlyOriginal = false;
    if (!lua_getmetatable(L, 1)) {
        luaL_argerrorL(L, 1, "Object has no metatable!");
    } else {
        isReadonlyOriginal = lua_getreadonly(L, -1);
        lua_setreadonly(L, -1, false);
        lua_pop(L, 1);
    }

    const auto metamethodName = lua_tostring(L, 2);

    if (!luaL_getmetafield(L, 1, metamethodName))
        luaL_argerrorL(L, 3, "Metamethod doesn't exist on the target object!");

    if (!lua_isfunction(L, -1))
        luaL_argerrorL(L, 3, "Metamethod value isn't function!");

    lua_pushcclosure(L, hookfunction, nullptr, 0);
    lua_pushvalue(L, -2); // hookWhat
    lua_pushvalue(L, 3); // hookWith
    lua_call(L, 2, 1);

    lua_getmetatable(L, 1);
    lua_setreadonly(L, -1, isReadonlyOriginal);
    lua_pop(L, 1);
    return 1;
}

std::size_t calculateProtoBytesSize(const Proto *proto) {
    std::size_t size = 2 * sizeof(std::byte);

    for (auto i = 0; i < proto->sizek; i++) {
        switch (const auto currentConstant = &proto->k[i]; currentConstant->tt) {
            case LUA_TNIL:
                size += 1;
                break;
            case LUA_TSTRING: {
                const auto currentConstantString = &currentConstant->value.gc->ts;
                size += currentConstantString->len;
                break;
            }
            case LUA_TNUMBER: {
                size += sizeof(lua_Number);
                break;
            }
            case LUA_TBOOLEAN:
                size += sizeof(int);
                break;
            case LUA_TVECTOR:
                size += sizeof(float) * 3;
                break;
            case LUA_TTABLE: {
                const auto currentConstantTable = &currentConstant->value.gc->h;
                if (currentConstantTable->node != &luaH_dummynode) {
                    for (int nodeI = 0; nodeI < sizenode(currentConstantTable); nodeI++) {
                        const auto currentNode = &currentConstantTable->node[nodeI];
                        if (currentNode->key.tt != LUA_TSTRING)
                            continue;
                        const auto nodeString = &currentNode->key.value.gc->ts;
                        size += nodeString->len;
                    }
                }
                break;
            }
            case LUA_TFUNCTION: {
                const auto constantFunction = &currentConstant->value.gc->cl;
                if (constantFunction->isC) {
                    size += 1;
                    break;
                }
                size += calculateProtoBytesSize(constantFunction->l.p);
                break;
            }
            default:
                break;
        }
    }

    for (auto i = 0; i < proto->sizep; i++)
        size += calculateProtoBytesSize(proto->p[i]);

    size += proto->sizecode * sizeof(Instruction);

    return size;
}

std::vector<unsigned char> getProtoBytes(const Proto *proto) {
    std::vector<unsigned char> protoBytes{};
    auto protoBytesSize = calculateProtoBytesSize(proto);
    protoBytes.reserve(protoBytesSize);
    protoBytes.push_back(proto->is_vararg);

    for (auto i = 0; i < proto->sizek; i++) {
        switch (const auto currentConstant = &proto->k[i]; currentConstant->tt) {
            case LUA_TNIL:
                protoBytes.push_back(0);
                break;
            case LUA_TSTRING: {
                const auto currentConstantString = &currentConstant->value.gc->ts;
                protoBytes.insert(protoBytes.end(),
                                  reinterpret_cast<const char *>(currentConstantString->data),
                                  reinterpret_cast<const char *>(currentConstantString->data) + currentConstantString->
                                  len
                );
                break;
            }
            case LUA_TNUMBER: {
                const lua_Number *n = &currentConstant->value.n;
                protoBytes.insert(protoBytes.end(),
                                  reinterpret_cast<const char *>(n),
                                  reinterpret_cast<const char *>(n) + sizeof(lua_Number)
                );
                break;
            }
            case LUA_TBOOLEAN:
                protoBytes.push_back(currentConstant->value.b);
                break;
            case LUA_TVECTOR:
                protoBytes.push_back(reinterpret_cast<int *>(currentConstant->value.v)[0] & 0xff000000);
                protoBytes.push_back(reinterpret_cast<int *>(currentConstant->value.v)[0] & 0x00ff0000);
                protoBytes.push_back(reinterpret_cast<int *>(currentConstant->value.v)[0] & 0x0000ff00);
                protoBytes.push_back(reinterpret_cast<int *>(currentConstant->value.v)[0] & 0x000000ff);
                protoBytes.push_back(reinterpret_cast<int *>(currentConstant->value.v)[1] & 0xff000000);
                protoBytes.push_back(reinterpret_cast<int *>(currentConstant->value.v)[1] & 0x00ff0000);
                protoBytes.push_back(reinterpret_cast<int *>(currentConstant->value.v)[1] & 0x0000ff00);
                protoBytes.push_back(reinterpret_cast<int *>(currentConstant->value.v)[1] & 0x000000ff);
                protoBytes.push_back(currentConstant->extra[0] & 0xff000000); // z lives in extra
                protoBytes.push_back(currentConstant->extra[0] & 0x00ff0000);
                protoBytes.push_back(currentConstant->extra[0] & 0x0000ff00);
                protoBytes.push_back(currentConstant->extra[0] & 0x000000ff);
                break;
            case LUA_TTABLE: {
                const auto currentConstantTable = &currentConstant->value.gc->h;
                if (currentConstantTable->node != &luaH_dummynode) {
                    for (int nodeI = 0; nodeI < sizenode(currentConstantTable); nodeI++) {
                        const auto currentNode = &currentConstantTable->node[nodeI];
                        if (currentNode->key.tt != LUA_TSTRING)
                            continue;
                        const auto nodeString = &currentNode->key.value.gc->ts;
                        protoBytes.insert(protoBytes.end(),
                                          reinterpret_cast<const char *>(nodeString->data),
                                          reinterpret_cast<const char *>(nodeString->data) + nodeString->len
                        );
                    }
                }
                break;
            }
            case LUA_TFUNCTION: {
                const auto constantFunction = &currentConstant->value.gc->cl;
                if (constantFunction->isC) {
                    protoBytes.push_back('C');
                    break;
                }

                std::vector<unsigned char> functionBytes = getProtoBytes(constantFunction->l.p);
                protoBytes.insert(protoBytes.end(), functionBytes.data(), functionBytes.data() + functionBytes.size());
                break;
            }
            default:
                break;
        }
    }

    for (auto i = 0; i < proto->sizep; i++) {
        std::vector<unsigned char> currentProtoBytes = getProtoBytes(proto->p[i]);

        protoBytes.insert(protoBytes.end(), currentProtoBytes.data(),
                          currentProtoBytes.data() + currentProtoBytes.size()
        );
    }


    protoBytes.insert(protoBytes.end(), reinterpret_cast<const char *>(static_cast<Instruction *>(proto->code)),
                      reinterpret_cast<const char *>(static_cast<Instruction *>(proto->code)) + proto->sizecode * sizeof
                      (Instruction)
    );
    return protoBytes;
}

int getfunctionhash(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    const auto functionToHash = lua_toclosure(L, 1);
    if (functionToHash->isC)
        luaL_argerrorL(L, 1, "Expected L Closure");

    byte functionHashDigest[CryptoPP::SHA384::DIGESTSIZE];
    CryptoPP::SHA384 hasher;

    const auto functionBytes = getProtoBytes(functionToHash->l.p);
    hasher.CalculateDigest(functionHashDigest, functionBytes.data(), functionBytes.size());

    CryptoPP::HexEncoder encoder;
    std::string finalHash;
    encoder.Attach(new CryptoPP::StringSink(finalHash));
    encoder.Put(functionHashDigest, sizeof(functionHashDigest));
    encoder.MessageEnd();

    lua_pushlstring(L, finalHash.c_str(), finalHash.length());
    return 1;
}

int comparefunctions(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    const auto lovrePastes = lua_toclosure(L, 1);
    const auto msddPastes = lua_toclosure(L, 2);

    if (lovrePastes->isC != msddPastes->isC) {
        lua_pushboolean(L, false);
        return 1;
    }

    if (lovrePastes->isC) {
        lua_pushboolean(L, lovrePastes->c.f == msddPastes->c.f);
    } else {
        lua_pushboolean(L, lovrePastes->l.p == msddPastes->l.p);
    }
    return 1;
}

std::vector<RegistrationType> Closures::GetRegistrationTypes() {
    return {
        RegisterIntoGlobals{}
    };
}

luaL_Reg *Closures::GetFunctions() {
     const auto Closures_functions = new luaL_Reg[] {
         {"checkcaller", checkcaller},
         {"newcclosure", newcclosure},
         {"newlclosure", newlclosure},
         {"isexecutorclosure", isexecutorclosure},
         {"iscclosure", iscclosure},
         {"islclosure", islclosure},
         {"clonefunction", clonefunction},
         {"copyfunction", clonefunction},
         {"getcallingscript", getcallingscript},
         {"hookfunction", hookfunction},
         {"hookmetamethod", hookmetamethod},
         {"getfunctionhash", getfunctionhash},
         {"comparefunctions", comparefunctions},
         {nullptr, nullptr}
     };

     return Closures_functions;
}
