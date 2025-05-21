//
// Created by Pixeluted on 17/02/2025.
//

#include "Filesystem.hpp"

#include "ldebug.h"
#include "../../Managers/FileSystemManager.hpp"
#include "../../Execution/InternalTaskScheduler.hpp"

int isfile(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const auto fileSystemManager = ApplicationContext::GetService<FileSystemManager>();
    fileSystemManager->IsFileSystemEnabled(L);

    const auto finalPath = fileSystemManager->IsPathSafe(L, lua_tostring(L, 1));

    lua_pushboolean(L, exists(finalPath) && is_regular_file(finalPath));
    return 1;
}

int isfolder(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const auto fileSystemManager = ApplicationContext::GetService<FileSystemManager>();
    fileSystemManager->IsFileSystemEnabled(L);

    const auto finalPath = fileSystemManager->IsPathSafe(L, lua_tostring(L, 1));

    lua_pushboolean(L, exists(finalPath) && is_directory(finalPath));
    return 1;
}

int listfiles(lua_State* L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const auto fileSystemManager = ApplicationContext::GetService<FileSystemManager>();
    fileSystemManager->IsFileSystemEnabled(L);

    const auto finalPath = fileSystemManager->IsPathSafe(L, lua_tostring(L, 1));
    if (!is_directory(finalPath))
        luaG_runerrorL(L, "This directory doesn't exist!");

    lua_newtable(L);
    int currentIndex = 1;
    for (const auto &entry: fs::directory_iterator(finalPath)) {
        lua_pushstring(L, entry.path().filename().string().c_str());
        lua_rawseti(L, -2, currentIndex);
        currentIndex++;
    }

    return 1;
}

int readfile(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const auto fileSystemManager = ApplicationContext::GetService<FileSystemManager>();
    fileSystemManager->IsFileSystemEnabled(L);

    const auto finalPath = fileSystemManager->IsPathSafe(L, lua_tostring(L, 1));
    auto readContentVar = fileSystemManager->ReadFile(finalPath);
    if (std::holds_alternative<const char *>(readContentVar))
        luaG_runerrorL(L, std::get<const char *>(readContentVar));
    auto& readContent = std::get<std::string>(readContentVar);

    lua_pushlstring(L, readContent.c_str(), readContent.size());
    return 1;
}

int writefile(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TSTRING);

    const auto fileSystemManager = ApplicationContext::GetService<FileSystemManager>();
    fileSystemManager->IsFileSystemEnabled(L);

    const auto finalPath = fileSystemManager->IsPathSafe(L, lua_tostring(L, 1));
    size_t fileContentSize;
    const auto fileContentString = lua_tolstring(L, 2, &fileContentSize);
    const auto fileContent = std::string(fileContentString, fileContentSize);

    const auto writeResults = fileSystemManager->WriteFile(finalPath, fileContent);
    if (std::holds_alternative<const char *>(writeResults))
        luaG_runerrorL(L, std::get<const char *>(writeResults));

    return 0;
}

int makefolder(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const auto fileSystemManager = ApplicationContext::GetService<FileSystemManager>();
    fileSystemManager->IsFileSystemEnabled(L);

    const auto finalPath = fileSystemManager->IsPathSafe(L, lua_tostring(L, 1));
    if (is_directory(finalPath))
        return 0;

    if (exists(finalPath))
        luaG_runerrorL(L, "Something already exists on this path");

    create_directories(finalPath);
    return 0;
}

int delfile(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const auto fileSystemManager = ApplicationContext::GetService<FileSystemManager>();
    fileSystemManager->IsFileSystemEnabled(L);

    const auto finalPath = fileSystemManager->IsPathSafe(L, lua_tostring(L, 1));
    if (!is_regular_file(finalPath))
        luaG_runerrorL(L, "This file doesn't exist");

    fs::remove(finalPath);
    return 0;
}

int delfolder(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const auto fileSystemManager = ApplicationContext::GetService<FileSystemManager>();
    fileSystemManager->IsFileSystemEnabled(L);

    const auto finalPath = fileSystemManager->IsPathSafe(L, lua_tostring(L, 1));
    if (!is_directory(finalPath))
        luaG_runerrorL(L, "This folder doesn't exist");

    remove_all(finalPath);
    return 0;
}

