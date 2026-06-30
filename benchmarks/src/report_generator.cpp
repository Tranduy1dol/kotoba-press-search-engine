#include "benchmark/report_generator.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace benchmark {
namespace {

json LatencyStatsToJson(const LatencyStats& s) {
  return json{{"p50_ms", s.p50_ms},
              {"p90_ms", s.p90_ms},
              {"p95_ms", s.p95_ms},
              {"p99_ms", s.p99_ms},
              {"avg_ms", s.avg_ms},
              {"max_ms", s.max_ms},
              {"throughput_qps", s.throughput_qps},
              {"total_queries", s.total_queries},
              {"failed_queries", s.failed_queries},
              {"histogram_bins", s.histogram_bins},
              {"histogram_counts", s.histogram_counts}};
}

json IndexingToJson(const IndexingResult& r) {
  return json{{"thread_count", r.thread_count},
              {"total_time_sec", r.total_time_sec},
              {"docs_per_sec", r.docs_per_sec},
              {"tokens_per_sec", r.tokens_per_sec},
              {"peak_memory_kb", r.peak_memory_kb},
              {"final_index_bytes", r.final_index_bytes},
              {"compression_ratio", r.compression_ratio},
              {"avg_cpu_percent", r.avg_cpu_percent}};
}

json ResultsToJson(const BenchmarkResults& r) {
  json j;
  j["timestamp"] = r.timestamp;
  j["commit_hash"] = r.commit_hash;
  j["environment"] = r.environment;
  j["corpus_stats"] = {
      {"num_documents", r.corpus_stats.num_documents},
      {"avg_document_length", r.corpus_stats.avg_document_length},
      {"vocabulary_size", r.corpus_stats.vocabulary_size},
      {"unique_terms", r.corpus_stats.unique_terms},
      {"avg_tokens_per_document", r.corpus_stats.avg_tokens_per_document},
      {"total_tokens", r.corpus_stats.total_tokens},
  };
  j["indexing_scaling"] = json::array();
  for (const auto& ir : r.indexing_scaling) {
    j["indexing_scaling"].push_back(IndexingToJson(ir));
  }
  j["query_benchmarks"] = r.query_benchmarks;
  j["mixed_workload"] = r.mixed_workload;
  j["concurrency_scaling"] = r.concurrency_scaling;
  j["memory_benchmark"] = r.memory_benchmark;
  j["dataset_scaling"] = json::array();
  for (const auto& sp : r.dataset_scaling) {
    j["dataset_scaling"].push_back(
        {{"dataset_size", sp.dataset_size},
         {"indexing_time_sec", sp.indexing_time_sec},
         {"query_latency", LatencyStatsToJson(sp.query_latency)},
         {"peak_memory_kb", sp.peak_memory_kb},
         {"index_size_bytes", sp.index_size_bytes}});
  }
  j["quality"] = {{"precision_at_10", r.quality.precision_at_10},
                  {"recall", r.quality.recall},
                  {"map", r.quality.map_score},
                  {"mrr", r.quality.mrr},
                  {"ndcg_at_10", r.quality.ndcg_at_10},
                  {"num_queries", r.quality.num_queries}};
  j["cache"] = {{"cold_first_query_ms", r.cache.cold_first_query_ms},
                {"warm_repeated_query_ms", r.cache.warm_repeated_query_ms},
                {"cache_hit_ratio", r.cache.cache_hit_ratio}};
  j["stress_levels"] = json::array();
  for (const auto& sl : r.stress_levels) {
    j["stress_levels"].push_back(
        {{"threads", sl.concurrent_threads},
         {"throughput_qps", sl.throughput_qps},
         {"error_rate", sl.error_rate},
         {"latency", LatencyStatsToJson(sl.latency)}});
  }
  j["unsupported_features"] = r.unsupported_features;
  return j;
}

}  // namespace

ReportGenerator::ReportGenerator(const std::filesystem::path& output_dir)
    : output_dir_(output_dir) {
  std::filesystem::create_directories(output_dir_);
}

void ReportGenerator::WriteJson(const BenchmarkResults& results,
                                const std::string& filename) const {
  std::ofstream out(output_dir_ / filename);
  out << std::setw(2) << ResultsToJson(results) << std::endl;
}

