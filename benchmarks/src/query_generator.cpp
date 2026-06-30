#include "benchmark/query_generator.hpp"

#include <random>
#include <sstream>
#include <unordered_set>

namespace benchmark {

QueryGenerator::QueryGenerator(uint64_t seed,
                               const std::vector<std::string>& vocabulary,
                               const std::vector<CorpusDocument>& corpus)
    : seed_(seed), vocabulary_(vocabulary), corpus_(corpus) {}

std::string QueryGenerator::PickTerm(uint64_t index) const {
  if (vocabulary_.empty()) {
    return "term0";
  }
  return vocabulary_[index % vocabulary_.size()];
}

std::vector<uint32_t> QueryGenerator::FindRelevantDocs(
    const std::string& term) const {
  std::vector<uint32_t> relevant;
  for (const auto& doc : corpus_) {
    if (doc.text.find(term) != std::string::npos) {
      relevant.push_back(doc.doc_id);
    }
  }
  return relevant;
}

std::vector<Query> QueryGenerator::Generate(QueryType type,
                                            uint64_t count) const {
  std::vector<Query> queries;
  queries.reserve(count);
  std::mt19937_64 rng(seed_ ^ static_cast<uint64_t>(type));

  for (uint64_t i = 0; i < count; ++i) {
    Query q;
    q.type = type;

    switch (type) {
      case QueryType::kSingleKeyword:
        q.text = PickTerm(i);
        break;
      case QueryType::kMultiKeyword: {
        q.text = PickTerm(i) + " " + PickTerm(i + 7) + " " + PickTerm(i + 13);
        break;
      }
      case QueryType::kPhrase: {
        q.text = "\"" + PickTerm(i) + " " + PickTerm(i + 1) + "\"";
        break;
      }
      case QueryType::kBooleanAnd:
        q.text = PickTerm(i) + " AND " + PickTerm(i + 3);
        break;
      case QueryType::kBooleanOr:
        q.text = PickTerm(i) + " OR " + PickTerm(i + 5);
        break;
      case QueryType::kWildcard:
        q.text = PickTerm(i).substr(0, 4) + "*";
        break;
      case QueryType::kPrefix:
        q.text = PickTerm(i).substr(0, 5);
        break;
      case QueryType::kFuzzy:
        q.text = PickTerm(i) + "~1";
        break;
      case QueryType::kRanked:
        q.text = PickTerm(i);
        q.top_k = 10;
        break;
      case QueryType::kTopK:
        q.text = PickTerm(i);
        q.top_k = 5 + static_cast<size_t>(i % 20);
        break;
    }
    queries.push_back(std::move(q));
  }
  return queries;
}

std::vector<Query> QueryGenerator::GenerateMixed(
    const std::unordered_map<std::string, double>& profile,
    uint64_t total_count) const {
  std::vector<Query> all;
  all.reserve(total_count);

  struct TypeWeight {
    QueryType type;
    std::string name;
    double weight;
  };

  std::vector<TypeWeight> weights;
  double sum = 0.0;
  auto add = [&](const std::string& name, QueryType type) {
    auto it = profile.find(name);
    if (it != profile.end() && it->second > 0.0) {
      weights.push_back({type, name, it->second});
      sum += it->second;
    }
  };
  add("keyword", QueryType::kSingleKeyword);
  add("phrase", QueryType::kPhrase);
  add("boolean", QueryType::kBooleanAnd);
  add("fuzzy", QueryType::kFuzzy);

  if (weights.empty() || sum <= 0.0) {
    return Generate(QueryType::kSingleKeyword, total_count);
  }

  for (const auto& w : weights) {
    uint64_t count = static_cast<uint64_t>(
        std::round(static_cast<double>(total_count) * w.weight / sum));
    if (count == 0 && total_count > 0) count = 1;
    auto batch = Generate(w.type, count);
    all.insert(all.end(), batch.begin(), batch.end());
  }

  while (all.size() > total_count) {
    all.pop_back();
  }
  while (all.size() < total_count) {
    auto extra = Generate(QueryType::kSingleKeyword, 1);
    all.insert(all.end(), extra.begin(), extra.end());
  }

  return all;
}

std::vector<RelevanceJudgment> QueryGenerator::GenerateRelevanceJudgments(
    uint64_t count) const {
  std::vector<RelevanceJudgment> judgments;
  judgments.reserve(count);

  for (uint64_t i = 0; i < count; ++i) {
    RelevanceJudgment j;
    j.query_id = "q" + std::to_string(i);
    j.query_text = PickTerm(i);
    j.relevant_doc_ids = FindRelevantDocs(j.query_text);
    judgments.push_back(std::move(j));
  }
  return judgments;
}

}  // namespace benchmark
