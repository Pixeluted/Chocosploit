//
// Created by Pixeluted on 03/02/2025.
//
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../ApplicationContext.hpp"

struct RobloxJob {
    uintptr_t jobAddress;
    std::string jobName;
    std::string jobTypeName;
};

class TaskScheduler final : public Service {
    uintptr_t taskSchedulerAddress;
public:
    TaskScheduler();

    static bool IsInGame();
    [[nodiscard]] std::vector<RobloxJob> GetJobs() const;
    [[nodiscard]] std::optional<RobloxJob> FindGameWhsj(uintptr_t lastWhsjAddress = 0) const;
    [[nodiscard]] std::optional<RobloxJob> FindRenderJob() const;
    void PrintJobs() const;
};
