#include "benchmark/report_analysis.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace benchmark {
namespace {

void AddObservation(json& arr, const std::string& text) {
  arr.push_back(text);
}

void AddHypothesis(json& arr, const std::string& text) {
  arr.push_back(text);
}

std::vector<std::pair<uint32_t, double>> SortedConcurrencyQps(
    const json& concurrency_scaling) {
  std::vector<std::pair<uint32_t, double>> points;
  for (auto& [key, val] : concurrency_scaling.items()) {
    points.emplace_back(val.value("threads", 0),
                        val["latency"].value("throughput_qps", 0.0));
  }
  std::sort(points.begin(), points.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  return points;
}

}  // namespace

std::vector<std::string> ValidateLatencyStats(const LatencyStats& stats,
                                              const std::string& context) {
  std::vector<std::string> warnings;
  if (stats.total_queries == 0) {
    return warnings;
  }

  auto check_order = [&](double lo, double hi, const char* label_lo,
                         const char* label_hi) {
    if (hi + 1e-9 < lo) {
      std::ostringstream oss;
      oss << context << ": " << label_hi << " (" << hi << " ms) is less than "
          << label_lo << " (" << lo << " ms)";
      warnings.push_back(oss.str());
    }
  };

  check_order(stats.p50_ms, stats.p90_ms, "p50", "p90");
  check_order(stats.p90_ms, stats.p95_ms, "p90", "p95");
  check_order(stats.p95_ms, stats.p99_ms, "p95", "p99");
  check_order(stats.p99_ms, stats.max_ms, "p99", "max");

  if (stats.avg_ms > stats.max_ms + 1e-9) {
    warnings.push_back(context + ": average exceeds max latency");
  }
  if (stats.min_ms > stats.p50_ms + 1e-9) {
    warnings.push_back(context + ": min exceeds p50");
  }

  return warnings;
}

std::vector<std::string> ValidateConcurrencyScaling(
    const json& concurrency_scaling) {
  std::vector<std::string> warnings;
  auto points = SortedConcurrencyQps(concurrency_scaling);
  if (points.size() < 2) {
    return warnings;
  }

  double baseline_qps = points[0].second;
  for (size_t i = 0; i < points.size(); ++i) {
    double speedup =
        baseline_qps > 0 ? points[i].second / baseline_qps : 0.0;
    double efficiency =
        points[i].first > 0 ? speedup / static_cast<double>(points[i].first) : 0.0;

    if (efficiency > 1.05) {
      std::ostringstream oss;
      oss << "Concurrency at " << points[i].first
          << " threads: parallel efficiency is " << efficiency
          << " (>100%). This may indicate measurement noise or a flawed baseline.";
      warnings.push_back(oss.str());
    }

    if (i > 0 && points[i].second < points[i - 1].second * 0.95) {
      std::ostringstream oss;
      oss << "Throughput decreased from " << points[i - 1].first << " to "
          << points[i].first << " threads ("
          << points[i - 1].second << " -> " << points[i].second
          << " qps). Investigate contention or oversubscription.";
      warnings.push_back(oss.str());
    }
  }

  return warnings;
}

std::vector<std::string> ValidateQualityMetrics(const QualityMetrics& quality) {
  std::vector<std::string> warnings;

  if (quality.experimental) {
    warnings.push_back(
        "Quality metrics are marked EXPERIMENTAL: judgments are synthetically "
        "derived from substring matching, not human-labeled relevance.");
  }

  if (quality.precision_at_10 >= 0.999 && quality.recall >= 0.999 &&
      quality.ndcg_at_10 >= 0.999) {
    warnings.push_back(
        "Perfect or near-perfect quality scores on a synthetic corpus are "
        "expected and do not indicate real-world retrieval quality.");
  }

  if (quality.num_queries_evaluated == 0) {
    warnings.push_back("No queries with relevance judgments were evaluated.");
  }

  if (quality.num_queries_evaluated > 0 &&
      quality.num_queries_evaluated < quality.num_judgments / 2) {
    warnings.push_back(
        "Fewer than half of generated judgments had matching relevant "
        "documents; quality metrics may be unstable.");
  }

  return warnings;
}

std::vector<std::string> CollectAllSanityWarnings(
    const BenchmarkResults& results) {
  std::vector<std::string> warnings = results.sanity_warnings;

  for (auto& [name, data] : results.query_benchmarks.items()) {
    if (!data.value("supported", true)) {
      continue;
    }
    LatencyStats stats;
    stats.p50_ms = data.value("p50_ms", 0.0);
    stats.p90_ms = data.value("p90_ms", 0.0);
    stats.p95_ms = data.value("p95_ms", 0.0);
    stats.p99_ms = data.value("p99_ms", 0.0);
    stats.avg_ms = data.value("avg_ms", 0.0);
    stats.max_ms = data.value("max_ms", 0.0);
    stats.min_ms = data.value("min_ms", 0.0);
    stats.total_queries = data.value("total_queries", 0);
    auto w = ValidateLatencyStats(stats, "query:" + name);
    warnings.insert(warnings.end(), w.begin(), w.end());
  }

  auto conc = ValidateConcurrencyScaling(results.concurrency_scaling);
  warnings.insert(warnings.end(), conc.begin(), conc.end());

  auto qual = ValidateQualityMetrics(results.quality);
  warnings.insert(warnings.end(), qual.begin(), qual.end());

  for (const auto& ir : results.indexing_scaling) {
    if (ir.thread_count > 1 && ir.docs_per_sec < results.indexing_scaling[0].docs_per_sec * 0.9) {
      std::ostringstream oss;
      oss << "Indexing with " << ir.thread_count
          << " threads is slower than single-threaded ("
          << ir.docs_per_sec << " vs "
          << results.indexing_scaling[0].docs_per_sec
          << " docs/sec). Shard-merge overhead may dominate.";
      warnings.push_back(oss.str());
    }
  }

  return warnings;
}

json AnalyzeConcurrencyScaling(const json& concurrency_scaling) {
  json analysis;
  json measured = json::array();
  json observations = json::array();
  json hypotheses = json::array();
  json investigations = json::array();

  auto points = SortedConcurrencyQps(concurrency_scaling);
  if (points.empty()) {
    observations.push_back("No concurrency data collected.");
    analysis["measured"] = measured;
    analysis["observations"] = observations;
    analysis["possible_causes"] = hypotheses;
    analysis["future_investigation"] = investigations;
    return analysis;
  }

  double baseline_qps = points[0].second;
  double peak_qps = baseline_qps;
  uint32_t peak_threads = points[0].first;
  bool plateaued = false;

  for (const auto& [threads, qps] : points) {
    double speedup = baseline_qps > 0 ? qps / baseline_qps : 0.0;
    double efficiency =
        threads > 0 ? speedup / static_cast<double>(threads) : 0.0;
    measured.push_back({{"threads", threads},
                        {"qps", qps},
                        {"speedup", speedup},
                        {"efficiency", efficiency}});
    if (qps > peak_qps) {
      peak_qps = qps;
      peak_threads = threads;
    }
  }

  std::ostringstream obs;
  obs << "Throughput rose from " << baseline_qps << " qps (1 thread) to peak "
      << peak_qps << " qps at " << peak_threads << " threads.";
  AddObservation(observations, obs.str());

  for (size_t i = 1; i < points.size(); ++i) {
    double gain = points[i].second - points[i - 1].second;
    double rel = points[i - 1].second > 0 ? gain / points[i - 1].second : 0.0;
    if (rel < 0.05 && points[i].first >= 4) {
      plateaued = true;
      std::ostringstream p;
      p << "Throughput plateaued around " << points[i].first
        << " threads (" << points[i].second << " qps, <5% gain from prior level).";
      AddObservation(observations, p.str());
      break;
    }
    if (rel < 0.0) {
      std::ostringstream d;
      d << "Throughput decreased at " << points[i].first << " threads.";
      AddObservation(observations, d.str());
    }
  }

  double best_eff = 0.0;
  for (auto& [key, val] : concurrency_scaling.items()) {
    double eff = val.value("efficiency", 0.0);
    if (eff > best_eff) {
      best_eff = eff;
    }
  }

  if (best_eff < 0.5 && points.size() > 1) {
    AddObservation(observations,
                   "Parallel efficiency remains below 50% beyond 1 thread.");
    AddHypothesis(hypotheses,
                  "Search path may be memory-bandwidth limited or subject to "
                  "lock contention on shared index structures.");
    investigations.push_back(
        "Profile with per-thread CPU and cache-miss counters during concurrent "
        "search.");
  } else if (plateaued) {
    AddHypothesis(
        hypotheses,
        "Diminishing returns may indicate the workload is no longer CPU-bound "
        "at higher thread counts.");
    investigations.push_back(
        "Repeat concurrency sweep with query mixes of varying selectivity.");
  }

  if (hypotheses.empty()) {
    AddHypothesis(hypotheses,
                  "Insufficient evidence to attribute scaling behavior to a "
                  "specific subsystem.");
  }

  analysis["measured"] = measured;
  analysis["observations"] = observations;
  analysis["possible_causes"] = hypotheses;
  analysis["future_investigation"] = investigations;
  return analysis;
}

json AnalyzeIndexingScaling(const std::vector<IndexingResult>& results) {
  json analysis;
  json observations = json::array();
  json hypotheses = json::array();
  json investigations = json::array();

  if (results.empty()) {
    observations.push_back("No indexing scaling data collected.");
    analysis["observations"] = observations;
    analysis["possible_causes"] = hypotheses;
    analysis["future_investigation"] = investigations;
    return analysis;
  }

  const auto& single = results[0];
  std::ostringstream base;
  base << "Single-threaded indexing: " << single.docs_per_sec
       << " docs/sec, peak RSS "
       << static_cast<int>(single.peak_memory_kb / 1024) << " MB, avg CPU "
       << single.avg_cpu_percent << "%.";
  observations.push_back(base.str());

  if (single.avg_cpu_percent < 70.0) {
    observations.push_back(
        "Average CPU utilization during single-threaded indexing was below 70%.");
    hypotheses.push_back(
        "Indexing may be memory-allocation bound or waiting on I/O during "
        "serialization — not proven without finer-grained profiling.");
    investigations.push_back(
        "Use CPU flame graphs and track page faults during indexing.");
  }

  for (size_t i = 1; i < results.size(); ++i) {
    const auto& r = results[i];
    if (r.docs_per_sec < single.docs_per_sec) {
      std::ostringstream oss;
      oss << r.thread_count << "-thread indexing (" << r.docs_per_sec
          << " docs/sec) did not improve over single-threaded.";
      observations.push_back(oss.str());
    }
  }

  if (results.size() > 1) {
    hypotheses.push_back(
        "Multi-threaded indexing uses shard-build + merge via re-tokenization; "
        "merge cost may offset parallel gains.");
    investigations.push_back(
        "Benchmark native parallel AddDocument or batch merge without "
        "re-tokenization.");
  }

  analysis["observations"] = observations;
  analysis["possible_causes"] = hypotheses;
  analysis["future_investigation"] = investigations;
  return analysis;
}

json AnalyzeStressLevels(const std::vector<StressLevel>& levels) {
  json analysis;
  json observations = json::array();
  json hypotheses = json::array();

  if (levels.empty()) {
    analysis["observations"] = observations;
    analysis["possible_causes"] = hypotheses;
    return analysis;
  }

  double peak = 0.0;
  uint32_t peak_threads = 0;
  bool degraded = false;

  for (size_t i = 0; i < levels.size(); ++i) {
    if (levels[i].throughput_qps > peak) {
      peak = levels[i].throughput_qps;
      peak_threads = levels[i].concurrent_threads;
    }
    if (i > 0 && levels[i].throughput_qps < levels[i - 1].throughput_qps * 0.9) {
      degraded = true;
      std::ostringstream oss;
      oss << "Throughput dropped at " << levels[i].concurrent_threads
          << " concurrent threads (stress test).";
      observations.push_back(oss.str());
    }
    if (levels[i].error_rate > 0.0) {
      std::ostringstream oss;
      oss << "Non-zero error rate (" << levels[i].error_rate * 100.0
          << "%) at " << levels[i].concurrent_threads << " threads.";
      observations.push_back(oss.str());
    }
  }

  std::ostringstream peak_obs;
  peak_obs << "Highest observed stress throughput: " << peak << " qps at "
           << peak_threads << " threads.";
  observations.push_back(peak_obs.str());

  if (degraded) {
    hypotheses.push_back(
        "System may have reached oversubscription or resource exhaustion under "
        "sustained concurrent load.");
  }

  analysis["observations"] = observations;
  analysis["possible_causes"] = hypotheses;
  return analysis;
}

json BuildActionableInsights(const BenchmarkResults& results) {
  json insights;
  json scales_well = json::array();
  json does_not_scale = json::array();
  json focus_areas = json::array();
  json repeat_benchmarks = json::array();

  scales_well.push_back(
      "Single-threaded query latency on in-memory mmap index (if corpus fits RAM).");
  scales_well.push_back(
      "Index serialization throughput for corpora tested within configured scaling sizes.");

  does_not_scale.push_back(
      "Multi-threaded indexing via shard-merge (throughput may decrease vs single-threaded).");
  if (!results.unsupported_features.empty()) {
    does_not_scale.push_back(
        "Query types not implemented in the engine (excluded from latency measurements).");
  }

  focus_areas.push_back(
      "Tokenizer and posting-list traversal — dominant cost in single-threaded search.");
  focus_areas.push_back(
      "Concurrent read scalability if efficiency drops below 50%.");

  if (!results.indexing_scaling.empty() &&
      results.indexing_scaling[0].avg_cpu_percent < 70.0) {
    focus_areas.push_back(
        "Indexing pipeline CPU utilization — currently below 70% in single-threaded run.");
  }

  repeat_benchmarks.push_back(
      "Concurrency scaling after any change to index structure or locking.");
  repeat_benchmarks.push_back(
      "Dataset scaling after index format or mmap changes.");
  repeat_benchmarks.push_back(
      "Quality evaluation only after implementing real relevance judgments.");

  insights["scales_well"] = scales_well;
  insights["does_not_scale"] = does_not_scale;
  insights["optimization_focus"] = focus_areas;
  insights["repeat_after_changes"] = repeat_benchmarks;

  insights["bottleneck_assessment"] =
      "The current benchmark does not provide enough evidence to determine the "
      "primary bottleneck. No CPU profiler, lock profiler, or storage isolation "
      "was used.";

  return insights;
}

}  // namespace benchmark
