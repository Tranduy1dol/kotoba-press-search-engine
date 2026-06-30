#pragma once

#include <filesystem>
#include <string>

#include "benchmark/types.hpp"

namespace benchmark {

class ReportGenerator {
 public:
  explicit ReportGenerator(const std::filesystem::path& output_dir);

  void WriteJson(const BenchmarkResults& results,
                 const std::string& filename = "results.json") const;
  void WriteMarkdown(const BenchmarkResults& results,
                     const std::string& filename = "report.md") const;
  void WriteHtml(const BenchmarkResults& results,
                 const std::string& filename = "report.html") const;

  std::filesystem::path OutputDir() const { return output_dir_; }

 private:
  std::filesystem::path output_dir_;
};

}  // namespace benchmark
