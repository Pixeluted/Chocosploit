//
// Created by Pixeluted on 11/02/2025.
//

#define _WINSOCKAPI_
#include <WinSock2.h>

#include "Communication.hpp"
#include "Logger.hpp"
#include "WinReg.hpp"
#include "IXWebsocket/ixwebsocket/IXWebSocket.h"
#include "Execution/InternalTaskScheduler.hpp"

void Communication::HandleMessage(const std::string &message) {
    ApplicationContext::GetService<InternalTaskScheduler>()->ScheduleExecution(message);
}

Communication::Communication() {
    websocketInstance = std::make_unique<ix::WebSocket>();
    websocketInstance->setUrl(WEBSOCKET_URL);
    websocketInstance->setPingInterval(1);

    std::thread(&Communication::StartListening, this).detach();
    isInitialized = true;
}

bool Communication::IsConnected() const {
    if (websocketInstance != nullptr)
        return websocketInstance->getReadyState() == ix::ReadyState::Open;

    return false;
}

void Communication::StartListening() {
    const auto logger = ApplicationContext::GetService<Logger>();
    websocketInstance->setOnMessageCallback([this, &logger](const ix::WebSocketMessagePtr &msg) {
        switch (msg->type) {
            case ix::WebSocketMessageType::Message:
                HandleMessage(msg->str);
                break;
            case ix::WebSocketMessageType::Open:
                logger->Log(Info, "Now listening for messages!");
                break;
            case ix::WebSocketMessageType::Close:
                if (isReconnecting)
                    break;

                logger->Log(Info, "Websocket was closed! Reconnecting in 1 second!");
                std::thread([this]() {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    this->StartListening();
                }).detach();
                break;
            default:
                break;
        }
    });

    isReconnecting = true;
    while (true) {
        websocketInstance->start();

        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (this->IsConnected()) {
            break;
        }

        websocketInstance->stop();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    isReconnecting = false;
}

void Communication::SendText(const std::string &text) {
    if (websocketInstance == nullptr || !IsConnected())
        return;

    std::lock_guard lock(wsMutex);
    websocketInstance->send(text);
}

Communication::~Communication() {
    if (websocketInstance != nullptr) {
        websocketInstance->stop();
    }
}

std::optional<std::filesystem::path> Communication::ObtainBaseUIPath() {
    winreg::RegKey exploitSettings;
    if (exploitSettings.TryOpen(HKEY_CURRENT_USER, L"Software\\Chocosploit", KEY_READ).Failed()) {
        return std::nullopt;
    }

    const auto UIFolderResults = exploitSettings.TryGetStringValue(L"UIFolder");
    if (!UIFolderResults.IsValid()) {
        return std::nullopt;
    }

    return std::filesystem::path(UIFolderResults.GetValue());
}