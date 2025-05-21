//
// Created by Pixeluted on 17/02/2025.
//
#pragma once
#include <memory>
#include <filesystem>
#include <variant>

#include "lua.h"
#include "../ApplicationContext.hpp"

namespace fs = std::filesystem;
class FileSystemManager final : public Service {
public:
    fs::path workspaceDirectory;
    fs::path autoexecDirectory;
    bool areFSFunctionsEnabled;

    FileSystemManager();

    /**
     * @brief Checks if filesystem functions are enabled, if not we throw lua error
     * @param L The thread to throw the error if functions are disabled
     */
    void IsFileSystemEnabled(lua_State* L) const;

    /**
     * @brief Resolves the path and makes sure it is safe to write to. (Isn't outside workspace directory)
     * @param relativePath The relative path to check
     * @return Only returns if the path is safe, otherwise throws error.
     */
    fs::path IsPathSafe(lua_State* L, const std::string_view& relativePath) const;

    /**
     * @brief Resolves the relative path to our workspace directory, if any filesystem escalation is detected, the function will return nullopt.
     * @param relativePath The relative path
     * @return Optional of the absolute path
     */
    std::optional<fs::path> ResolvePath(const std::string_view& relativePath) const;

    std::variant<std::string, const char*> ReadFile(const fs::path &filePath) const;
    std::variant<bool, const char*> WriteFile(const fs::path &filePath, const std::string_view& fileContent) const;
};

