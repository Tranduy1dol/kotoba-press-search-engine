#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "benchmark/types.hpp"

namespace benchmark {

class CorpusGenerator {
 public:
  CorpusGenerator(uint64_t seed, uint64_t vocabulary_size,
                  uint64_t avg_doc_length, uint32_t num_topics);

  std::vector<CorpusDocument> Generate(uint64_t num_documents) const;
  CorpusStats ComputeStats(const std::vector<CorpusDocument>& corpus) const;
  std::vector<std::string> GetVocabulary() const { return vocabulary_; }

 private:
  std::string GenerateTerm(uint64_t index) const;
  std::vector<std::string> SampleTerms(uint64_t count, uint64_t doc_seed) const;

  uint64_t seed_;
  uint64_t vocabulary_size_;
  uint64_t avg_doc_length_;
  uint32_t num_topics_;
  std::vector<std::string> vocabulary_;
  std::vector<std::string> topic_terms_;
};

}  // namespace benchmark
