//
// Created by Pixeluted on 03/02/2025.
//

#include "Logger.hpp"
#include <termcolor.hpp>
#include <sstream>
#include "Communication.hpp"

Logger::Logger() {
    const auto baseUIPathOpt = Communication::ObtainBaseUIPath();
    if (!baseUIPathOpt.has_value()) {
        isInitialized = true;
        MessageBoxA(nullptr, "I hate n", ":3", MB_OK);
        return;
    }

    const auto& baseUIPath = baseUIPathOpt.value();
    const auto baseLogPath = baseUIPath / "logs";
    if (!exists(baseLogPath))
        fs::create_directory(baseLogPath);

    auto currentTime = std::time(nullptr);
    auto currentLocalTime = *std::localtime(&currentTime);

    std::ostringstream oss;
    oss << std::put_time(&currentLocalTime, "%d-%m-%Y %H-%M-%S");
    const auto currentTimestamp = oss.str();

    const auto logPath = baseLogPath / std::format("{}-log.txt", currentTimestamp);

    logFile.open(logPath, std::ios::binary);
    if (!logFile.is_open()) {
        isInitialized = true;
        MessageBoxA(nullptr, "I hate n 2", ":3", MB_OK);
        return;
    }

    isInitialized = true;
    this->WriteToLog(Info, "Initialized logging!");
}

void Logger::WriteToLog(const LogType logType, const std::string &logMessage) {
    std::ostringstream logConstructor;

    switch (logType) {
        case Info:
            logConstructor << "[INFO - ";
            break;
        case Warn:
            logConstructor << "[WARNING - ";
            break;
        case Error:
            logConstructor << "[ERROR - ";
            break;
    }

    auto currentTime = std::time(nullptr);
    auto currentLocalTime = *std::localtime(&currentTime);

    std::ostringstream oss;
    oss << std::put_time(&currentLocalTime, "%H:%M:%S");
    const auto currentTimestamp = oss.str();
    logConstructor << currentTimestamp << "] ";
    logConstructor << logMessage;

    const auto finalLog = logConstructor.str();
    logFile << finalLog << "\n";

    logFile.flush();
}

void Logger::Log(const LogType logType, const std::string &logMessage) const {
    const auto websocketInstance = ApplicationContext::GetService<Communication>();
    std::stringstream finalMessage;

    switch (logType) {
        case Info:
            finalMessage << "INFO";
            break;
        case Warn:
            finalMessage << "WARNING";
            break;
        case Error:
            finalMessage << "ERROR";
            break;
        default:
            finalMessage << "INFO";
            break;
    }

    finalMessage << "|" << logMessage;
    websocketInstance->SendText(finalMessage.str());
}
