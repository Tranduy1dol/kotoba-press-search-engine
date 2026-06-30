#pragma once

#include <string>
#include <vector>

#include "benchmark/types.hpp"

namespace benchmark {

std::vector<std::string> ValidateLatencyStats(const LatencyStats& stats,
                                              const std::string& context);

std::vector<std::string> ValidateConcurrencyScaling(
    const json& concurrency_scaling);

std::vector<std::string> ValidateQualityMetrics(const QualityMetrics& quality);

std::vector<std::string> CollectAllSanityWarnings(
    const BenchmarkResults& results);

json AnalyzeConcurrencyScaling(const json& concurrency_scaling);

json AnalyzeIndexingScaling(const std::vector<IndexingResult>& results);

json AnalyzeStressLevels(const std::vector<StressLevel>& levels);

json BuildActionableInsights(const BenchmarkResults& results);

}  // namespace benchmark
