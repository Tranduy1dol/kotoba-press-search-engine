#include "benchmark/resource_monitor.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>

#include "benchmark/system_info.hpp"

namespace benchmark {
namespace {

uint64_t ReadProcCounter(const std::string& path, const std::string& key) {
  std::ifstream file(path);
  std::string line;
  while (std::getline(file, line)) {
    if (line.rfind(key, 0) == 0) {
      std::string val = line.substr(key.size());
      while (!val.empty() && val[0] == ' ') {
        val.erase(val.begin());
      }
      if (val == "kB") {
        continue;
      }
      try {
        return std::stoull(val);
      } catch (...) {
        return 0;
      }
    }
  }
  return 0;
}

uint64_t ReadIoBytes(const std::string& key) {
  std::ifstream file("/proc/self/io");
  std::string line;
  while (std::getline(file, line)) {
    if (line.rfind(key, 0) == 0) {
      auto pos = line.find(':');
      if (pos != std::string::npos) {
        return std::stoull(line.substr(pos + 1));
      }
    }
  }
  return 0;
}

uint32_t CountThreads() {
  std::ifstream file("/proc/self/status");
  std::string line;
  while (std::getline(file, line)) {
    if (line.rfind("Threads:", 0) == 0) {
      return static_cast<uint32_t>(std::stoul(line.substr(8)));
    }
  }
  return 1;
}

double ReadCpuPercent(uint64_t& prev_idle, uint64_t& prev_total) {
  std::ifstream file("/proc/stat");
  std::string line;
  if (!std::getline(file, line)) {
    return 0.0;
  }
  std::istringstream iss(line);
  std::string cpu;
  uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0;
  iss >> cpu >> user >> nice >> system >> idle >> iowait;
  uint64_t idle_total = idle + iowait;
  uint64_t total = user + nice + system + idle + iowait;
  uint64_t idle_diff = idle_total - prev_idle;
  uint64_t total_diff = total - prev_total;
  prev_idle = idle_total;
  prev_total = total;
  if (total_diff == 0) {
    return 0.0;
  }
  return 100.0 * (1.0 - static_cast<double>(idle_diff) /
                              static_cast<double>(total_diff));
}

}  // namespace

ResourceMonitor::ResourceMonitor() = default;

ResourceMonitor::~ResourceMonitor() { Stop(); }

void ResourceMonitor::Start(std::chrono::milliseconds interval) {
  Stop();
  samples_.clear();
  initial_disk_read_ = ReadIoBytes("read_bytes");
  initial_disk_write_ = ReadIoBytes("write_bytes");
  initial_page_faults_ = ReadProcCounter("/proc/self/status", "Fault:");
  initial_voluntary_ctx_ =
      ReadProcCounter("/proc/self/status", "voluntary_ctxt_switches:");
  initial_involuntary_ctx_ =
      ReadProcCounter("/proc/self/status", "nonvoluntary_ctxt_switches:");
  start_time_ = std::chrono::steady_clock::now();
  running_ = true;
  sampler_ = std::thread([this, interval]() {
    uint64_t prev_idle = 0;
    uint64_t prev_total = 0;
    while (running_) {
      ResourceSample sample;
      auto elapsed = std::chrono::steady_clock::now() - start_time_;
      sample.timestamp_sec =
          std::chrono::duration<double>(elapsed).count();
      sample.cpu_percent = ReadCpuPercent(prev_idle, prev_total);
      sample.rss_kb = GetCurrentRssKb();
      sample.thread_count = CountThreads();
      sample.voluntary_ctx_switches =
          ReadProcCounter("/proc/self/status", "voluntary_ctxt_switches:");
      sample.involuntary_ctx_switches =
          ReadProcCounter("/proc/self/status", "nonvoluntary_ctxt_switches:");
      sample.page_faults = ReadProcCounter("/proc/self/status", "Fault:");
      sample.disk_read_bytes = ReadIoBytes("read_bytes");
      sample.disk_write_bytes = ReadIoBytes("write_bytes");
      samples_.push_back(sample);
      std::this_thread::sleep_for(interval);
    }
  });
}

void ResourceMonitor::Stop() {
  if (running_) {
    running_ = false;
    if (sampler_.joinable()) {
      sampler_.join();
    }
  }
}

ResourceSummary ResourceMonitor::GetSummary() const {
  ResourceSummary summary;
  summary.samples = samples_;
  if (samples_.empty()) {
    summary.peak_rss_kb = static_cast<double>(GetPeakRssKb());
    return summary;
  }

  double cpu_sum = 0.0;
  for (const auto& s : samples_) {
    summary.peak_rss_kb = std::max(summary.peak_rss_kb, static_cast<double>(s.rss_kb));
    summary.avg_rss_kb += static_cast<double>(s.rss_kb);
    summary.peak_cpu_percent =
        std::max(summary.peak_cpu_percent, s.cpu_percent);
    cpu_sum += s.cpu_percent;
  }
  summary.avg_rss_kb /= static_cast<double>(samples_.size());
  summary.avg_cpu_percent = cpu_sum / static_cast<double>(samples_.size());

  const auto& last = samples_.back();
  summary.total_disk_read_bytes =
      last.disk_read_bytes > initial_disk_read_
          ? last.disk_read_bytes - initial_disk_read_
          : 0;
  summary.total_disk_write_bytes =
      last.disk_write_bytes > initial_disk_write_
          ? last.disk_write_bytes - initial_disk_write_
          : 0;
  summary.total_ctx_switches =
      (last.voluntary_ctx_switches > initial_voluntary_ctx_
           ? last.voluntary_ctx_switches - initial_voluntary_ctx_
           : 0) +
      (last.involuntary_ctx_switches > initial_involuntary_ctx_
           ? last.involuntary_ctx_switches - initial_involuntary_ctx_
           : 0);
  summary.total_page_faults =
      last.page_faults > initial_page_faults_
          ? last.page_faults - initial_page_faults_
          : 0;

  return summary;
}

}  // namespace benchmark
