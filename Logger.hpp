//
// Created by Pixeluted on 03/02/2025.
//
#pragma once
#include <string>

#include "ApplicationContext.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
enum LogType : uint8_t {
    Info,
    Warn,
    Error
};

class Logger final : public Service {
    std::ofstream logFile;
public:
    Logger();

    void Log(LogType logType, const std::string& logMessage) const;
    void WriteToLog(LogType logType, const std::string& logMessage);
};
