//
// Created by Pixeluted on 25/03/2025.
//
#pragma once
#include <memory>
#include <unordered_map>

class Service {
public:
    bool isInitialized;
    virtual ~Service() = default;

    Service() {
        isInitialized = true;
    };

    [[nodiscard]] virtual bool IsInitialized() const {
        return isInitialized;
    }
};

class ApplicationContext final {
    static std::unordered_map<size_t, std::shared_ptr<Service>> allServices;
public:
    template<typename T>
    requires std::is_base_of_v<Service, T>
    static std::shared_ptr<T> AddService() {
        const auto createdService = std::make_shared<T>();
        if (!createdService->IsInitialized())
            return nullptr;
        allServices[typeid(T).hash_code()] = createdService;
        return createdService;
    };

    template<typename T>
    requires std::is_base_of_v<Service, T>
    static std::shared_ptr<T> GetService() {
        if (!allServices.contains(typeid(T).hash_code()))
            return nullptr;

        return std::dynamic_pointer_cast<T>(allServices.at(typeid(T).hash_code()));
    }
};
