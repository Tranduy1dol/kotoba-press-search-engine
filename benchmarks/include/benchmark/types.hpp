#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace benchmark {

using json = nlohmann::json;

struct CorpusDocument {
  uint32_t doc_id = 0;
  std::string title;
  std::string url;
  std::string text;
  std::vector<std::string> topics;
};

struct CorpusStats {
  uint64_t num_documents = 0;
  double avg_document_length = 0.0;
  uint64_t min_document_length = 0;
  uint64_t max_document_length = 0;
  uint64_t vocabulary_size = 0;
  uint64_t unique_terms = 0;
  double avg_tokens_per_document = 0.0;
  uint64_t total_tokens = 0;
  uint64_t total_corpus_bytes = 0;
  std::string dataset_type = "synthetic";
  std::string generation_method;
  double avg_posting_list_length = 0.0;
  double max_posting_list_length = 0.0;
  double index_density = 0.0;
  uint64_t total_postings = 0;
  uint64_t indexed_terms = 0;
};

enum class QueryType {
  kSingleKeyword,
  kMultiKeyword,
  kPhrase,
  kBooleanAnd,
  kBooleanOr,
  kWildcard,
  kPrefix,
  kFuzzy,
  kRanked,
  kTopK,
};

std::string QueryTypeName(QueryType type);
bool IsQueryTypeSupported(QueryType type);

struct Query {
  QueryType type = QueryType::kSingleKeyword;
  std::string text;
  size_t top_k = 10;
};

struct RelevanceJudgment {
  std::string query_id;
  std::string query_text;
  std::vector<uint32_t> relevant_doc_ids;
};

struct LatencyStats {
  double p50_ms = 0.0;
  double p90_ms = 0.0;
  double p95_ms = 0.0;
  double p99_ms = 0.0;
  double avg_ms = 0.0;
  double max_ms = 0.0;
  double min_ms = 0.0;
  double stddev_ms = 0.0;
  double throughput_qps = 0.0;
  double wall_time_sec = 0.0;
  uint64_t total_queries = 0;
  uint64_t failed_queries = 0;
  std::vector<double> histogram_bins;
  std::vector<uint64_t> histogram_counts;
};

struct ResourceSample {
  double timestamp_sec = 0.0;
  double cpu_percent = 0.0;
  uint64_t rss_kb = 0;
  uint32_t thread_count = 0;
  uint64_t voluntary_ctx_switches = 0;
  uint64_t involuntary_ctx_switches = 0;
  uint64_t page_faults = 0;
  uint64_t disk_read_bytes = 0;
  uint64_t disk_write_bytes = 0;
  double io_wait_percent = 0.0;
};

struct ResourceSummary {
  double peak_rss_kb = 0.0;
  double avg_rss_kb = 0.0;
  double peak_cpu_percent = 0.0;
  double avg_cpu_percent = 0.0;
  uint64_t total_disk_read_bytes = 0;
  uint64_t total_disk_write_bytes = 0;
  uint64_t total_ctx_switches = 0;
  uint64_t total_page_faults = 0;
  std::vector<ResourceSample> samples;
};

struct IndexingResult {
  uint32_t thread_count = 1;
  double total_time_sec = 0.0;
  double docs_per_sec = 0.0;
  double tokens_per_sec = 0.0;
  uint64_t peak_memory_kb = 0;
  uint64_t final_index_bytes = 0;
  double compression_ratio = 0.0;
  double avg_cpu_percent = 0.0;
  ResourceSummary resources;
};

struct QualityMetrics {
  double precision_at_10 = 0.0;
  double recall = 0.0;
  double map_score = 0.0;
  double mrr = 0.0;
  double ndcg_at_10 = 0.0;
  uint64_t num_queries = 0;
  uint64_t num_judgments = 0;
  uint64_t num_queries_with_relevance = 0;
  uint64_t num_queries_evaluated = 0;
  bool experimental = true;
  std::string judgment_source;
  std::string methodology;
  std::string evaluation_dataset;
};

struct WorkloadContext {
  std::string query_mix_description;
  double avg_query_terms = 0.0;
  std::string cache_state;
  uint32_t concurrency_threads_tested = 0;
  std::string indexing_strategy;
  uint64_t queries_per_type = 0;
  uint64_t warmup_queries = 0;
  json mixed_workload_profile;
};

struct BenchmarkScope {
  std::vector<std::string> in_scope;
  std::vector<std::string> out_of_scope;
  std::vector<std::string> assumptions;
  std::vector<std::string> limitations;
};

struct CacheResult {
  double cold_first_query_ms = 0.0;
  double warm_repeated_query_ms = 0.0;
  double cache_hit_ratio = 0.0;
  uint64_t cold_iterations = 0;
  uint64_t warm_iterations = 0;
};

struct StressLevel {
  uint32_t concurrent_threads = 0;
  double throughput_qps = 0.0;
  LatencyStats latency;
  double error_rate = 0.0;
  ResourceSummary resources;
};

struct ScalingPoint {
  uint64_t dataset_size = 0;
  double indexing_time_sec = 0.0;
  LatencyStats query_latency;
  uint64_t peak_memory_kb = 0;
  uint64_t index_size_bytes = 0;
};

struct BenchmarkResults {
  json environment;
  CorpusStats corpus_stats;
  WorkloadContext workload;
  BenchmarkScope scope;
  std::vector<IndexingResult> indexing_scaling;
  json query_benchmarks;
  json mixed_workload;
  json concurrency_scaling;
  json memory_benchmark;
  std::vector<ScalingPoint> dataset_scaling;
  QualityMetrics quality;
  CacheResult cache;
  std::vector<StressLevel> stress_levels;
  json unsupported_features;
  std::vector<std::string> sanity_warnings;
  json analysis;
  std::string timestamp;
  std::string commit_hash;
  uint64_t random_seed = 0;
  bool single_run = true;
};

}  // namespace benchmark
