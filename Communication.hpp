//
// Created by Pixeluted on 11/02/2025.
//
#pragma once
#include <filesystem>
#include <memory>
#include <mutex>

#include "ApplicationContext.hpp"
#include "IXWebsocket/ixwebsocket/IXWebSocket.h"

#define WEBSOCKET_URL "ws://localhost:6969"

class Communication final : public Service {
    bool isReconnecting;
    std::shared_ptr<ix::WebSocket> websocketInstance;
    std::mutex wsMutex;
public:
    static std::optional<std::filesystem::path> ObtainBaseUIPath();
    static void HandleMessage(const std::string& message);

    Communication();
    ~Communication() override;

    void StartListening();
    void SendText(const std::string& text);
    [[nodiscard]] bool IsConnected() const;
};