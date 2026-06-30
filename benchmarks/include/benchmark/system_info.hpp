#pragma once

#include <string>

#include "benchmark/types.hpp"

namespace benchmark {

json CollectEnvironmentInfo(uint64_t dataset_size, uint64_t num_documents,
                            uint64_t index_size_bytes);

std::string GetCpuModel();
uint32_t GetPhysicalCores();
uint32_t GetLogicalCores();
uint64_t GetTotalRamBytes();
std::string GetOsInfo();
std::string GetCompilerInfo();
std::string GetCompilerFlags();
std::string GetStlImplementation();
std::string GetBuildType();
std::string GetStorageDevice(const std::string& path);
std::string GetTimestamp();
std::string GetCommitHash();

uint64_t GetCurrentRssKb();
uint64_t GetPeakRssKb();

}  // namespace benchmark
