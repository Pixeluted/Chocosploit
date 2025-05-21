//
// Created by Pixeluted on 17/03/2025.
//

#include "Instance.hpp"

#include "../OffsetsAndFunctions.hpp"

std::unordered_map<std::string, uintptr_t> GetInstanceProperties(const uintptr_t rawInstance) {
    auto foundProperties = std::unordered_map<std::string, uintptr_t>();

    const auto classDescriptor = *reinterpret_cast<uintptr_t *>(
        rawInstance + ChocoSploit::StructOffsets::Instance::InstanceClassDescriptor);
    const auto allPropertiesStart = *reinterpret_cast<uintptr_t *>(
        classDescriptor + ChocoSploit::StructOffsets::Instance::ClassDescriptor::PropertiesStart);
    const auto allPropertiesEnd = *reinterpret_cast<uintptr_t *>(
        classDescriptor + ChocoSploit::StructOffsets::Instance::ClassDescriptor::PropertiesEnd);

    for (uintptr_t currentPropertyAddress = allPropertiesStart; currentPropertyAddress != allPropertiesEnd;
         currentPropertyAddress += 0x8) {
        const auto currentProperty = *reinterpret_cast<uintptr_t *>(currentPropertyAddress);
        const auto propertyNameAddress = *reinterpret_cast<uintptr_t *>(
            currentProperty + ChocoSploit::StructOffsets::Instance::ClassDescriptor::Property::Name);
        if (propertyNameAddress == 0)
            continue;
        const auto propertyName = *reinterpret_cast<std::string *>(propertyNameAddress);
        foundProperties[propertyName] = currentProperty;
    }

    return foundProperties;
}

int GetPropertyCapabilities(const uintptr_t property) {
    return *reinterpret_cast<int *>(
        property + ChocoSploit::StructOffsets::Instance::ClassDescriptor::Property::Capabilities);
}

void SetPropertyCapabilities(const uintptr_t property, const int newCapabilities) {
    *reinterpret_cast<int *>(
                property + ChocoSploit::StructOffsets::Instance::ClassDescriptor::Property::Capabilities) =
            newCapabilities;
}
