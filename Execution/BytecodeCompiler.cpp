//
// Created by Pixeluted on 05/02/2025.
//

#include "BytecodeCompiler.hpp"

#include <chrono>
#include <stdexcept>

#include "Luau/BytecodeUtils.h"
#include "Luau/Compiler.h"
#include "zstd.h"
#include "xxhash.h"
#include "../ApplicationContext.hpp"
#include "../Logger.hpp"
#include "../OffsetsAndFunctions.hpp"

void PixelSploitV2BytecodeEncoder::encode(uint32_t *data, size_t count) {
    for (auto i = 0u; i < count;) {
        uint8_t op = LUAU_INSN_OP(data[i]);
        const auto opLength = Luau::getOpLength(static_cast<LuauOpcode>(op));
        const auto lookupTable = reinterpret_cast<BYTE *>(OpcodeLookupTable);
        uint8_t newOp = op * 227;
        newOp = lookupTable[newOp];
        data[i] = (newOp) | (data[i] & ~0xff);
        i += opLength;
    }
}

static auto BytecodeEncoder = PixelSploitV2BytecodeEncoder();

std::string CompileBytecode(const std::string &source) {
    Luau::CompileOptions options;
    options.debugLevel = 1;
    options.optimizationLevel = 1;
    const char *mutableGlobals[] = {
        "Game", "Workspace", "game", "plugin", "script", "shared", "workspace",
        "_G", "_ENV", nullptr
    };
    options.mutableGlobals = mutableGlobals;
    options.vectorLib = "Vector3";
    options.vectorCtor = "new";
    options.vectorType = "Vector3";

    return Luau::compile(source, options, {}, &BytecodeEncoder);
}

std::optional<std::string> DecompressBytecode(std::string bytecode) {
    auto kBytecodeMagic = "RSB1";
    static constexpr auto kBytecodeHashMultiplier = 41u;

    unsigned char HashBytes[4];
    memcpy(HashBytes, bytecode.data(), 4);

    for (size_t i = 0; i < 4; i++) {
        HashBytes[i] ^= kBytecodeMagic[i];
        HashBytes[i] -= i * kBytecodeHashMultiplier;
    }

    for (size_t i = 0; i < bytecode.size(); i++) {
        bytecode[i] ^= HashBytes[i % 4] + i * kBytecodeHashMultiplier;
    }

    unsigned int hash;
    memcpy(&hash, HashBytes, 4);

    int decompressedSize;
    memcpy(&decompressedSize, &bytecode[4], 4);

    std::vector<char> decompressedBytecode(decompressedSize);

    const auto decompressResults = ZSTD_decompress(decompressedBytecode.data(), decompressedSize, bytecode.data() + 8,
                                                   bytecode.size() - 8);
    if (ZSTD_isError(decompressResults))
        return std::nullopt;

    return std::string(decompressedBytecode.begin(), decompressedBytecode.end());
}
