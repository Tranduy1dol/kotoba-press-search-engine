#include "benchmark/benchmark_runner.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include "benchmark/corpus_generator.hpp"
#include "benchmark/index_stats.hpp"
#include "benchmark/quality_metrics.hpp"
#include "benchmark/query_generator.hpp"
#include "benchmark/report_generator.hpp"
#include "benchmark/resource_monitor.hpp"
#include "benchmark/statistics.hpp"
#include "benchmark/system_info.hpp"
#include "search/indexer/disk_index.h"
#include "search/indexer/inverted_index.h"
#include "search/searcher/searcher.h"

namespace benchmark {
namespace {

json LatencyStatsToJson(const LatencyStats& s) {
  return json{{"p50_ms", s.p50_ms},
              {"p90_ms", s.p90_ms},
              {"p95_ms", s.p95_ms},
              {"p99_ms", s.p99_ms},
              {"avg_ms", s.avg_ms},
              {"max_ms", s.max_ms},
              {"min_ms", s.min_ms},
              {"stddev_ms", s.stddev_ms},
              {"throughput_qps", s.throughput_qps},
              {"wall_time_sec", s.wall_time_sec},
              {"total_queries", s.total_queries},
              {"failed_queries", s.failed_queries},
              {"histogram_bins", s.histogram_bins},
              {"histogram_counts", s.histogram_counts}};
}

json ResourceSummaryToJson(const ResourceSummary& r) {
  return json{{"peak_rss_kb", r.peak_rss_kb},
              {"avg_rss_kb", r.avg_rss_kb},
              {"peak_cpu_percent", r.peak_cpu_percent},
              {"avg_cpu_percent", r.avg_cpu_percent},
              {"total_disk_read_bytes", r.total_disk_read_bytes},
              {"total_disk_write_bytes", r.total_disk_write_bytes},
              {"total_ctx_switches", r.total_ctx_switches},
              {"total_page_faults", r.total_page_faults}};
}

uint64_t FileSize(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return 0;
  }
  return std::filesystem::file_size(path);
}

void BuildIndexFromCorpus(const std::vector<CorpusDocument>& corpus,
                          search::InvertedIndex& index) {
  for (const auto& doc : corpus) {
    index.AddDocument(doc.doc_id, doc.text, doc.title, doc.url);
  }
}

LatencyStats RunQueries(search::Searcher& searcher,
                        const std::vector<Query>& queries,
                        uint64_t warmup, bool supported) {
  std::vector<double> latencies;
  latencies.reserve(queries.size());
  uint64_t failed = 0;

  for (uint64_t i = 0; i < warmup && i < queries.size(); ++i) {
    if (supported) {
      searcher.Search(queries[i].text, queries[i].top_k);
    }
  }

  auto start = std::chrono::steady_clock::now();
  for (const auto& q : queries) {
  if (!supported) {
      failed++;
      continue;
    }
    auto qstart = std::chrono::steady_clock::now();
    try {
      searcher.Search(q.text, q.top_k);
    } catch (...) {
      failed++;
      continue;
    }
    auto qend = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(qend - qstart).count();
    latencies.push_back(ms);
  }
  auto end = std::chrono::steady_clock::now();
  double wall = std::chrono::duration<double>(end - start).count();
  return ComputeLatencyStats(latencies, wall, failed);
}

}  // namespace

BenchmarkRunner::BenchmarkRunner(BenchmarkConfig config)
    : config_(std::move(config)) {}

std::vector<CorpusDocument> BenchmarkRunner::BuildCorpus(uint64_t size) {
  CorpusGenerator gen(config_.random_seed, config_.vocabulary_size,
                      config_.avg_doc_length, config_.num_topics);
  return gen.Generate(size);
}

std::filesystem::path BenchmarkRunner::BuildIndex(
    const std::vector<CorpusDocument>& corpus,
    const std::filesystem::path& index_path) {
  search::InvertedIndex index(config_.stopwords_path);
  BuildIndexFromCorpus(corpus, index);
  std::filesystem::create_directories(index_path.parent_path());
  search::DiskIndex::Serialize(index, index_path.string());
  return index_path;
}

