#include <cstring>
#include <iostream>
#include <string>

#include "benchmark/benchmark_runner.hpp"
#include "benchmark/config.hpp"

#ifndef BENCHMARK_CONFIG_DIR
#define BENCHMARK_CONFIG_DIR "benchmarks/config"
#endif

namespace {

void PrintUsage(const char* prog) {
  std::cerr << "Usage: " << prog << " [options]\n"
            << "  --config <path>   Benchmark config JSON (default: "
            << BENCHMARK_CONFIG_DIR "/default.json)\n"
            << "  --quick           Run reduced benchmark suite\n"
            << "  --output <dir>    Override output directory\n"
            << "  --seed <n>        Random seed for reproducibility\n"
            << "  --help            Show this help\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string config_path = BENCHMARK_CONFIG_DIR "/default.json";
  std::string output_override;
  bool quick = false;
  uint64_t seed_override = 0;
  bool seed_set = false;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      PrintUsage(argv[0]);
      return 0;
    }
    if (std::strcmp(argv[i], "--quick") == 0) {
      quick = true;
    } else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      config_path = argv[++i];
    } else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
      output_override = argv[++i];
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed_override = std::stoull(argv[++i]);
      seed_set = true;
    } else {
      std::cerr << "Unknown argument: " << argv[i] << "\n";
      PrintUsage(argv[0]);
      return 1;
    }
  }

  try {
    benchmark::BenchmarkConfig config =
        benchmark::BenchmarkConfig::LoadFromFile(config_path);
    if (!output_override.empty()) {
      config.output_dir = output_override;
    }
    if (seed_set) {
      config.random_seed = seed_override;
    }

    benchmark::BenchmarkRunner runner(std::move(config));
    if (quick) {
      runner.RunQuick();
    } else {
      runner.RunAll();
    }
  } catch (const std::exception& e) {
    std::cerr << "Benchmark failed: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
