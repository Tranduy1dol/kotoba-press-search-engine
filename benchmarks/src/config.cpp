#include "benchmark/config.hpp"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace benchmark {

BenchmarkConfig BenchmarkConfig::Default() {
  BenchmarkConfig config;
  config.stopwords_path = BENCHMARK_DATA_DIR "/stopwords_vi.txt";
  return config;
}

BenchmarkConfig BenchmarkConfig::LoadFromFile(const std::string& path) {
  BenchmarkConfig config = Default();
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open config file: " + path);
  }

  nlohmann::json j;
  file >> j;

  if (j.contains("random_seed")) config.random_seed = j["random_seed"];
  if (j.contains("stopwords_path")) config.stopwords_path = j["stopwords_path"];
  if (j.contains("output_dir")) config.output_dir = j["output_dir"];
  if (j.contains("dataset_path")) config.dataset_path = j["dataset_path"];
  if (j.contains("query_set_path")) config.query_set_path = j["query_set_path"];
  if (j.contains("dataset_sizes")) {
    config.dataset_sizes = j["dataset_sizes"].get<std::vector<uint64_t>>();
  }
  if (j.contains("scaling_sizes")) {
    config.scaling_sizes = j["scaling_sizes"].get<std::vector<uint64_t>>();
  }
  if (j.contains("indexing_thread_counts")) {
    config.indexing_thread_counts =
        j["indexing_thread_counts"].get<std::vector<uint32_t>>();
  }
  if (j.contains("concurrency_thread_counts")) {
    config.concurrency_thread_counts =
        j["concurrency_thread_counts"].get<std::vector<uint32_t>>();
  }
  if (j.contains("queries_per_type")) {
    config.queries_per_type = j["queries_per_type"];
  }
  if (j.contains("warmup_queries")) {
    config.warmup_queries = j["warmup_queries"];
  }
  if (j.contains("avg_doc_length")) {
    config.avg_doc_length = j["avg_doc_length"];
  }
  if (j.contains("vocabulary_size")) {
    config.vocabulary_size = j["vocabulary_size"];
  }
  if (j.contains("num_topics")) config.num_topics = j["num_topics"];
  if (j.contains("mixed_workload")) {
    for (auto& [key, val] : j["mixed_workload"].items()) {
      config.mixed_workload[key] = val.get<double>();
    }
  }
  if (j.contains("stress")) {
    auto& s = j["stress"];
    if (s.contains("initial_threads")) {
      config.stress_initial_threads = s["initial_threads"];
    }
    if (s.contains("max_threads")) config.stress_max_threads = s["max_threads"];
    if (s.contains("step")) config.stress_step = s["step"];
    if (s.contains("queries_per_level")) {
      config.stress_queries_per_level = s["queries_per_level"];
    }
  }
  if (j.contains("cache_cold_iterations")) {
    config.cache_cold_iterations = j["cache_cold_iterations"];
  }
  if (j.contains("cache_warm_iterations")) {
    config.cache_warm_iterations = j["cache_warm_iterations"];
  }
  if (j.contains("skip_large_datasets")) {
    config.skip_large_datasets = j["skip_large_datasets"];
  }
  if (j.contains("large_dataset_threshold")) {
    config.large_dataset_threshold = j["large_dataset_threshold"];
  }

  return config;
}

}  // namespace benchmark
