//
// Created by Pixeluted on 17/02/2025.
//
#pragma once
#include <filters.h>
#include <hex.h>
#include <sha.h>
#include <Windows.h>

#include "../EnvironmentManager.hpp"

__forceinline std::string GetHWID() {
    HW_PROFILE_INFO profileInfo;
    if (!GetCurrentHwProfileA(&profileInfo))
        return "erm";

    CryptoPP::SHA256 sha256;
    unsigned char digest[CryptoPP::SHA256::DIGESTSIZE];
    sha256.CalculateDigest(digest, reinterpret_cast<unsigned char*>(profileInfo.szHwProfileGuid), sizeof(profileInfo.szHwProfileGuid));

    CryptoPP::HexEncoder encoder;
    std::string output;
    encoder.Attach(new CryptoPP::StringSink(output));
    encoder.Put(digest, sizeof(digest));
    encoder.MessageEnd();

    return output;
}

class Misc final : public Library {
    std::vector<RegistrationType> GetRegistrationTypes() override;
    luaL_Reg *GetFunctions() override;
};