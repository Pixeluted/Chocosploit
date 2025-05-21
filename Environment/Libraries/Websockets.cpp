//
// Created by Pixeluted on 15/03/2025.
//

#include "Websockets.hpp"

#include <regex>

#include "ldebug.h"
#include "../../Execution/InternalTaskScheduler.hpp"
#include "../ExploitObjects/Websocket.hpp"

bool isValidWebSocketURL(const std::string& url) {
    const std::regex websocket_regex(
        R"(^(ws|wss)://([a-zA-Z0-9.-]+(:[0-9]{1,5})?)(/[a-zA-Z0-9/.-]*)?$)");
    return std::regex_match(url, websocket_regex);
}

int connect(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const std::string targetUrl = lua_tostring(L, 1);
    if (targetUrl.find("ws://") == std::string::npos && targetUrl.find("wss://") == std::string::npos) {
        luaG_runerror(L, "Invalid protocol (expected 'ws://' or 'wss://')");
    }
    if (!isValidWebSocketURL(targetUrl))
        luaG_runerrorL(L, "Invalid URL");
    
    const auto websocketInstance = CreateNewWebsocket(L, targetUrl);
    ApplicationContext::GetService<InternalTaskScheduler>()->ScheduleYield(
        L, [websocketInstance](const std::shared_ptr<YieldingRequest> &yieldingRequest) {
            websocketInstance->websocketObject->start();
            const auto startedAt = std::time(nullptr);
            while (websocketInstance->websocketObject->getReadyState() != ix::ReadyState::Open && websocketInstance->
                   lastWebsocketErrorMessage.empty() && (std::time(nullptr) - startedAt) < 5) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (websocketInstance->websocketObject->getReadyState() != ix::ReadyState::Open) {
                CloseWebsocketInstance(websocketInstance, false);
            }

            yieldingRequest->completionCallback = [websocketInstance](lua_State *L) -> YieldResult {
                if (websocketInstance->websocketObject->getReadyState() != ix::ReadyState::Open) {
                    if (websocketInstance->lastWebsocketErrorMessage.empty()) {
                        return {false, 0, "Connection timed out."};
                    }

                    return {false, 0, websocketInstance->lastWebsocketErrorMessage.c_str()};
                }

                websocketInstance->BuildLuaObject(L, websocketInstance);
                return {true, 1};
            };
            yieldingRequest->isReady = true;
        });

    return lua_yield(L, 1);
}

std::vector<RegistrationType> Websockets::GetRegistrationTypes() {
    return {
        RegisterIntoTable{"WebSocket"}
    };
}

luaL_Reg *Websockets::GetFunctions() {
     const auto Websockets_functions = new luaL_Reg[] {
         {"connect", connect},
         {nullptr, nullptr}
     };

     return Websockets_functions;
}
