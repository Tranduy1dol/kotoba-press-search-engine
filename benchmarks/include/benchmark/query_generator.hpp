#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "benchmark/corpus_generator.hpp"
#include "benchmark/types.hpp"

namespace benchmark {

class QueryGenerator {
 public:
  QueryGenerator(uint64_t seed, const std::vector<std::string>& vocabulary,
                 const std::vector<CorpusDocument>& corpus);

  std::vector<Query> Generate(QueryType type, uint64_t count) const;
  std::vector<Query> GenerateMixed(
      const std::unordered_map<std::string, double>& profile,
      uint64_t total_count) const;
  std::vector<RelevanceJudgment> GenerateRelevanceJudgments(
      uint64_t count) const;

 private:
  std::string PickTerm(uint64_t index) const;
  std::vector<uint32_t> FindRelevantDocs(const std::string& term) const;

  uint64_t seed_;
  std::vector<std::string> vocabulary_;
  std::vector<CorpusDocument> corpus_;
};

}  // namespace benchmark
