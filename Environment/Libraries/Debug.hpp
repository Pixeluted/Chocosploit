//
// Created by Pixeluted on 18/02/2025.
//
#pragma once
#include "../EnvironmentManager.hpp"

class Debug final : public Library {
    std::vector<RegistrationType> GetRegistrationTypes() override;
    luaL_Reg *GetFunctions() override;
};
