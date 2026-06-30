#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace benchmark {

struct BenchmarkConfig {
  uint64_t random_seed = 42;
  std::string stopwords_path = BENCHMARK_DATA_DIR "/stopwords_vi.txt";
  std::string output_dir = "benchmarks/results";
  std::string dataset_path;
  std::string query_set_path;

  std::vector<uint64_t> dataset_sizes = {10000, 100000};
  std::vector<uint64_t> scaling_sizes = {10000, 100000, 1000000};
  std::vector<uint32_t> indexing_thread_counts = {1, 2, 4, 8, 16};
  std::vector<uint32_t> concurrency_thread_counts = {1,  2,  4,  8,
                                                     16, 32, 64, 128};

  uint64_t queries_per_type = 1000;
  uint64_t warmup_queries = 100;
  uint64_t avg_doc_length = 200;
  uint64_t vocabulary_size = 50000;
  uint32_t num_topics = 100;

  std::unordered_map<std::string, double> mixed_workload = {
      {"keyword", 0.6}, {"phrase", 0.2}, {"boolean", 0.1}, {"fuzzy", 0.1}};

  uint32_t stress_initial_threads = 1;
  uint32_t stress_max_threads = 256;
  uint32_t stress_step = 4;
  uint64_t stress_queries_per_level = 500;

  uint64_t cache_cold_iterations = 10;
  uint64_t cache_warm_iterations = 1000;

  bool skip_large_datasets = true;
  uint64_t large_dataset_threshold = 1000000;

  static BenchmarkConfig LoadFromFile(const std::string& path);
  static BenchmarkConfig Default();
};

}  // namespace benchmark
