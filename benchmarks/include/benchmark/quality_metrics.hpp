#pragma once

#include <string>
#include <vector>

#include "benchmark/types.hpp"

namespace search {
class InvertedIndex;
class DiskIndex;
class Searcher;
}  // namespace search

namespace benchmark {

QualityMetrics EvaluateQuality(
    search::Searcher& searcher,
    const std::vector<RelevanceJudgment>& judgments, size_t k = 10);

double PrecisionAtK(const std::vector<uint32_t>& retrieved,
                    const std::vector<uint32_t>& relevant, size_t k);

double RecallAtK(const std::vector<uint32_t>& retrieved,
                 const std::vector<uint32_t>& relevant, size_t k);

double MeanAveragePrecision(
    const std::vector<std::vector<uint32_t>>& all_retrieved,
    const std::vector<std::vector<uint32_t>>& all_relevant);

double MeanReciprocalRank(const std::vector<std::vector<uint32_t>>& retrieved,
                          const std::vector<std::vector<uint32_t>>& relevant);

double NdcgAtK(const std::vector<uint32_t>& retrieved,
               const std::vector<uint32_t>& relevant, size_t k);

}  // namespace benchmark
