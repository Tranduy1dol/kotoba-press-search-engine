#pragma once

#include <vector>

#include "benchmark/types.hpp"

namespace benchmark {

LatencyStats ComputeLatencyStats(const std::vector<double>& latencies_ms,
                                 double wall_time_sec,
                                 uint64_t failed = 0);

std::vector<double> BuildHistogram(const std::vector<double>& values,
                                   size_t num_bins = 20);

std::vector<uint64_t> CountHistogram(const std::vector<double>& values,
                                     const std::vector<double>& bins);

double Percentile(const std::vector<double>& sorted, double p);

}  // namespace benchmark
