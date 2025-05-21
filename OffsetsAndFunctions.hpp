//
// Created by Pixeluted on 03/02/2025.
//
#pragma once
#define RebaseOffset(x) reinterpret_cast<uintptr_t>(GetModuleHandleA("RobloxPlayerBeta.exe")) + x
#define RebaseHyperionOffset(x) reinterpret_cast<uintptr_t>(GetModuleHandleA("RobloxPlayerBeta.dll")) + x
#define DefineFunction(functionName, offsetName, functionType) \
    static ChocoSploit::FunctionDefinitions::functionType functionName = reinterpret_cast<ChocoSploit::FunctionDefinitions::functionType>(RebaseOffset(ChocoSploit::Offsets::Functions::offsetName));
#define DefinePointer(pointerName, pointerOffsetName) \
    static uintptr_t pointerName = RebaseOffset(ChocoSploit::Offsets::Pointers::pointerOffsetName);
#define DefineHyperionPointer(pointerName, pointerOffsetName) \
    static uintptr_t pointerName = RebaseHyperionOffset(ChocoSploit::Offsets::Pointers::pointerOffsetName);
#include <cstdint>
#include <windows.h>
#include <string>

struct lua_State;

struct WeakThreadRef {
    int _Refs;
    lua_State *thread;
    int32_t thread_ref;
    int32_t objectId;
};

namespace ChocoSploit {
    namespace StructOffsets {
        namespace TaskScheduler {
            constexpr uintptr_t JobStart = 0x1D0;
            constexpr uintptr_t JobEnd = 0x1D8;

            namespace Job {
                constexpr uintptr_t Name = 0x18;
                constexpr uintptr_t TypeName = 0x150;
                namespace WaitingHybridScriptsJob {
                    constexpr uintptr_t ScriptContext = 0x1F8;
                }
                namespace RenderJob {
                    constexpr uintptr_t RenderView = 0x218;
                    constexpr uintptr_t DeviceD3D11 = 0x8;
                    constexpr uintptr_t SwapChain = 0xC8;
                }
            }
            namespace AppInfo {
                constexpr uintptr_t InGame = 0x310;
            }
        }

        namespace ScriptContext {
            constexpr uintptr_t GlobalStateMap = 0x140;
            constexpr uintptr_t SharedGlobals = 0x278;
            constexpr uintptr_t LoadedModulesSet = 0x678; // Unused but might be useful
            constexpr uintptr_t ResumeFaceOffset = 0x610;
            namespace VmState {
                constexpr uintptr_t LuaState = 0x88;
            }
        }

        namespace Instance {
            constexpr uintptr_t InstanceClassDescriptor = 0x18;
            namespace ClassDescriptor {
                constexpr uintptr_t PropertiesStart = 0x30;
                constexpr uintptr_t PropertiesEnd = 0x38;
                constexpr uintptr_t PropertyDescriptor = 0x3B8;
                constexpr uintptr_t Type = 0x50;
                constexpr uintptr_t TypeGetSetDescriptor = 0x88;
                namespace GetSetDescriptor {
                    constexpr uintptr_t getVFtableFunc = 0x18;
                }
                namespace TypeStructure {
                    enum PropertyType : uint32_t {
                        BinaryString = 0x1E,
                        SystemAddress = 0x1D,
                        SharedString = 0x3D,
                        Enum = 0x20,
                        String = 0x6,
                        Instance = 0x8
                    };

                    constexpr uintptr_t typeId = 0x38;
                }
                namespace Property {
                    constexpr uintptr_t Name = 0x8;
                    constexpr uintptr_t Capabilities = 0x48;
                }
            }
        }

        namespace BasePart {
            constexpr uintptr_t Primitive = 0x170;
            namespace PrimitiveStructure {
                constexpr uintptr_t Overlap = 0x1D0;
            }
        }

        namespace LocalScript {
            constexpr uintptr_t ProtectedBytecode = 0x1B0;
            constexpr uintptr_t ThreadNodeList = 0x188;
            namespace ThreadNode {
                constexpr uintptr_t next = 0x18;
                constexpr uintptr_t liveLuaRef = 0x20;
            }
        }

        namespace ModuleScript {
            constexpr uintptr_t ProtectedBytecode = 0x158;
            constexpr uintptr_t VmStateMap = 0x1A8;
            namespace VmState {
                constexpr uintptr_t LoadedStatus = 0x18;
                constexpr uintptr_t LuaState = 0x28;
            }
        }

