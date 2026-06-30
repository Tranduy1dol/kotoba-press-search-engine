#pragma once

#include <filesystem>
#include <string>

#include "benchmark/config.hpp"
#include "benchmark/types.hpp"

namespace benchmark {

class BenchmarkRunner {
 public:
  explicit BenchmarkRunner(BenchmarkConfig config);

  BenchmarkResults RunAll();
  BenchmarkResults RunQuick();

 private:
  std::vector<CorpusDocument> BuildCorpus(uint64_t size);
  std::filesystem::path BuildIndex(
      const std::vector<CorpusDocument>& corpus,
      const std::filesystem::path& index_path);

  BenchmarkConfig config_;
  std::filesystem::path run_output_dir_;
};

}  // namespace benchmark