std::string ReportGenerator::FormatIndexingTable(
    const std::vector<IndexingResult>& results) const {
  std::ostringstream oss;
  oss << "| Threads | Time (s) | Docs/s | Tokens/s | Peak RSS (MB) | "
         "Index (MB) | Compression | CPU % |\n";
  oss << "|--------:|---------:|-------:|---------:|--------------:|"
         "----------:|------------:|------:|\n";
  for (const auto& r : results) {
    oss << "| " << r.thread_count << " | " << std::fixed << std::setprecision(2)
        << r.total_time_sec << " | " << std::setprecision(0) << r.docs_per_sec
        << " | " << r.tokens_per_sec << " | "
        << std::setprecision(1)
        << static_cast<double>(r.peak_memory_kb) / 1024.0 << " | "
        << static_cast<double>(r.final_index_bytes) / (1024.0 * 1024.0) << " | "
        << std::setprecision(2) << r.compression_ratio << " | "
        << r.avg_cpu_percent << " |\n";
  }
  return oss.str();
}

std::string ReportGenerator::FormatLatencyTable(
    const json& query_benchmarks) const {
  std::ostringstream oss;
  oss << "| Query Type | Supported | p50 (ms) | p90 | p95 | p99 | Avg | "
         "Max | QPS | Failed |\n";
  oss << "|:-----------|:---------:|---------:|----:|----:|----:|----:|"
         "----:|----:|-------:|\n";
  for (auto& [name, data] : query_benchmarks.items()) {
    bool supported = data.value("supported", true);
    oss << "| " << name << " | " << (supported ? "yes" : "no") << " | "
        << data.value("p50_ms", 0.0) << " | " << data.value("p90_ms", 0.0)
        << " | " << data.value("p95_ms", 0.0) << " | "
        << data.value("p99_ms", 0.0) << " | " << data.value("avg_ms", 0.0)
        << " | " << data.value("max_ms", 0.0) << " | "
        << data.value("throughput_qps", 0.0) << " | "
        << data.value("failed_queries", 0) << " |\n";
  }
  return oss.str();
}

std::string ReportGenerator::FormatSummary(const BenchmarkResults& r) const {
  std::ostringstream oss;
  const auto& env = r.environment;

  double best_index_dps = 0.0;
  for (const auto& ir : r.indexing_scaling) {
    best_index_dps = std::max(best_index_dps, ir.docs_per_sec);
  }

  double best_qps = 0.0;
  for (auto& [k, v] : r.concurrency_scaling.items()) {
    best_qps = std::max(best_qps, v["latency"].value("throughput_qps", 0.0));
  }

  double saturation_threads = 0;
  double max_qps = 0;
  for (const auto& sl : r.stress_levels) {
    if (sl.throughput_qps > max_qps) {
      max_qps = sl.throughput_qps;
      saturation_threads = sl.concurrent_threads;
    }
  }

  oss << "## Executive Summary\n\n";
  oss << "- **Indexing throughput:** " << std::fixed << std::setprecision(0)
      << best_index_dps << " docs/sec\n";
  oss << "- **Peak query throughput:** " << best_qps << " queries/sec\n";
  oss << "- **Max sustainable throughput (stress):** " << max_qps
      << " qps at " << saturation_threads << " threads\n";
  oss << "- **Peak memory:** "
      << r.memory_benchmark.value("peak_rss_kb", 0) / 1024.0 << " MB\n";
  oss << "- **Retrieval quality (P@10):** " << std::setprecision(3)
      << r.quality.precision_at_10 << ", NDCG@10: " << r.quality.ndcg_at_10
      << "\n";
  oss << "- **Likely bottleneck:** ";
  double avg_cpu = 0;
  if (!r.indexing_scaling.empty()) {
    avg_cpu = r.indexing_scaling[0].avg_cpu_percent;
  }
  if (avg_cpu > 80) {
    oss << "CPU-bound\n";
  } else if (r.memory_benchmark.value("peak_rss_kb", 0) >
             env.value("ram_bytes", 1ULL) * 0.7 / 1024) {
    oss << "Memory-bound\n";
  } else {
    oss << "I/O or synchronization (evaluate concurrency efficiency)\n";
  }
  return oss.str();
}

