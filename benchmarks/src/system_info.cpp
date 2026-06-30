#include "benchmark/system_info.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <sys/utsname.h>
#include <thread>
#include <unordered_set>

#ifndef BENCHMARK_GIT_COMMIT
#define BENCHMARK_GIT_COMMIT "unknown"
#endif

#ifndef BENCHMARK_CXX_FLAGS
#define BENCHMARK_CXX_FLAGS "unknown"
#endif

namespace benchmark {
namespace {

std::string ReadFirstLine(const std::string& path) {
  std::ifstream file(path);
  std::string line;
  if (std::getline(file, line)) {
    return line;
  }
  return "";
}

std::string TrimPrefix(const std::string& value, const std::string& prefix) {
  if (value.rfind(prefix, 0) == 0) {
    return value.substr(prefix.size());
  }
  return value;
}

uint64_t ParseProcValue(const std::string& path, const std::string& key) {
  std::ifstream file(path);
  std::string line;
  while (std::getline(file, line)) {
    if (line.rfind(key, 0) == 0) {
      std::string val = TrimPrefix(line, key);
      return std::stoull(val);
    }
  }
  return 0;
}

}  // namespace

std::string GetCpuModel() {
  std::string model = ReadFirstLine("/proc/cpuinfo");
  if (model.find("model name") != std::string::npos) {
    auto pos = model.find(':');
    if (pos != std::string::npos) {
      std::string result = model.substr(pos + 1);
      while (!result.empty() && result[0] == ' ') {
        result.erase(result.begin());
      }
      return result;
    }
  }
  std::ifstream file("/proc/cpuinfo");
  std::string line;
  while (std::getline(file, line)) {
    if (line.rfind("model name", 0) == 0) {
      auto pos = line.find(':');
      if (pos != std::string::npos) {
        return TrimPrefix(line.substr(pos + 1), " ");
      }
    }
  }
  return "unknown";
}

uint32_t GetPhysicalCores() {
  std::ifstream file("/proc/cpuinfo");
  std::string line;
  while (std::getline(file, line)) {
    if (line.rfind("cpu cores", 0) == 0) {
      auto pos = line.find(':');
      if (pos != std::string::npos) {
        uint32_t cores = static_cast<uint32_t>(
            std::stoul(TrimPrefix(line.substr(pos + 1), " ")));
        uint32_t sockets = 1;
        std::ifstream f2("/proc/cpuinfo");
        std::string l2;
        std::unordered_set<std::string> physical_ids;
        while (std::getline(f2, l2)) {
          if (l2.rfind("physical id", 0) == 0) {
            auto p = l2.find(':');
            if (p != std::string::npos) {
              physical_ids.insert(l2.substr(p + 1));
            }
          }
        }
        if (!physical_ids.empty()) {
          sockets = static_cast<uint32_t>(physical_ids.size());
        }
        return cores * sockets;
      }
    }
  }
  uint32_t logical = GetLogicalCores();
  return logical > 1 ? logical / 2 : logical;
}

uint32_t GetLogicalCores() {
  uint32_t cores = std::thread::hardware_concurrency();
  return cores > 0 ? cores : 1;
}

uint64_t GetTotalRamBytes() {
  return ParseProcValue("/proc/meminfo", "MemTotal:") * 1024;
}

std::string GetOsInfo() {
  struct utsname info {};
  if (uname(&info) == 0) {
    std::ostringstream oss;
    oss << info.sysname << " " << info.release << " (" << info.machine << ")";
    return oss.str();
  }
  return "unknown";
}

std::string GetCompilerInfo() {
#if defined(__clang__)
  return std::string("Clang ") + __clang_version__;
#elif defined(__GNUC__)
  return std::string("GCC ") + std::to_string(__GNUC__) + "." +
         std::to_string(__GNUC_MINOR__) + "." +
         std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
  return "MSVC " + std::to_string(_MSC_VER);
#else
  return "unknown";
#endif
}

std::string GetCompilerFlags() { return BENCHMARK_CXX_FLAGS; }

std::string GetStlImplementation() {
#if defined(_LIBCPP_VERSION)
  return "libc++ " + std::to_string(_LIBCPP_VERSION);
#elif defined(__GLIBCXX__)
  return "libstdc++ " + std::to_string(__GLIBCXX__);
#elif defined(_MSVC_STL_VERSION)
  return "MSVC STL " + std::to_string(_MSVC_STL_VERSION);
#else
  return "unknown";
#endif
}

std::string GetBuildType() {
#if defined(NDEBUG)
  return "Release";
#else
  return "Debug";
#endif
}

std::string GetStorageDevice(const std::string& path) {
  std::error_code ec;
  auto abs = std::filesystem::absolute(path, ec);
  std::string abs_path = ec ? path : abs.string();
  if (abs_path.empty()) {
    abs_path = ".";
  }
  std::ifstream mounts("/proc/mounts");
  std::string line;
  std::string best_match;
  while (std::getline(mounts, line)) {
    std::istringstream iss(line);
    std::string device, mount_point;
    iss >> device >> mount_point;
    if (abs_path.rfind(mount_point, 0) == 0 &&
        mount_point.size() > best_match.size()) {
      best_match = device;
    }
  }
  if (best_match.empty()) {
    return "unknown";
  }
  if (best_match.find("nvme") != std::string::npos) {
    return "NVMe (" + best_match + ")";
  }
  if (best_match.find("sd") != std::string::npos) {
    return "SSD/HDD (" + best_match + ")";
  }
  return best_match;
}

std::string GetTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm_buf{};
  localtime_r(&time, &tm_buf);
  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

std::string GetCommitHash() { return BENCHMARK_GIT_COMMIT; }

uint64_t GetCurrentRssKb() {
  return ParseProcValue("/proc/self/status", "VmRSS:");
}

uint64_t GetPeakRssKb() {
  return ParseProcValue("/proc/self/status", "VmHWM:");
}

json CollectEnvironmentInfo(uint64_t dataset_size, uint64_t num_documents,
                            uint64_t index_size_bytes) {
  return json{
      {"cpu_model", GetCpuModel()},
      {"physical_cores", GetPhysicalCores()},
      {"logical_cores", GetLogicalCores()},
      {"ram_bytes", GetTotalRamBytes()},
      {"ram_gb", static_cast<double>(GetTotalRamBytes()) / (1024.0 * 1024 * 1024)},
      {"os", GetOsInfo()},
      {"compiler", GetCompilerInfo()},
      {"compiler_flags", GetCompilerFlags()},
      {"stl", GetStlImplementation()},
      {"build_type", GetBuildType()},
      {"storage_device", GetStorageDevice(".")},
      {"dataset_size", dataset_size},
      {"num_documents", num_documents},
      {"index_size_bytes", index_size_bytes},
      {"index_size_mb",
       static_cast<double>(index_size_bytes) / (1024.0 * 1024.0)},
      {"timestamp", GetTimestamp()},
      {"commit_hash", GetCommitHash()},
  };
}

}  // namespace benchmark
