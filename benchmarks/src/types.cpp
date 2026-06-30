#include "benchmark/types.hpp"

namespace benchmark {

std::string QueryTypeName(QueryType type) {
  switch (type) {
    case QueryType::kSingleKeyword:
      return "single_keyword";
    case QueryType::kMultiKeyword:
      return "multi_keyword";
    case QueryType::kPhrase:
      return "phrase";
    case QueryType::kBooleanAnd:
      return "boolean_and";
    case QueryType::kBooleanOr:
      return "boolean_or";
    case QueryType::kWildcard:
      return "wildcard";
    case QueryType::kPrefix:
      return "prefix";
    case QueryType::kFuzzy:
      return "fuzzy";
    case QueryType::kRanked:
      return "ranked";
    case QueryType::kTopK:
      return "top_k";
  }
  return "unknown";
}

bool IsQueryTypeSupported(QueryType type) {
  switch (type) {
    case QueryType::kSingleKeyword:
    case QueryType::kMultiKeyword:
    case QueryType::kRanked:
    case QueryType::kTopK:
      return true;
    case QueryType::kPhrase:
    case QueryType::kBooleanAnd:
    case QueryType::kBooleanOr:
    case QueryType::kWildcard:
    case QueryType::kPrefix:
    case QueryType::kFuzzy:
      return false;
  }
  return false;
}

}  // namespace benchmark