        namespace Signal {
            constexpr uintptr_t next = 0x10;
            constexpr uintptr_t enabled = 0x20;
            constexpr uintptr_t signalSlot = 0x30;
            constexpr uintptr_t signalSlotWrapper = 0x38;
            namespace SignalSlotWrapper {
                constexpr uintptr_t Ptr = 0x10;
                constexpr uintptr_t secondPtr = 0x18;
                constexpr uintptr_t thirdPtr = 0x38;
            }
            namespace SignalSlot {
                constexpr uintptr_t isOnce = 0xB5;
                constexpr uintptr_t SignalRefs = 0x70;
                namespace Refs {
                    constexpr uintptr_t luaThread = 0x8;
                    constexpr uintptr_t luaThreadRefId = 0x10;
                    constexpr uintptr_t functionRefId = 0x14;
                }
            }
            namespace WaitSlot {
                constexpr uintptr_t checkPointer = 0x10;
                constexpr uintptr_t connectionRefs = 0x38;
                constexpr uintptr_t connectionRefs2 = 0x40;
                constexpr uintptr_t connectionRefs3 = 0x50;
                constexpr uintptr_t threadRefContainer = 0x18;
                constexpr uintptr_t threadRefContainer2 = 0x8;
                constexpr uintptr_t threadRefContainer3 = 0x10;
                constexpr uintptr_t threadRef = 0x20;
            }
        }
    }

    namespace Offsets {
        namespace Functions {
            // Roblox Functions
            constexpr uintptr_t printf = 0x16D2D00;
            constexpr uintptr_t getGlobalState = 0xF40490;
            constexpr uintptr_t decryptGlobalState = 0xCCA300;
            constexpr uintptr_t task_defer = 0x1172FB0;
            constexpr uintptr_t task_spawn = 0x11732A0;
            constexpr uintptr_t RBX_ScriptContext_resume = 0xF50980;
            constexpr uintptr_t makePartsTouch = 0x15BCDB0;
            constexpr uintptr_t pushInstance = 0x101AD00;
            constexpr uintptr_t fireProximityPrompt = 0x1EBD9A0;
            constexpr uintptr_t setFFlag = 0x38ADB00;
            constexpr uintptr_t getFFlag = 0x38AE6B0;
            constexpr uintptr_t getModuleVmState = 0x10A1DD0;
            constexpr uintptr_t getPropertyFromName = 0xBD6BB0;
            constexpr uintptr_t disconnectConnection = 0x388BDE0;

            constexpr uintptr_t fireClickDetector_MouseClick = 0x1DED7B0;
            constexpr uintptr_t fireClickDetector_HoverEnter = 0x1DEED50;
            constexpr uintptr_t fireClickDetector_HoverLeave = 0x1DEEEF0;
            constexpr uintptr_t fireClickDetector_RightMouseClick = 0x1DED950;

            constexpr uintptr_t getIdentityStruct = 0x38B5C70;

            // Luau Functions
            constexpr uintptr_t luau_execute = 0x2836FE0;
            constexpr uintptr_t luavm_load = 0xCCCFB0;
        }

        namespace Pointers {
            // Hyperion Pointers
            constexpr uintptr_t cfgBitmapPointer = 0x29AA80;

            constexpr uintptr_t opcodeLookupTable = 0x574C560;
            constexpr uintptr_t kTable = 0x650A350;
            constexpr uintptr_t taskScheduler = 0x69EA688;
            constexpr uintptr_t appStructurePointer = 0x692B4A8;
            constexpr uintptr_t datamodelPointer = 0x692B790;
            constexpr uintptr_t fflogDataBank = 0x659DB28;
            constexpr uintptr_t identityPointer = 0x6568838;

            // Luau Pointers
            constexpr uintptr_t dummyNode = 0x47EFC28;
            constexpr uintptr_t nilObject = 0x47F04F8;
        }
    }

    namespace FunctionDefinitions {
        enum PrintfType : uint8_t {
            Print = 0,
            Info,
            Warning,
            Error
        };