int dofile(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const auto fileSystemManager = ApplicationContext::GetService<FileSystemManager>();
    fileSystemManager->IsFileSystemEnabled(L);

    const auto finalPath = fileSystemManager->IsPathSafe(L, lua_tostring(L, 1));
    if (!is_regular_file(finalPath))
        luaG_runerrorL(L, "This file doesn't exist");

    auto readFileVar = fileSystemManager->ReadFile(finalPath);
    if (std::holds_alternative<const char *>(readFileVar))
        luaG_runerrorL(L, std::get<const char *>(readFileVar));
    auto& readContent = std::get<std::string>(readFileVar);

    ApplicationContext::GetService<InternalTaskScheduler>()->ScheduleExecution(readContent);
    return 0;
}

int loadfile(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const auto fileSystemManager = ApplicationContext::GetService<FileSystemManager>();
    fileSystemManager->IsFileSystemEnabled(L);

    const auto finalPath = fileSystemManager->IsPathSafe(L, lua_tostring(L, 1));
    const auto chunkName = luaL_optstring(L, 2, EXECUTOR_CHUNK_NAME);
    if (!is_regular_file(finalPath))
        luaG_runerrorL(L, "This file doesn't exist");

    auto readContentVar = fileSystemManager->ReadFile(finalPath);
    if (std::holds_alternative<const char *>(readContentVar))
        luaG_runerrorL(L, std::get<const char *>(readContentVar));
    auto& readContent = std::get<std::string>(readContentVar);

    lua_getglobal(L, "loadstring");
    lua_pushstring(L, readContent.c_str());
    lua_pushstring(L, chunkName);
    if (lua_pcall(L, 2, 2, 0) != LUA_OK)
        return 2;

    lua_pop(L, 1);
    return 1;
}

int appendfile(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TSTRING);
    const auto fileSystemManager = ApplicationContext::GetService<FileSystemManager>();
    fileSystemManager->IsFileSystemEnabled(L);

    const auto finalPath = fileSystemManager->IsPathSafe(L, lua_tostring(L, 1));
    if (!is_regular_file(finalPath))
        luaG_runerrorL(L, "This file doesn't exist");
    const auto contentToAppend = lua_tostring(L, 2);

    auto readContentVar = fileSystemManager->ReadFile(finalPath);
    if (std::holds_alternative<const char *>(readContentVar))
        luaG_runerrorL(L, std::get<const char *>(readContentVar));
    auto& readContent = std::get<std::string>(readContentVar);
    readContent.append(contentToAppend);

    const auto writeContentVar = fileSystemManager->WriteFile(finalPath, readContent);
    if (std::holds_alternative<const char *>(writeContentVar))
        luaG_runerrorL(L, std::get<const char *>(writeContentVar));

    return 0;
}

int getcustomasset(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const auto fileSystemManager = ApplicationContext::GetService<FileSystemManager>();
    fileSystemManager->IsFileSystemEnabled(L);

    const auto finalPath = fileSystemManager->IsPathSafe(L, lua_tostring(L, 1));
    if (!exists(finalPath) || !is_regular_file(finalPath))
        luaL_argerrorL(L, 1, "Couldn't find the asset");

    const auto assetFolder = fs::current_path() / "ExtraContent" / "ChocoSploit";
    if (!exists(assetFolder)) {
        fs::create_directory(assetFolder);
    }
    const auto finalAssetPath = assetFolder / finalPath.filename();
    fs::copy_file(finalPath, finalAssetPath, fs::copy_options::update_existing);

    lua_pushstring(L, std::format("rbxasset://ChocoSploit/{}", finalPath.filename().string()).c_str());
    return 1;
}

std::vector<RegistrationType> Filesystem::GetRegistrationTypes() {
    return {
        RegisterIntoGlobals{}
    };
}

luaL_Reg *Filesystem::GetFunctions() {
     const auto Filesystem_functions = new luaL_Reg[] {
         {"isfile", isfile},
         {"isfolder", isfolder},
         {"listfiles", listfiles},
         {"readfile", readfile},
         {"writefile", writefile},
         {"makefolder", makefolder},
         {"delfile", delfile},
         {"delfolder", delfolder},
         {"dofile", dofile},
         {"loadfile", loadfile},
         {"appendfile", appendfile},
         {"getcustomasset", getcustomasset},
         {nullptr, nullptr}
     };

     return Filesystem_functions;
}
