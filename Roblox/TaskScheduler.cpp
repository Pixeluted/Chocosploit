//
// Created by Pixeluted on 03/02/2025.
//

#include "TaskScheduler.hpp"

#include <algorithm>
#include <format>

#include "../Logger.hpp"
#include "../OffsetsAndFunctions.hpp"

TaskScheduler::TaskScheduler() {
    const auto logger = ApplicationContext::GetService<Logger>();
    const auto foundTaskSchedulerAddress = *reinterpret_cast<uintptr_t *>(TaskSchedulerPointer);
    if (foundTaskSchedulerAddress == 0x0) {
        logger->Log(Error, "Failed to find Task Scheduler!");
        return;
    }

    isInitialized = true;
    taskSchedulerAddress = foundTaskSchedulerAddress;
    logger->Log(Info, std::format("Found task scheduler at 0x{:x}", taskSchedulerAddress));
}

bool TaskScheduler::IsInGame() {
    const auto inGameValue = *reinterpret_cast<int32_t *>(
        *reinterpret_cast<uintptr_t *>(AppStructurePointer) +
        ChocoSploit::StructOffsets::TaskScheduler::AppInfo::InGame);
    return inGameValue == 4;
}

std::vector<RobloxJob> TaskScheduler::GetJobs() const {
    const auto JobStartAddress = *reinterpret_cast<uintptr_t *>(
        taskSchedulerAddress + ChocoSploit::StructOffsets::TaskScheduler::JobStart);
    const auto JobEndAddress = *reinterpret_cast<uintptr_t *>(
        taskSchedulerAddress + ChocoSploit::StructOffsets::TaskScheduler::JobEnd);
    auto allJobs = std::vector<RobloxJob>();

    for (uintptr_t jobPointer = JobStartAddress; jobPointer < JobEndAddress; jobPointer += 16) {
        const auto job = *reinterpret_cast<uintptr_t *>(jobPointer);
        const auto jobName = *reinterpret_cast<std::string *>(
            job + ChocoSploit::StructOffsets::TaskScheduler::Job::Name);
        const auto jobTypeName = reinterpret_cast<const char *>(
            job + ChocoSploit::StructOffsets::TaskScheduler::Job::TypeName);

        allJobs.emplace_back(job, jobName, jobTypeName);
    }

    return allJobs;
}

std::optional<RobloxJob> TaskScheduler::FindGameWhsj(const uintptr_t lastWhsjAddress) const {
    const auto &allJobs = this->GetJobs();
    for (const auto &job: allJobs) {
        if (strcmp(job.jobTypeName.c_str(), "WaitingHybridScriptsJob(Default;Ugc)") == 0 && job.jobAddress !=
            lastWhsjAddress) {
            return job;
        }
    }

    return std::nullopt;
}

std::optional<RobloxJob> TaskScheduler::FindRenderJob() const {
    const auto &allJobs = this->GetJobs();
    for (const auto &job: allJobs) {
        if (strcmp(job.jobName.c_str(), "RenderJob") == 0) {
            return job;
        }
    }

    return std::nullopt;
}

void TaskScheduler::PrintJobs() const {
    const auto &allJobs = this->GetJobs();

    for (const auto &job: allJobs) {
        ApplicationContext::GetService<Logger>()->Log(
            Info, std::format("Job: {} : {} at 0x{:x}", job.jobName, job.jobTypeName, job.jobAddress));
    }
}