void ReportGenerator::WriteMarkdown(const BenchmarkResults& results,
                                    const std::string& filename) const {
  std::ostringstream md;
  const auto& env = results.environment;

  md << "# Search Engine Benchmark Report\n\n";
  md << "**Date:** " << results.timestamp << "  \n";
  md << "**Commit:** `" << results.commit_hash << "`\n\n";

  md << FormatSummary(results) << "\n";

  md << "## Environment\n\n";
  md << "| Property | Value |\n";
  md << "|:---------|:------|\n";
  md << "| CPU | " << env.value("cpu_model", "unknown") << " |\n";
  md << "| Physical / Logical cores | "
     << env.value("physical_cores", 0) << " / "
     << env.value("logical_cores", 0) << " |\n";
  md << "| RAM | " << std::fixed << std::setprecision(1)
     << env.value("ram_gb", 0.0) << " GB |\n";
  md << "| OS | " << env.value("os", "unknown") << " |\n";
  md << "| Compiler | " << env.value("compiler", "unknown") << " |\n";
  md << "| Flags | " << env.value("compiler_flags", "unknown") << " |\n";
  md << "| STL | " << env.value("stl", "unknown") << " |\n";
  md << "| Build type | " << env.value("build_type", "unknown") << " |\n";
  md << "| Storage | " << env.value("storage_device", "unknown") << " |\n";
  md << "| Documents | " << env.value("num_documents", 0) << " |\n";
  md << "| Index size | " << env.value("index_size_mb", 0.0) << " MB |\n\n";

  md << "## Dataset Statistics\n\n";
  md << "| Metric | Value |\n";
  md << "|:-------|------:|\n";
  md << "| Documents | " << results.corpus_stats.num_documents << " |\n";
  md << "| Avg document length | "
     << results.corpus_stats.avg_document_length << " tokens |\n";
  md << "| Vocabulary size | " << results.corpus_stats.vocabulary_size
     << " |\n";
  md << "| Unique terms in corpus | " << results.corpus_stats.unique_terms
     << " |\n";
  md << "| Avg tokens per document | "
     << results.corpus_stats.avg_tokens_per_document << " |\n\n";

  md << "## Indexing Performance\n\n";
  md << FormatIndexingTable(results.indexing_scaling) << "\n";

  md << "## Query Latency by Type\n\n";
  md << FormatLatencyTable(results.query_benchmarks) << "\n";

  md << "## Concurrency Scaling\n\n";
  md << "| Threads | QPS | Speedup | Efficiency | p99 (ms) |\n";
  md << "|--------:|----:|--------:|-----------:|---------:|\n";
  for (auto& [k, v] : results.concurrency_scaling.items()) {
    md << "| " << v.value("threads", 0) << " | "
       << v["latency"].value("throughput_qps", 0.0) << " | "
       << v.value("speedup", 0.0) << " | " << v.value("efficiency", 0.0)
       << " | " << v["latency"].value("p99_ms", 0.0) << " |\n";
  }
  md << "\n";

  md << "## Dataset Scaling\n\n";
  md << "| Size | Index Time (s) | p50 (ms) | Peak RSS (MB) | Index (MB) |\n";
  md << "|-----:|---------------:|---------:|--------------:|-----------:|\n";
  for (const auto& sp : results.dataset_scaling) {
    md << "| " << sp.dataset_size << " | " << sp.indexing_time_sec << " | "
       << sp.query_latency.p50_ms << " | "
       << static_cast<double>(sp.peak_memory_kb) / 1024.0 << " | "
       << static_cast<double>(sp.index_size_bytes) / (1024.0 * 1024.0)
       << " |\n";
  }
  md << "\n";

  md << "## Search Quality (separate from performance)\n\n";
  md << "| Metric | Value |\n";
  md << "|:-------|------:|\n";
  md << "| Precision@10 | " << results.quality.precision_at_10 << " |\n";
  md << "| Recall | " << results.quality.recall << " |\n";
  md << "| MAP | " << results.quality.map_score << " |\n";
  md << "| MRR | " << results.quality.mrr << " |\n";
  md << "| NDCG@10 | " << results.quality.ndcg_at_10 << " |\n\n";

  md << "## Cache Behavior\n\n";
  md << "- Cold (first query): " << results.cache.cold_first_query_ms
     << " ms avg\n";
  md << "- Warm (repeated): " << results.cache.warm_repeated_query_ms
     << " ms avg\n";
  md << "- Estimated cache benefit: " << results.cache.cache_hit_ratio * 100.0
     << "%\n\n";

  md << "## Stress Test\n\n";
  md << "| Threads | QPS | p99 (ms) | Error rate |\n";
  md << "|--------:|----:|---------:|-----------:|\n";
  for (const auto& sl : results.stress_levels) {
    md << "| " << sl.concurrent_threads << " | " << sl.throughput_qps << " | "
       << sl.latency.p99_ms << " | " << sl.error_rate * 100.0 << "% |\n";
  }
  md << "\n";

  if (!results.unsupported_features.empty()) {
    md << "## Unsupported Query Features\n\n";
    for (auto& [k, v] : results.unsupported_features.items()) {
      md << "- **" << k << ":** " << v.get<std::string>() << "\n";
    }
    md << "\n";
  }

  md << "## Charts\n\n";
  md << "See `charts/` directory for generated visualizations.\n";

  std::ofstream out(output_dir_ / filename);
  out << md.str();
}

