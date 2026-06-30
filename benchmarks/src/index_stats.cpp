#include "benchmark/index_stats.hpp"

#include <algorithm>
#include <climits>
#include <sstream>

namespace benchmark {

IndexStats ComputeIndexStats(const search::InvertedIndex& index) {
  IndexStats stats;
  const auto& inverted = index.GetIndex();
  stats.num_terms = inverted.size();

  double sum_list_len = 0.0;
  for (const auto& [term, postings] : inverted) {
    double len = static_cast<double>(postings.size());
    stats.total_postings += postings.size();
    sum_list_len += len;
    stats.max_posting_list_length =
        std::max(stats.max_posting_list_length, len);
  }

  if (stats.num_terms > 0) {
    stats.avg_posting_list_length = sum_list_len / static_cast<double>(stats.num_terms);
  }

  uint64_t docs = index.TotalDocs();
  if (docs > 0 && stats.num_terms > 0) {
    stats.index_density =
        static_cast<double>(stats.total_postings) /
        (static_cast<double>(docs) * static_cast<double>(stats.num_terms));
  }

  return stats;
}

void ExtendCorpusStats(CorpusStats& stats,
                       const std::vector<CorpusDocument>& corpus,
                       const IndexStats& index_stats) {
  stats.dataset_type = "synthetic";
  stats.generation_method =
      "Deterministic corpus: topic-biased term sampling with normal-distributed "
      "document lengths (seeded PRNG). Not real-world text.";
  stats.avg_posting_list_length = index_stats.avg_posting_list_length;
  stats.max_posting_list_length = index_stats.max_posting_list_length;
  stats.index_density = index_stats.index_density;
  stats.total_postings = index_stats.total_postings;
  stats.indexed_terms = index_stats.num_terms;

  if (corpus.empty()) {
    return;
  }

  uint64_t min_len = UINT64_MAX;
  uint64_t max_len = 0;
  uint64_t total_bytes = 0;

  for (const auto& doc : corpus) {
    total_bytes += doc.text.size();
    std::istringstream iss(doc.text);
    std::string token;
    uint64_t tokens = 0;
    while (iss >> token) {
      tokens++;
    }
    min_len = std::min(min_len, tokens);
    max_len = std::max(max_len, tokens);
  }

  stats.min_document_length = min_len;
  stats.max_document_length = max_len;
  stats.total_corpus_bytes = total_bytes;
}

}  // namespace benchmark
