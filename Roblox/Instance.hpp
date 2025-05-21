//
// Created by Pixeluted on 17/03/2025.
//
#pragma once
#include <string>
#include <unordered_map>

std::unordered_map<std::string, uintptr_t> GetInstanceProperties(uintptr_t rawInstance);
int GetPropertyCapabilities(uintptr_t property);
void SetPropertyCapabilities(uintptr_t property, int newCapabilities);

#define IS_SCRIPTABLE_PROPERTY(property) GetPropertyCapabilities(property) >> 5 & 1
