//
// Created by Pixeluted on 17/02/2025.
//
#pragma once
#include "../EnvironmentManager.hpp"

class Metatable final : public Library {
    std::vector<RegistrationType> GetRegistrationTypes() override;
    luaL_Reg *GetFunctions() override;
};