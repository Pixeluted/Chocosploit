//
// Created by Pixeluted on 16/02/2025.
//
#pragma once
#include "../EnvironmentManager.hpp"

class Closures final : public Library {
    std::vector<RegistrationType> GetRegistrationTypes() override;
    luaL_Reg *GetFunctions() override;
};