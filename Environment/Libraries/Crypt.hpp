//
// Created by Pixeluted on 19/02/2025.
//
#pragma once
#include "../EnvironmentManager.hpp"

class Crypt final : public Library {
    std::vector<RegistrationType> GetRegistrationTypes() override;
    luaL_Reg *GetFunctions() override;
};
