#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "benchmark/types.hpp"
#include "search/indexer/inverted_index.h"

namespace benchmark {

struct IndexStats {
  uint64_t num_terms = 0;
  uint64_t total_postings = 0;
  double avg_posting_list_length = 0.0;
  double max_posting_list_length = 0.0;
  double index_density = 0.0;
};

IndexStats ComputeIndexStats(const search::InvertedIndex& index);

void ExtendCorpusStats(CorpusStats& stats,
                       const std::vector<CorpusDocument>& corpus,
                       const IndexStats& index_stats);

}  // namespace benchmark
