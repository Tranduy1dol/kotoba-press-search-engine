#include "benchmark/quality_metrics.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "search/searcher/searcher.h"

namespace benchmark {

double PrecisionAtK(const std::vector<uint32_t>& retrieved,
                    const std::vector<uint32_t>& relevant, size_t k) {
  if (retrieved.empty() || relevant.empty()) {
    return 0.0;
  }
  std::unordered_set<uint32_t> rel_set(relevant.begin(), relevant.end());
  size_t limit = std::min(k, retrieved.size());
  size_t hits = 0;
  for (size_t i = 0; i < limit; ++i) {
    if (rel_set.count(retrieved[i]) > 0) {
      hits++;
    }
  }
  return static_cast<double>(hits) / static_cast<double>(limit);
}

double RecallAtK(const std::vector<uint32_t>& retrieved,
                 const std::vector<uint32_t>& relevant, size_t k) {
  if (relevant.empty()) {
    return 0.0;
  }
  std::unordered_set<uint32_t> rel_set(relevant.begin(), relevant.end());
  size_t limit = std::min(k, retrieved.size());
  size_t hits = 0;
  for (size_t i = 0; i < limit; ++i) {
    if (rel_set.count(retrieved[i]) > 0) {
      hits++;
    }
  }
  return static_cast<double>(hits) / static_cast<double>(relevant.size());
}

double MeanAveragePrecision(
    const std::vector<std::vector<uint32_t>>& all_retrieved,
    const std::vector<std::vector<uint32_t>>& all_relevant) {
  if (all_retrieved.empty()) {
    return 0.0;
  }
  double sum_ap = 0.0;
  size_t valid = 0;

  for (size_t q = 0; q < all_retrieved.size(); ++q) {
    const auto& retrieved = all_retrieved[q];
    const auto& relevant = q < all_relevant.size() ? all_relevant[q]
                                                   : std::vector<uint32_t>{};
    if (relevant.empty()) {
      continue;
    }
    std::unordered_set<uint32_t> rel_set(relevant.begin(), relevant.end());
    double ap = 0.0;
    size_t hits = 0;
    for (size_t i = 0; i < retrieved.size(); ++i) {
      if (rel_set.count(retrieved[i]) > 0) {
        hits++;
        ap += static_cast<double>(hits) / static_cast<double>(i + 1);
      }
    }
    sum_ap += ap / static_cast<double>(relevant.size());
    valid++;
  }
  return valid > 0 ? sum_ap / static_cast<double>(valid) : 0.0;
}

double MeanReciprocalRank(
    const std::vector<std::vector<uint32_t>>& retrieved,
    const std::vector<std::vector<uint32_t>>& relevant) {
  if (retrieved.empty()) {
    return 0.0;
  }
  double sum_rr = 0.0;
  size_t valid = 0;
  for (size_t q = 0; q < retrieved.size(); ++q) {
    const auto& rel = q < relevant.size() ? relevant[q] : std::vector<uint32_t>{};
    if (rel.empty()) {
      continue;
    }
    std::unordered_set<uint32_t> rel_set(rel.begin(), rel.end());
    for (size_t i = 0; i < retrieved[q].size(); ++i) {
      if (rel_set.count(retrieved[q][i]) > 0) {
        sum_rr += 1.0 / static_cast<double>(i + 1);
        break;
      }
    }
    valid++;
  }
  return valid > 0 ? sum_rr / static_cast<double>(valid) : 0.0;
}

double NdcgAtK(const std::vector<uint32_t>& retrieved,
               const std::vector<uint32_t>& relevant, size_t k) {
  if (relevant.empty()) {
    return 0.0;
  }
  std::unordered_set<uint32_t> rel_set(relevant.begin(), relevant.end());
  size_t limit = std::min(k, retrieved.size());

  auto dcg = [&](size_t n) {
    double score = 0.0;
    for (size_t i = 0; i < n; ++i) {
      double rel = rel_set.count(retrieved[i]) > 0 ? 1.0 : 0.0;
      score += rel / std::log2(static_cast<double>(i + 2));
    }
    return score;
  };

  double ideal_limit = std::min(k, relevant.size());
  double idcg = 0.0;
  for (size_t i = 0; i < ideal_limit; ++i) {
    idcg += 1.0 / std::log2(static_cast<double>(i + 2));
  }
  if (idcg == 0.0) {
    return 0.0;
  }
  return dcg(limit) / idcg;
}

QualityMetrics EvaluateQuality(
    search::Searcher& searcher,
    const std::vector<RelevanceJudgment>& judgments, size_t k) {
  QualityMetrics metrics;
  metrics.num_queries = judgments.size();

  std::vector<std::vector<uint32_t>> all_retrieved;
  std::vector<std::vector<uint32_t>> all_relevant;
  double sum_p10 = 0.0;
  double sum_recall = 0.0;
  double sum_ndcg = 0.0;
  size_t valid = 0;

  for (const auto& j : judgments) {
    if (j.relevant_doc_ids.empty()) {
      continue;
    }
    auto results = searcher.Search(j.query_text, k);
    std::vector<uint32_t> retrieved;
    retrieved.reserve(results.size());
    for (const auto& r : results) {
      retrieved.push_back(r.doc_id_);
    }
    all_retrieved.push_back(retrieved);
    all_relevant.push_back(j.relevant_doc_ids);

    sum_p10 += PrecisionAtK(retrieved, j.relevant_doc_ids, k);
    sum_recall += RecallAtK(retrieved, j.relevant_doc_ids, k);
    sum_ndcg += NdcgAtK(retrieved, j.relevant_doc_ids, k);
    valid++;
  }

  if (valid > 0) {
    metrics.precision_at_10 = sum_p10 / static_cast<double>(valid);
    metrics.recall = sum_recall / static_cast<double>(valid);
    metrics.ndcg_at_10 = sum_ndcg / static_cast<double>(valid);
  }
  metrics.map_score = MeanAveragePrecision(all_retrieved, all_relevant);
  metrics.mrr = MeanReciprocalRank(all_retrieved, all_relevant);

  return metrics;
}

}  // namespace benchmark
