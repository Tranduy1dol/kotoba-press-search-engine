#include "benchmark/statistics.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace benchmark {

double Percentile(const std::vector<double>& sorted, double p) {
  if (sorted.empty()) {
    return 0.0;
  }
  if (sorted.size() == 1) {
    return sorted[0];
  }
  double rank = p * static_cast<double>(sorted.size() - 1);
  size_t lo = static_cast<size_t>(std::floor(rank));
  size_t hi = static_cast<size_t>(std::ceil(rank));
  double frac = rank - static_cast<double>(lo);
  return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

LatencyStats ComputeLatencyStats(const std::vector<double>& latencies_ms,
                                 double wall_time_sec, uint64_t failed) {
  LatencyStats stats;
  stats.total_queries = latencies_ms.size();
  stats.failed_queries = failed;

  if (latencies_ms.empty()) {
    return stats;
  }

  std::vector<double> sorted = latencies_ms;
  std::sort(sorted.begin(), sorted.end());

  stats.min_ms = sorted.front();
  stats.max_ms = sorted.back();
  stats.avg_ms =
      std::accumulate(sorted.begin(), sorted.end(), 0.0) /
      static_cast<double>(sorted.size());
  stats.p50_ms = Percentile(sorted, 0.50);
  stats.p90_ms = Percentile(sorted, 0.90);
  stats.p95_ms = Percentile(sorted, 0.95);
  stats.p99_ms = Percentile(sorted, 0.99);
  stats.stddev_ms = StdDev(sorted, stats.avg_ms);
  stats.wall_time_sec = wall_time_sec;

  if (wall_time_sec > 0.0) {
    stats.throughput_qps =
        static_cast<double>(latencies_ms.size()) / wall_time_sec;
  }

  stats.histogram_bins = BuildHistogram(sorted, 20);
  stats.histogram_counts = CountHistogram(sorted, stats.histogram_bins);

  return stats;
}

std::vector<double> BuildHistogram(const std::vector<double>& values,
                                   size_t num_bins) {
  if (values.empty() || num_bins == 0) {
    return {};
  }
  double min_val = *std::min_element(values.begin(), values.end());
  double max_val = *std::max_element(values.begin(), values.end());
  if (min_val == max_val) {
    return {min_val, max_val + 1.0};
  }
  double step = (max_val - min_val) / static_cast<double>(num_bins);
  std::vector<double> bins;
  bins.reserve(num_bins + 1);
  for (size_t i = 0; i <= num_bins; ++i) {
    bins.push_back(min_val + step * static_cast<double>(i));
  }
  return bins;
}

std::vector<uint64_t> CountHistogram(const std::vector<double>& values,
                                     const std::vector<double>& bins) {
  if (bins.size() < 2) {
    return {};
  }
  std::vector<uint64_t> counts(bins.size() - 1, 0);
  for (double v : values) {
    for (size_t i = 0; i + 1 < bins.size(); ++i) {
      if (v >= bins[i] && (v < bins[i + 1] || i + 2 == bins.size())) {
        counts[i]++;
        break;
      }
    }
  }
  return counts;
}

double StdDev(const std::vector<double>& values, double mean) {
  if (values.size() < 2) {
    return 0.0;
  }
  double sum_sq = 0.0;
  for (double v : values) {
    double d = v - mean;
    sum_sq += d * d;
  }
  return std::sqrt(sum_sq / static_cast<double>(values.size() - 1));
}

}  // namespace benchmark