void ReportGenerator::WriteHtml(const BenchmarkResults& results,
                                const std::string& filename) const {
  WriteMarkdown(results, "report_body.md");
  std::ifstream md_in(output_dir_ / "report_body.md");
  std::ostringstream md_content;
  md_content << md_in.rdbuf();

  std::ostringstream html;
  html << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Search Engine Benchmark Report</title>
<style>
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
       max-width: 1100px; margin: 2rem auto; padding: 0 1rem; line-height: 1.6; }
h1 { border-bottom: 2px solid #333; }
table { border-collapse: collapse; width: 100%; margin: 1rem 0; }
th, td { border: 1px solid #ddd; padding: 8px; text-align: right; }
th { background: #f5f5f5; }
th:first-child, td:first-child { text-align: left; }
.charts { display: grid; grid-template-columns: 1fr 1fr; gap: 1rem; }
.charts img { max-width: 100%; border: 1px solid #eee; }
code { background: #f4f4f4; padding: 2px 6px; border-radius: 3px; }
</style>
</head>
<body>
)";

  std::string line;
  std::string md = md_content.str();
  bool in_table = false;
  std::istringstream lines(md);
  while (std::getline(lines, line)) {
    if (line.empty()) {
      if (in_table) { html << "</table>\n"; in_table = false; }
      html << "<br>\n";
      continue;
    }
    if (line[0] == '#') {
      if (in_table) { html << "</table>\n"; in_table = false; }
      size_t level = 0;
      while (level < line.size() && line[level] == '#') level++;
      std::string text = line.substr(level);
      while (!text.empty() && text[0] == ' ') text.erase(text.begin());
      html << "<h" << level << ">" << text << "</h" << level << ">\n";
    } else if (line[0] == '|') {
      if (!in_table) { html << "<table>\n"; in_table = true; }
      if (line.find("---") != std::string::npos) continue;
      html << "<tr>";
      std::istringstream cells(line);
      std::string cell;
      bool first = true;
      while (std::getline(cells, cell, '|')) {
        if (cell.empty() && first) { first = false; continue; }
        while (!cell.empty() && cell[0] == ' ') cell.erase(cell.begin());
        while (!cell.empty() && cell.back() == ' ') cell.pop_back();
        html << "<td>" << cell << "</td>";
      }
      html << "</tr>\n";
    } else if (line[0] == '-') {
      html << "<li>" << line.substr(2) << "</li>\n";
    } else {
      if (in_table) { html << "</table>\n"; in_table = false; }
      html << "<p>" << line << "</p>\n";
    }
  }
  if (in_table) html << "</table>\n";

  html << R"(<h2>Charts</h2>
<div class="charts">
<img src="charts/latency_percentiles.png" alt="Latency percentiles">
<img src="charts/throughput_concurrency.png" alt="Throughput vs concurrency">
<img src="charts/indexing_scaling.png" alt="Indexing scaling">
<img src="charts/memory_scaling.png" alt="Memory scaling">
<img src="charts/latency_histogram.png" alt="Latency histogram">
<img src="charts/cpu_utilization.png" alt="CPU utilization">
</div>
</body>
</html>
)";

  std::ofstream out(output_dir_ / filename);
  out << html.str();
}

}  // namespace benchmark
