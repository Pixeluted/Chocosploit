//
// Created by Pixeluted on 17/02/2025.
//

#include "FileSystemManager.hpp"

#include <fstream>
#include <unordered_set>

#include "ldebug.h"
#include "../Communication.hpp"
#include "../Logger.hpp"

static std::unordered_set<std::string> blacklistedExtensions = {
    ".wsf", ".exe", ".dll", ".bat", ".cmd", ".scr", ".vbs", ".js",
    ".ts", ".wsf", ".msi", ".com", ".lnk", ".ps1", ".py",
    ".py3", ".pyc", ".pyw", ".scr", ".msi", ".html"
};

FileSystemManager::FileSystemManager() {
    const auto logger = ApplicationContext::GetService<Logger>();
    const auto basePathOpt = Communication::ObtainBaseUIPath();
    if (!basePathOpt.has_value()) {
        logger->Log(Error, "Failed to obtain filesystem settings!");
        areFSFunctionsEnabled = false;
        isInitialized = true;
        return;
    }
    const auto& basePath = basePathOpt.value();

    workspaceDirectory = basePath / "workspace";
    autoexecDirectory = basePath / "autoexec";
    if (!exists(workspaceDirectory)) {
        if (!create_directory(workspaceDirectory)) {
            logger->Log(Error, "Failed to create workspace directory!");
            areFSFunctionsEnabled = false;
            isInitialized = true;
            return;
        }
    }

    if (!is_directory(workspaceDirectory)) {
        logger->Log(Error, "Workspace directory isn't directory...");
        areFSFunctionsEnabled = false;
        isInitialized = true;
        return;
    }

    if (!exists(autoexecDirectory)) {
        if (!create_directory(autoexecDirectory)) {
            logger->Log(Error, "Failed to create autoexec directory!");
        }
    }

    if (!is_directory(autoexecDirectory)) {
        logger->Log(Error, "Autoexec directory isn't directory...");
    }

    areFSFunctionsEnabled = true;
    isInitialized = true;
}

void FileSystemManager::IsFileSystemEnabled(lua_State *L) const {
    if (!areFSFunctionsEnabled)
        luaG_runerrorL(L, "FileSystem functions are disabled.");
}

fs::path FileSystemManager::IsPathSafe(lua_State *L, const std::string_view &relativePath) const {
    const auto resolveResults = this->ResolvePath(relativePath);
    if (!resolveResults.has_value())
        luaG_runerrorL(L, "This path is not valid!");

    return resolveResults.value();
}

std::optional<fs::path> FileSystemManager::ResolvePath(const std::string_view &relativePath) const {
    const auto workspaceDirAbsolute = absolute(workspaceDirectory);
    auto absolutePath = workspaceDirAbsolute / relativePath;

    const auto normalizedBase = workspaceDirAbsolute.lexically_normal();
    const auto normalizedCombined = absolutePath.lexically_normal();

    const auto baseString = normalizedBase.string();
    const auto combinedString = normalizedCombined.string();

    if (combinedString.compare(0, baseString.length(), baseString) != 0)
        return std::nullopt;

    return absolutePath;
}


std::variant<std::string, const char *> FileSystemManager::ReadFile(const fs::path &filePath) const {
    if (!areFSFunctionsEnabled)
        throw std::runtime_error("Tried to read file while FS functions are disabled!");

    if (!exists(filePath) || !is_regular_file(filePath))
        return "File doesn't exist";

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return "Couldn't open the file";

    const auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string fileBuffer = "";
    fileBuffer.resize(fileSize);
    if (!file.read(fileBuffer.data(), fileSize))
        return "Couldn't read the file";

    return fileBuffer;
}

std::variant<bool, const char *> FileSystemManager::WriteFile(const fs::path &filePath,
                                                              const std::string_view &fileContent) const {
    if (!areFSFunctionsEnabled)
        throw std::runtime_error("Tried to write file while FS functions are disabled!");

    if (!filePath.has_extension())
        return "File needs to have a extension";

    if (is_directory(filePath))
        return "Cannot write to a folder";

    auto fileExtension = filePath.extension().string();
    std::ranges::transform(fileExtension, fileExtension.begin(), tolower);
    if (blacklistedExtensions.contains(fileExtension))
        return "Disallowed file extension";

    create_directories(filePath.parent_path());

    std::ofstream newFile(filePath, std::ios::binary);
    if (!newFile.is_open())
        return "Failed to open the file";

    newFile.write(fileContent.data(), fileContent.size());
    if (newFile.fail())
        return "Failed to write to the file";

    newFile.close();
    return true;
}
