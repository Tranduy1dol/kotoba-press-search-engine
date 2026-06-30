#pragma once

#include <atomic>
#include <chrono>
#include <thread>

#include "benchmark/types.hpp"

namespace benchmark {

class ResourceMonitor {
 public:
  ResourceMonitor();
  ~ResourceMonitor();

  void Start(std::chrono::milliseconds interval = std::chrono::milliseconds(50));
  void Stop();
  ResourceSummary GetSummary() const;

 private:
  void SampleLoop();

  std::atomic<bool> running_{false};
  std::thread sampler_;
  mutable std::vector<ResourceSample> samples_;
  std::chrono::steady_clock::time_point start_time_;
  uint64_t initial_disk_read_ = 0;
  uint64_t initial_disk_write_ = 0;
  uint64_t initial_page_faults_ = 0;
  uint64_t initial_voluntary_ctx_ = 0;
  uint64_t initial_involuntary_ctx_ = 0;
};

}  // namespace benchmark
