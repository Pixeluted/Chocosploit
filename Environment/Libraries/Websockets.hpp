//
// Created by Pixeluted on 15/03/2025.
//
#pragma once
#include "../EnvironmentManager.hpp"

class Websockets final : public Library {
    std::vector<RegistrationType> GetRegistrationTypes() override;
    luaL_Reg *GetFunctions() override;
};
