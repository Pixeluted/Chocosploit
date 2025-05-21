//
// Created by Pixeluted on 03/05/2025.
//
#pragma once
#include "../EnvironmentManager.hpp"

class Drawing final : public Library {
    std::vector<RegistrationType> GetRegistrationTypes() override;
    luaL_Reg *GetFunctions() override;
};