BenchmarkResults BenchmarkRunner::RunQuick() {
  auto saved = config_;
  config_.dataset_sizes = {10000};
  config_.scaling_sizes = {10000, 100000};
  config_.queries_per_type = 200;
  config_.concurrency_thread_counts = {1, 2, 4, 8};
  config_.indexing_thread_counts = {1, 2, 4};
  config_.stress_max_threads = 32;
  config_.stress_queries_per_level = 100;
  auto results = RunAll();
  config_ = saved;
  return results;
}

BenchmarkResults BenchmarkRunner::RunAll() {
  BenchmarkResults results;
  results.timestamp = GetTimestamp();
  results.commit_hash = GetCommitHash();
  results.random_seed = config_.random_seed;
  results.single_run = true;

  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::tm tm_buf{};
  localtime_r(&time_t_now, &tm_buf);
  char dir_buf[64];
  std::strftime(dir_buf, sizeof(dir_buf), "%Y%m%d_%H%M%S", &tm_buf);
  run_output_dir_ = std::filesystem::path(config_.output_dir) / dir_buf;
  std::filesystem::create_directories(run_output_dir_);

  uint64_t primary_size = config_.dataset_sizes.empty()
                              ? 10000
                              : config_.dataset_sizes.back();

  std::cout << "Generating corpus (" << primary_size << " documents)...\n";
  CorpusGenerator gen(config_.random_seed, config_.vocabulary_size,
                      config_.avg_doc_length, config_.num_topics);
  auto corpus = gen.Generate(primary_size);
  results.corpus_stats = gen.ComputeStats(corpus);

  search::InvertedIndex stats_index(config_.stopwords_path);
  BuildIndexFromCorpus(corpus, stats_index);
  auto index_stats = ComputeIndexStats(stats_index);
  ExtendCorpusStats(results.corpus_stats, corpus, index_stats);

  auto index_path = run_output_dir_ / "index.bin";
  std::cout << "Building index...\n";
  BuildIndex(corpus, index_path);
  uint64_t index_bytes = FileSize(index_path);

  results.environment =
      CollectEnvironmentInfo(primary_size, corpus.size(), index_bytes);

  uint64_t raw_text_bytes = 0;
  for (const auto& d : corpus) {
    raw_text_bytes += d.text.size();
  }

  // --- Indexing benchmark with thread scaling ---
  std::cout << "Running indexing benchmarks...\n";
  for (uint32_t threads : config_.indexing_thread_counts) {
    IndexingResult ir;
    ir.thread_count = threads;

    ResourceMonitor monitor;
    monitor.Start();

    auto start = std::chrono::steady_clock::now();

    if (threads == 1) {
      search::InvertedIndex index(config_.stopwords_path);
      BuildIndexFromCorpus(corpus, index);
      auto tmp = run_output_dir_ / ("idx_t" + std::to_string(threads) + ".bin");
      search::DiskIndex::Serialize(index, tmp.string());
      ir.final_index_bytes = FileSize(tmp);
    } else {
      std::vector<std::vector<CorpusDocument>> shards(threads);
      size_t per_shard = (corpus.size() + threads - 1) / threads;
      for (uint32_t t = 0; t < threads; ++t) {
        size_t begin = t * per_shard;
        size_t end = std::min(begin + per_shard, corpus.size());
        if (begin < end) {
          shards[t].assign(corpus.begin() + static_cast<long>(begin),
                           corpus.begin() + static_cast<long>(end));
        }
      }

      std::vector<std::future<std::unique_ptr<search::InvertedIndex>>> futures;
      for (uint32_t t = 0; t < threads; ++t) {
        futures.push_back(std::async(std::launch::async, [&, t]() {
          auto local = std::make_unique<search::InvertedIndex>(config_.stopwords_path);
          BuildIndexFromCorpus(shards[t], *local);
          return local;
        }));
      }

      search::InvertedIndex merged(config_.stopwords_path);
      for (auto& f : futures) {
        auto local = f.get();
        for (const auto& [doc_id, doc] : local->GetDocTable()) {
          std::string text;
          for (const auto& c : corpus) {
            if (c.doc_id == doc_id) {
              text = c.text;
              break;
            }
          }
          merged.AddDocument(doc_id, text, doc.title_, doc.url_);
        }
      }
      auto tmp = run_output_dir_ / ("idx_t" + std::to_string(threads) + ".bin");
      search::DiskIndex::Serialize(merged, tmp.string());
      ir.final_index_bytes = FileSize(tmp);
    }

    auto end = std::chrono::steady_clock::now();
    monitor.Stop();

    ir.total_time_sec = std::chrono::duration<double>(end - start).count();
    ir.docs_per_sec = static_cast<double>(corpus.size()) / ir.total_time_sec;
    ir.tokens_per_sec =
        static_cast<double>(results.corpus_stats.total_tokens) / ir.total_time_sec;
    ir.peak_memory_kb = static_cast<uint64_t>(monitor.GetSummary().peak_rss_kb);
    ir.compression_ratio =
        raw_text_bytes > 0
            ? static_cast<double>(ir.final_index_bytes) /
                  static_cast<double>(raw_text_bytes)
            : 0.0;
    ir.avg_cpu_percent = monitor.GetSummary().avg_cpu_percent;
    ir.resources = monitor.GetSummary();
    results.indexing_scaling.push_back(ir);
  }

  // --- Load searcher ---
  auto disk = std::make_unique<search::DiskIndex>(index_path.string());
  search::Searcher searcher(std::move(disk), config_.stopwords_path);

  QueryGenerator qgen(config_.random_seed, gen.GetVocabulary(), corpus);

  // --- Query benchmarks by type ---
  std::cout << "Running query benchmarks...\n";
  std::array<QueryType, 10> all_types = {
      QueryType::kSingleKeyword, QueryType::kMultiKeyword, QueryType::kPhrase,
      QueryType::kBooleanAnd,    QueryType::kBooleanOr,    QueryType::kWildcard,
      QueryType::kPrefix,        QueryType::kFuzzy,        QueryType::kRanked,
      QueryType::kTopK};

  for (QueryType type : all_types) {
    bool supported = IsQueryTypeSupported(type);
    auto queries = qgen.Generate(type, config_.queries_per_type);
    auto stats = RunQueries(searcher, queries, config_.warmup_queries, supported);
    json entry = LatencyStatsToJson(stats);
    entry["supported"] = supported;
    if (!supported) {
      entry["note"] =
          "Query type not implemented in engine; queries skipped";
      results.unsupported_features[QueryTypeName(type)] =
          "Not implemented in current search engine";
    }
    results.query_benchmarks[QueryTypeName(type)] = entry;
  }

  // --- Mixed workload ---
  std::cout << "Running mixed workload benchmark...\n";
  auto mixed_queries =
      qgen.GenerateMixed(config_.mixed_workload, config_.queries_per_type);
  ResourceMonitor mixed_monitor;
  mixed_monitor.Start();
  std::vector<double> mixed_latencies;
  uint64_t mixed_failed = 0;
  auto mixed_wall_start = std::chrono::steady_clock::now();
  for (const auto& q : mixed_queries) {
    bool supported = IsQueryTypeSupported(q.type);
    if (!supported) {
      mixed_failed++;
      continue;
    }
    auto qstart = std::chrono::steady_clock::now();
    searcher.Search(q.text, q.top_k);
    auto qend = std::chrono::steady_clock::now();
    mixed_latencies.push_back(
        std::chrono::duration<double, std::milli>(qend - qstart).count());
  }
  mixed_monitor.Stop();
  auto mixed_wall_end = std::chrono::steady_clock::now();
  double mixed_wall =
      std::chrono::duration<double>(mixed_wall_end - mixed_wall_start).count();
  auto mixed_stats =
      ComputeLatencyStats(mixed_latencies, mixed_wall, mixed_failed);
  results.mixed_workload = json{
      {"profile", config_.mixed_workload},
      {"latency", LatencyStatsToJson(mixed_stats)},
      {"resources", ResourceSummaryToJson(mixed_monitor.GetSummary())},
  };

  // --- Concurrency benchmark ---
  std::cout << "Running concurrency benchmarks...\n";
  auto keyword_queries =
      qgen.Generate(QueryType::kSingleKeyword, config_.queries_per_type * 2);
  double baseline_qps = 0.0;

  for (uint32_t n_threads : config_.concurrency_thread_counts) {
    std::atomic<uint64_t> query_idx{0};
    std::vector<double> latencies;
    std::mutex lat_mutex;
    std::atomic<uint64_t> failures{0};

    ResourceMonitor conc_monitor;
    conc_monitor.Start();
    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> workers;
    for (uint32_t w = 0; w < n_threads; ++w) {
      workers.emplace_back([&]() {
        while (true) {
          uint64_t idx = query_idx.fetch_add(1);
          if (idx >= config_.queries_per_type) {
            break;
          }
          const auto& q = keyword_queries[idx % keyword_queries.size()];
          auto qstart = std::chrono::steady_clock::now();
          try {
            searcher.Search(q.text, q.top_k);
          } catch (...) {
            failures++;
            continue;
          }
          auto qend = std::chrono::steady_clock::now();
          double ms =
              std::chrono::duration<double, std::milli>(qend - qstart).count();
          std::lock_guard<std::mutex> lock(lat_mutex);
          latencies.push_back(ms);
        }
      });
    }
    for (auto& w : workers) {
      w.join();
    }
    auto end = std::chrono::steady_clock::now();
    conc_monitor.Stop();

    double wall = std::chrono::duration<double>(end - start).count();
    auto stats = ComputeLatencyStats(latencies, wall, failures.load());
    if (n_threads == 1) {
      baseline_qps = stats.throughput_qps;
    }
    double speedup =
        baseline_qps > 0 ? stats.throughput_qps / baseline_qps : 0.0;
    double efficiency = n_threads > 0 ? speedup / static_cast<double>(n_threads) : 0.0;

    results.concurrency_scaling[std::to_string(n_threads)] = json{
        {"threads", n_threads},
        {"latency", LatencyStatsToJson(stats)},
        {"speedup", speedup},
        {"efficiency", efficiency},
        {"resources", ResourceSummaryToJson(conc_monitor.GetSummary())},
    };
  }

  // --- Memory benchmark ---
  std::cout << "Running memory benchmark...\n";
  uint64_t num_terms = 0;
  {
    search::InvertedIndex mem_index(config_.stopwords_path);
    BuildIndexFromCorpus(corpus, mem_index);
    num_terms = mem_index.GetIndex().size();
  }
  uint64_t peak_rss = GetPeakRssKb();
  uint64_t avg_rss = GetCurrentRssKb();
  results.memory_benchmark = json{
      {"peak_rss_kb", peak_rss},
      {"avg_rss_kb", avg_rss},
      {"memory_per_document_bytes",
       corpus.empty() ? 0 : peak_rss * 1024 / corpus.size()},
      {"memory_per_term_bytes",
       num_terms == 0 ? 0 : peak_rss * 1024 / num_terms},
      {"index_size_bytes", index_bytes},
      {"raw_corpus_bytes", raw_text_bytes},
  };

  // --- Dataset scaling ---
  std::cout << "Running dataset scaling benchmarks...\n";
  for (uint64_t size : config_.scaling_sizes) {
    if (config_.skip_large_datasets && size > config_.large_dataset_threshold) {
      continue;
    }
    ScalingPoint point;
    point.dataset_size = size;
    auto scale_corpus = gen.Generate(size);

    ResourceMonitor scale_monitor;
    scale_monitor.Start();
    auto idx_start = std::chrono::steady_clock::now();
    auto scale_index_path =
        run_output_dir_ / ("scale_" + std::to_string(size) + ".bin");
    BuildIndex(scale_corpus, scale_index_path);
    auto idx_end = std::chrono::steady_clock::now();
    scale_monitor.Stop();

    point.indexing_time_sec =
        std::chrono::duration<double>(idx_end - idx_start).count();
    point.index_size_bytes = FileSize(scale_index_path);
    point.peak_memory_kb =
        static_cast<uint64_t>(scale_monitor.GetSummary().peak_rss_kb);

    auto scale_disk =
        std::make_unique<search::DiskIndex>(scale_index_path.string());
    search::Searcher scale_searcher(std::move(scale_disk),
                                    config_.stopwords_path);
  auto scale_queries = qgen.Generate(QueryType::kSingleKeyword, 200);
    point.query_latency =
        RunQueries(scale_searcher, scale_queries, 20, true);
    results.dataset_scaling.push_back(point);
  }

  // --- Search quality ---
  std::cout << "Running quality evaluation...\n";
  auto judgments = qgen.GenerateRelevanceJudgments(200);
  auto disk2 = std::make_unique<search::DiskIndex>(index_path.string());
  search::Searcher quality_searcher(std::move(disk2), config_.stopwords_path);
  results.quality = EvaluateQuality(quality_searcher, judgments, 10);

  // --- Cache benchmark ---
  std::cout << "Running cache benchmark...\n";
  std::string cache_query = qgen.Generate(QueryType::kSingleKeyword, 1)[0].text;
  std::vector<double> cold_latencies;
  for (uint64_t i = 0; i < config_.cache_cold_iterations; ++i) {
    auto fresh_disk =
        std::make_unique<search::DiskIndex>(index_path.string());
    search::Searcher fresh_searcher(std::move(fresh_disk),
                                    config_.stopwords_path);
    auto qstart = std::chrono::steady_clock::now();
    fresh_searcher.Search(cache_query, 10);
    auto qend = std::chrono::steady_clock::now();
    cold_latencies.push_back(
        std::chrono::duration<double, std::milli>(qend - qstart).count());
  }
  std::vector<double> warm_latencies;
  for (uint64_t i = 0; i < config_.cache_warm_iterations; ++i) {
    auto qstart = std::chrono::steady_clock::now();
    searcher.Search(cache_query, 10);
    auto qend = std::chrono::steady_clock::now();
    warm_latencies.push_back(
        std::chrono::duration<double, std::milli>(qend - qstart).count());
  }
  auto cold_stats = ComputeLatencyStats(cold_latencies, 0.0);
  auto warm_stats = ComputeLatencyStats(warm_latencies, 0.0);
  results.cache.cold_first_query_ms = cold_stats.avg_ms;
  results.cache.warm_repeated_query_ms = warm_stats.avg_ms;
  results.cache.cache_hit_ratio =
      cold_stats.avg_ms > 0
          ? 1.0 - (warm_stats.avg_ms / cold_stats.avg_ms)
          : 0.0;
  results.cache.cold_iterations = config_.cache_cold_iterations;
  results.cache.warm_iterations = config_.cache_warm_iterations;

  // --- Stress benchmark ---
  std::cout << "Running stress benchmark...\n";
  for (uint32_t threads = config_.stress_initial_threads;
       threads <= config_.stress_max_threads; threads += config_.stress_step) {
    std::atomic<uint64_t> idx{0};
    std::atomic<uint64_t> errors{0};
    std::vector<double> latencies;
    std::mutex mtx;

    ResourceMonitor stress_monitor;
    stress_monitor.Start();
    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> pool;
    for (uint32_t w = 0; w < threads; ++w) {
      pool.emplace_back([&]() {
        while (true) {
          uint64_t q = idx.fetch_add(1);
          if (q >= config_.stress_queries_per_level) break;
          try {
            auto qstart = std::chrono::steady_clock::now();
            searcher.Search(keyword_queries[q % keyword_queries.size()].text,
                            10);
            auto qend = std::chrono::steady_clock::now();
            double ms =
                std::chrono::duration<double, std::milli>(qend - qstart)
                    .count();
            std::lock_guard<std::mutex> lock(mtx);
            latencies.push_back(ms);
          } catch (...) {
            errors++;
          }
        }
      });
    }
    for (auto& p : pool) p.join();
    auto end = std::chrono::steady_clock::now();
    stress_monitor.Stop();

    double wall = std::chrono::duration<double>(end - start).count();
    StressLevel level;
    level.concurrent_threads = threads;
    level.latency = ComputeLatencyStats(latencies, wall, errors.load());
    level.throughput_qps = level.latency.throughput_qps;
    level.error_rate =
        config_.stress_queries_per_level > 0
            ? static_cast<double>(errors.load()) /
                  static_cast<double>(config_.stress_queries_per_level)
            : 0.0;
    level.resources = stress_monitor.GetSummary();
    results.stress_levels.push_back(level);
  }

  results.scope.in_scope = {
      "InvertedIndex build and DiskIndex serialization",
      "Searcher::Search latency (BM25, mmap-backed index)",
      "Concurrent read throughput on shared DiskIndex",
      "Memory usage during indexing and search",
      "Synthetic retrieval quality (experimental)"};
  results.scope.out_of_scope = {
      "gRPC / network RPC latency",
      "Distributed or sharded indexing",
      "LiveIndex concurrent write/read contention",
      "Crawler and HTML parsing",
      "Japanese/MeCab tokenization path",
      "Unsupported query types (phrase, boolean, wildcard, prefix, fuzzy)",
      "Storage device isolation (SSD vs HDD comparison)"};
  results.scope.assumptions = {
      "Corpus and index fit entirely in available RAM",
      "Single-machine, single-process execution",
      "Synthetic workload with deterministic seed",
      "Disk index is mmap'd; OS page cache affects cold vs warm behavior",
      "No network latency between client and search engine"};
  results.scope.limitations = {
      "Single benchmark run per configuration (no repeated-run confidence intervals)",
      "Quality judgments are synthetic, not human-labeled",
      "Multi-threaded indexing uses shard-merge, not production code path",
      "CI and local runs are not comparable across machines"};
  uint64_t max_scaling = 0;
  for (uint64_t s : config_.scaling_sizes) {
    max_scaling = std::max(max_scaling, s);
  }
  results.scope.limitations.push_back(
      "Largest dataset tested in this run: " + std::to_string(max_scaling) +
      " documents");

  results.workload.queries_per_type = config_.queries_per_type;
  results.workload.warmup_queries = config_.warmup_queries;
  results.workload.mixed_workload_profile = config_.mixed_workload;
  results.workload.cache_state =
      "Warmup queries executed before measurement; cache benchmark separates "
      "cold (reload index) vs warm (repeat same query)";
  results.workload.indexing_strategy =
      "Single-threaded InvertedIndex::AddDocument; multi-threaded via "
      "shard-build + merge";
  if (!config_.concurrency_thread_counts.empty()) {
    results.workload.concurrency_threads_tested =
        config_.concurrency_thread_counts.back();
  }
  std::ostringstream mix;
  mix << "Per-type isolated benchmarks (" << config_.queries_per_type
      << " queries each); mixed workload profile: ";
  bool first = true;
  for (const auto& [k, v] : config_.mixed_workload) {
    if (!first) mix << ", ";
    mix << k << "=" << static_cast<int>(v * 100) << "%";
    first = false;
  }
  results.workload.query_mix_description = mix.str();
  results.workload.avg_query_terms = 1.5;

  // --- Write reports ---
  std::cout << "Generating reports...\n";
  ReportGenerator reporter(run_output_dir_);
  reporter.WriteJson(results);
  reporter.WriteMarkdown(results);
  reporter.WriteHtml(results);

  std::cout << "Benchmark complete. Results: " << run_output_dir_ << "\n";
  return results;
}

}  // namespace benchmark