        using getModuleVmState = uintptr_t(__fastcall*)(uintptr_t* vmStateMap, uintptr_t* receivedVmState, lua_State** mainThread);
        using getIdentityStruct = void*(__fastcall*)(uintptr_t identityData);
        using fireClickDetector = void(__fastcall*)(uintptr_t clickDetector, float Distance, uintptr_t player);
        using fireClickDetector_Hover = void(__fastcall*)(uintptr_t clickDetector, uintptr_t player);
        using getFFlag = int(__fastcall*)(uintptr_t fflogDataBank, std::string* name, std::string* theReturn, bool checkUnknowns);
        using setFFlag = int(__fastcall*)(uintptr_t fflogDataBank, std::string* name, std::string* value, int varType, int zero, int zero2);
        using fireProximityPrompt = void(__fastcall*)(uintptr_t proximityPrompt);
        using makePartsTouch = void(__fastcall*)(uintptr_t overlap, uintptr_t primitive1, uintptr_t primitive2, bool toggle, bool alwaysTrue);
        using pushInstance = void(__fastcall*)(lua_State* L, void** instance);
        using RBX_ScriptContext_resume = void(__fastcall*)(void *scriptContext, int64_t unk[0x2],
                                                           WeakThreadRef **weakThreadRef, int32_t nRet, bool isError,
                                                           char const *errorMessage);
        using printf_t = void(__cdecl*)(PrintfType type, const char *message, ...);
        using task_defer = int32_t(__cdecl*)(lua_State *state);
        using task_spawn = int32_t(__cdecl*)(lua_State *state);
        using getGlobalState = uintptr_t(__fastcall*)(uintptr_t scriptContext, void *something, void *something2);
        using decryptGlobalState = uintptr_t(__fastcall*)(uintptr_t encryptedGlobalState);
        using luau_execute = void(__fastcall*)(lua_State *L);
        using luavm_load = int32_t(__fastcall*)(lua_State* state, std::string* bytecode, const char* chunk_name, int32_t env);
        using getPropertyFromName = uintptr_t(__fastcall*)(uintptr_t descriptor, uintptr_t* propertyName);
        using disconnectConnection = void(__fastcall*)(uintptr_t* connection);
    }
}

DefinePointer(TaskSchedulerPointer, taskScheduler);
DefinePointer(AppStructurePointer, appStructurePointer);
DefinePointer(DatamodelPointer, datamodelPointer);
DefinePointer(DummyNode, dummyNode);
DefinePointer(NilObject, nilObject);
DefinePointer(FFlogDataBank, fflogDataBank);
DefinePointer(IdentityPointer, identityPointer);
DefinePointer(OpcodeLookupTable, opcodeLookupTable);
DefinePointer(KTable, kTable);

DefineHyperionPointer(CfgBitmapPointer, cfgBitmapPointer);

DefineFunction(Printf, printf, printf_t);
DefineFunction(GetGlobalState, getGlobalState, getGlobalState);
DefineFunction(DecryptGlobalState, decryptGlobalState, decryptGlobalState);
DefineFunction(Luau_Execute, luau_execute, luau_execute);
DefineFunction(LuaVM_Load, luavm_load, luavm_load);
DefineFunction(Task_Defer, task_defer, task_defer);
DefineFunction(Task_Spawn, task_spawn, task_spawn);
DefineFunction(resumeThread, RBX_ScriptContext_resume, RBX_ScriptContext_resume);
DefineFunction(MakePartsTouch, makePartsTouch, makePartsTouch);
DefineFunction(PushInstance, pushInstance, pushInstance);
DefineFunction(FireProximityPrompt, fireProximityPrompt, fireProximityPrompt);
DefineFunction(SetFFlag, setFFlag, setFFlag);
DefineFunction(GetFFlag, getFFlag, getFFlag);
DefineFunction(FireClickDetector_MouseClick, fireClickDetector_MouseClick, fireClickDetector);
DefineFunction(FireClickDetector_HoverEnter, fireClickDetector_HoverEnter, fireClickDetector_Hover);
DefineFunction(FireClickDetector_HoverLeave, fireClickDetector_HoverLeave, fireClickDetector_Hover);
DefineFunction(FireClickDetector_RightMouseClick, fireClickDetector_RightMouseClick, fireClickDetector);
DefineFunction(GetIdentityStructure, getIdentityStruct, getIdentityStruct);
DefineFunction(GetModuleVmState, getModuleVmState, getModuleVmState);
DefineFunction(GetPropertyFromName, getPropertyFromName, getPropertyFromName);
DefineFunction(DisconnectConnection, disconnectConnection, disconnectConnection);