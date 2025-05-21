//
// Created by Pixeluted on 05/02/2025.
//
#pragma once
#include <optional>

#include "Luau/BytecodeBuilder.h"

class PixelSploitV2BytecodeEncoder final : public Luau::BytecodeEncoder {
    void encode(uint32_t *data, size_t count) override;
};

std::string CompileBytecode(const std::string& source);
std::optional<std::string> DecompressBytecode(std::string bytecode);