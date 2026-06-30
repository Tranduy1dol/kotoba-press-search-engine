#include "benchmark/report_generator.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

#include "benchmark/report_analysis.hpp"

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

json CorpusStatsToJson(const CorpusStats& c) {
  return json{{"num_documents", c.num_documents},
              {"avg_document_length", c.avg_document_length},
              {"min_document_length", c.min_document_length},
              {"max_document_length", c.max_document_length},
              {"vocabulary_size", c.vocabulary_size},
              {"unique_terms", c.unique_terms},
              {"avg_tokens_per_document", c.avg_tokens_per_document},
              {"total_tokens", c.total_tokens},
              {"total_corpus_bytes", c.total_corpus_bytes},
              {"dataset_type", c.dataset_type},
              {"generation_method", c.generation_method},
              {"avg_posting_list_length", c.avg_posting_list_length},
              {"max_posting_list_length", c.max_posting_list_length},
              {"index_density", c.index_density},
              {"total_postings", c.total_postings},
              {"indexed_terms", c.indexed_terms}};
}

json ScopeToJson(const BenchmarkScope& s) {
  return json{{"in_scope", s.in_scope},
              {"out_of_scope", s.out_of_scope},
              {"assumptions", s.assumptions},
              {"limitations", s.limitations}};
}

json WorkloadToJson(const WorkloadContext& w) {
  return json{{"query_mix", w.query_mix_description},
              {"avg_query_terms", w.avg_query_terms},
              {"cache_state", w.cache_state},
              {"concurrency_threads_tested", w.concurrency_threads_tested},
              {"indexing_strategy", w.indexing_strategy},
              {"queries_per_type", w.queries_per_type},
              {"warmup_queries", w.warmup_queries},
              {"mixed_workload_profile", w.mixed_workload_profile}};
}

json QualityToJson(const QualityMetrics& q) {
  return json{{"precision_at_10", q.precision_at_10},
              {"recall", q.recall},
              {"map", q.map_score},
              {"mrr", q.mrr},
              {"ndcg_at_10", q.ndcg_at_10},
              {"num_queries", q.num_queries},
              {"num_judgments", q.num_judgments},
              {"num_queries_with_relevance", q.num_queries_with_relevance},
              {"num_queries_evaluated", q.num_queries_evaluated},
              {"experimental", q.experimental},
              {"judgment_source", q.judgment_source},
              {"methodology", q.methodology},
              {"evaluation_dataset", q.evaluation_dataset}};
}

json ResultsToJson(const BenchmarkResults& r) {
  json j;
  j["timestamp"] = r.timestamp;
  j["commit_hash"] = r.commit_hash;
  j["random_seed"] = r.random_seed;
  j["single_run"] = r.single_run;
  j["environment"] = r.environment;
  j["scope"] = ScopeToJson(r.scope);
  j["workload"] = WorkloadToJson(r.workload);
  j["corpus_stats"] = CorpusStatsToJson(r.corpus_stats);
  j["sanity_warnings"] = r.sanity_warnings;
  j["analysis"] = r.analysis;
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
  j["quality"] = QualityToJson(r.quality);
  j["cache"] = {{"cold_first_query_ms", r.cache.cold_first_query_ms},
                {"warm_repeated_query_ms", r.cache.warm_repeated_query_ms},
                {"cache_hit_ratio", r.cache.cache_hit_ratio},
                {"cold_iterations", r.cache.cold_iterations},
                {"warm_iterations", r.cache.warm_iterations}};
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

void AppendBulletList(std::ostringstream& md, const std::vector<std::string>& items) {
  for (const auto& item : items) {
    md << "- " << item << "\n";
  }
}

void AppendJsonStringList(std::ostringstream& md, const json& arr) {
  for (const auto& item : arr) {
    md << "- " << item.get<std::string>() << "\n";
  }
}

void AppendAnalysisBlock(std::ostringstream& md, const json& analysis) {
  if (analysis.contains("observations")) {
    md << "\n**Observations (measured):**\n\n";
    AppendJsonStringList(md, analysis["observations"]);
  }
  if (analysis.contains("possible_causes") && !analysis["possible_causes"].empty()) {
    md << "\n**Possible causes (hypotheses — not verified):**\n\n";
    AppendJsonStringList(md, analysis["possible_causes"]);
  }
  if (analysis.contains("future_investigation") &&
      !analysis["future_investigation"].empty()) {
    md << "\n**Future investigation:**\n\n";
    AppendJsonStringList(md, analysis["future_investigation"]);
  }
}

std::string FormatExecutiveSummary(const BenchmarkResults& r) {
  std::ostringstream md;
  md << "## Executive Summary\n\n";
  md << "### What was measured\n\n";
  md << "This run evaluated the C++ search engine core (`InvertedIndex`, "
        "`DiskIndex`, `Searcher`) on a **" << r.corpus_stats.dataset_type
     << "** corpus with seed `" << r.random_seed << "`. ";
  md << r.corpus_stats.num_documents << " documents, "
     << r.workload.queries_per_type << " queries per type, ";
  md << "and concurrency levels up to "
     << r.workload.concurrency_threads_tested << " threads.\n\n";

  md << "### What was observed\n\n";
  if (!r.indexing_scaling.empty()) {
    md << "- Single-threaded indexing: "
       << std::fixed << std::setprecision(0)
       << r.indexing_scaling[0].docs_per_sec << " docs/sec over "
       << std::setprecision(2) << r.indexing_scaling[0].total_time_sec
       << " s\n";
  }
  if (r.query_benchmarks.contains("single_keyword")) {
    auto& sk = r.query_benchmarks["single_keyword"];
    md << "- Single-keyword search: p50="
       << std::setprecision(3) << sk.value("p50_ms", 0.0) << " ms, p99="
       << sk.value("p99_ms", 0.0) << " ms (n="
       << sk.value("total_queries", 0) << ")\n";
  }
  if (!r.stress_levels.empty()) {
    double peak = 0;
    uint32_t peak_t = 0;
    for (const auto& sl : r.stress_levels) {
      if (sl.throughput_qps > peak) {
        peak = sl.throughput_qps;
        peak_t = sl.concurrent_threads;
      }
    }
    md << "- Highest stress-test throughput: " << std::setprecision(0) << peak
       << " qps at " << peak_t << " threads\n";
  }
  md << "- Peak RSS: "
     << r.memory_benchmark.value("peak_rss_kb", 0) / 1024.0 << " MB\n";

  md << "\n### What remains unknown\n\n";
  md << "- Primary bottleneck (CPU, memory, I/O, synchronization) — "
        "**not determined**; no profiler data collected.\n";
  md << "- Real-world retrieval quality — quality metrics are **experimental** "
        "on synthetic judgments.\n";
  md << "- Behavior when index exceeds available RAM.\n";
  md << "- Production gRPC path and LiveIndex write contention.\n\n";

  md << "### Recommended next steps\n\n";
  if (r.analysis.contains("actionable_insights")) {
    AppendJsonStringList(md, r.analysis["actionable_insights"]["repeat_after_changes"]);
  } else {
    md << "- Repeat concurrency benchmark after index or locking changes.\n";
    md << "- Add CPU/lock profiling before drawing bottleneck conclusions.\n";
  }

  return md.str();
}

}  // namespace

ReportGenerator::ReportGenerator(const std::filesystem::path& output_dir)
    : output_dir_(output_dir) {
  std::filesystem::create_directories(output_dir_);
}

void ReportGenerator::WriteJson(const BenchmarkResults& results,
                                const std::string& filename) const {
  BenchmarkResults enriched = results;
  enriched.sanity_warnings = CollectAllSanityWarnings(results);
  enriched.analysis = json{
      {"concurrency", AnalyzeConcurrencyScaling(results.concurrency_scaling)},
      {"indexing", AnalyzeIndexingScaling(results.indexing_scaling)},
      {"stress", AnalyzeStressLevels(results.stress_levels)},
      {"actionable_insights", BuildActionableInsights(results)}};

  std::ofstream out(output_dir_ / filename);
  out << std::setw(2) << ResultsToJson(enriched) << std::endl;
}

void ReportGenerator::WriteMarkdown(const BenchmarkResults& results,
                                    const std::string& filename) const {
  BenchmarkResults r = results;
  r.sanity_warnings = CollectAllSanityWarnings(results);
  r.analysis = json{
      {"concurrency", AnalyzeConcurrencyScaling(results.concurrency_scaling)},
      {"indexing", AnalyzeIndexingScaling(results.indexing_scaling)},
      {"stress", AnalyzeStressLevels(results.stress_levels)},
      {"actionable_insights", BuildActionableInsights(results)}};

  std::ostringstream md;
  const auto& env = r.environment;
  const auto& cs = r.corpus_stats;

  md << "# Search Engine Benchmark Report\n\n";
  md << "*Engineering evaluation document — not a performance marketing summary.*\n\n";
  md << "**Date:** " << r.timestamp << "  \n";
  md << "**Commit:** `" << r.commit_hash << "`  \n";
  md << "**Random seed:** `" << r.random_seed << "`  \n";
  md << "**Runs per config:** " << (r.single_run ? "1 (no confidence intervals)" : "multiple")
     << "\n\n";

  // --- Scope ---
  md << "## Benchmark Scope\n\n";
  md << "### In scope\n\n";
  AppendBulletList(md, r.scope.in_scope);
  md << "\n### Out of scope\n\n";
  AppendBulletList(md, r.scope.out_of_scope);
  md << "\n### Assumptions\n\n";
  AppendBulletList(md, r.scope.assumptions);
  md << "\n### Known limitations\n\n";
  AppendBulletList(md, r.scope.limitations);
  md << "\n";

  // --- Warnings ---
  if (!r.sanity_warnings.empty()) {
    md << "## Data Quality Warnings\n\n";
    md << "The following issues were detected. Treat affected metrics with caution.\n\n";
    for (const auto& w : r.sanity_warnings) {
      md << "> ⚠ " << w << "\n\n";
    }
  }

  md << FormatExecutiveSummary(r) << "\n";

  // --- Workload ---
  md << "## Workload Characteristics\n\n";
  md << "| Property | Value |\n";
  md << "|:---------|:------|\n";
  md << "| Query mix | " << r.workload.query_mix_description << " |\n";
  md << "| Queries per type | " << r.workload.queries_per_type << " |\n";
  md << "| Warmup queries | " << r.workload.warmup_queries << " |\n";
  md << "| Cache state | " << r.workload.cache_state << " |\n";
  md << "| Indexing strategy | " << r.workload.indexing_strategy << " |\n";
  md << "| Max concurrency tested | " << r.workload.concurrency_threads_tested
     << " threads |\n\n";

  // --- Dataset ---
  md << "## Dataset Transparency\n\n";
  md << "**Type:** " << cs.dataset_type << "  \n";
  md << "**Generation:** " << cs.generation_method << "\n\n";
  md << "| Metric | Value | Interpretation |\n";
  md << "|:-------|------:|:---------------|\n";
  md << "| Documents | " << cs.num_documents
     << " | Corpus size for primary benchmark |\n";
  md << "| Total corpus size | " << std::fixed << std::setprecision(1)
     << cs.total_corpus_bytes / (1024.0 * 1024.0)
     << " MB | Raw text bytes before indexing |\n";
  md << "| Avg doc length | " << std::setprecision(1) << cs.avg_document_length
     << " tokens | Mean tokens per document |\n";
  md << "| Min / max doc length | " << cs.min_document_length << " / "
     << cs.max_document_length
     << " tokens | Document length spread |\n";
  md << "| Vocabulary (generator) | " << cs.vocabulary_size
     << " | Terms available to generator |\n";
  md << "| Unique terms in corpus | " << cs.unique_terms
     << " | Terms appearing in documents |\n";
  md << "| Indexed terms | " << cs.indexed_terms
     << " | Terms in inverted index |\n";
  md << "| Total postings | " << cs.total_postings
     << " | Sum of all posting list lengths |\n";
  md << "| Avg posting list length | " << std::setprecision(2)
     << cs.avg_posting_list_length
     << " | Mean documents per term |\n";
  md << "| Max posting list length | " << cs.max_posting_list_length
     << " | Most frequent term's document frequency |\n";
  md << "| Index density | " << std::scientific << std::setprecision(3)
     << cs.index_density
     << " | postings / (docs × terms); sparsity indicator |\n\n";

  // --- Environment ---
  md << "## Environment (Measured)\n\n";
  md << "| Property | Value |\n";
  md << "|:---------|:------|\n";
  md << "| CPU | " << env.value("cpu_model", "unknown") << " |\n";
  md << "| Physical / logical cores | " << env.value("physical_cores", 0)
     << " / " << env.value("logical_cores", 0) << " |\n";
  md << "| RAM | " << std::fixed << std::setprecision(1)
     << env.value("ram_gb", 0.0) << " GB |\n";
  md << "| OS | " << env.value("os", "unknown") << " |\n";
  md << "| Compiler | " << env.value("compiler", "unknown") << " |\n";
  md << "| Flags | " << env.value("compiler_flags", "unknown") << " |\n";
  md << "| Build type | " << env.value("build_type", "unknown") << " |\n";
  md << "| Storage | " << env.value("storage_device", "unknown") << " |\n";
  md << "| Index size | " << env.value("index_size_mb", 0.0) << " MB |\n\n";

  // --- Indexing ---
  md << "## Indexing Performance\n\n";
  md << "*Measures end-to-end time to tokenize documents, build an inverted "
        "index, and serialize to disk. Docs/sec = documents / wall time.*\n\n";
  md << "### Measured results\n\n";
  md << "| Threads | Time (s) | Docs/s | Tokens/s | Peak RSS (MB) | "
        "Index (MB) | Compression | Avg CPU % |\n";
  md << "|--------:|---------:|-------:|---------:|--------------:|"
        "----------:|------------:|----------:|\n";
  for (const auto& ir : r.indexing_scaling) {
    md << "| " << ir.thread_count << " | " << std::fixed << std::setprecision(2)
       << ir.total_time_sec << " | " << std::setprecision(0) << ir.docs_per_sec
       << " | " << ir.tokens_per_sec << " | "
       << std::setprecision(1)
       << static_cast<double>(ir.peak_memory_kb) / 1024.0 << " | "
       << static_cast<double>(ir.final_index_bytes) / (1024.0 * 1024.0)
       << " | " << std::setprecision(2) << ir.compression_ratio << " | "
       << ir.avg_cpu_percent << " |\n";
  }
  AppendAnalysisBlock(md, r.analysis["indexing"]);

  // --- Query latency ---
  md << "\n## Query Latency\n\n";
  md << "*Query latency is the wall-clock time for `Searcher::Search()` to "
        "return. Percentiles describe the distribution; averages alone can "
        "hide tail latency. Throughput is measured queries / wall time for "
        "the full query batch.*\n\n";
  md << "### Measured results\n\n";
  md << "| Query type | Supported | n | p50 | p95 | p99 | σ | Avg | Max | "
        "Wall (s) | QPS | Failed |\n";
  md << "|:-----------|:---------:|--:|----:|----:|----:|--:|----:|----:|"
        "--------:|----:|-------:|\n";
  for (auto& [name, data] : r.query_benchmarks.items()) {
    bool supported = data.value("supported", true);
    md << "| " << name << " | " << (supported ? "yes" : "no") << " | "
       << data.value("total_queries", 0) << " | "
       << std::setprecision(3) << data.value("p50_ms", 0.0) << " | "
       << data.value("p95_ms", 0.0) << " | " << data.value("p99_ms", 0.0)
       << " | " << data.value("stddev_ms", 0.0) << " | "
       << data.value("avg_ms", 0.0) << " | " << data.value("max_ms", 0.0)
       << " | " << data.value("wall_time_sec", 0.0) << " | "
       << std::setprecision(0) << data.value("throughput_qps", 0.0) << " | "
       << data.value("failed_queries", 0) << " |\n";
  }

  // --- Concurrency ---
  md << "\n## Concurrency Scaling\n\n";
  md << "*Speedup = throughput / single-thread throughput. Efficiency = "
        "speedup / thread count. Efficiency above 100% or negative throughput "
        "growth may indicate measurement noise or contention.*\n\n";
  md << "### Measured results\n\n";
  md << "| Threads | QPS | Speedup | Efficiency | p99 (ms) | σ (ms) |\n";
  md << "|--------:|----:|--------:|-----------:|---------:|-------:|\n";
  for (auto& [k, v] : r.concurrency_scaling.items()) {
    md << "| " << v.value("threads", 0) << " | "
       << v["latency"].value("throughput_qps", 0.0) << " | "
       << std::setprecision(2) << v.value("speedup", 0.0) << " | "
       << v.value("efficiency", 0.0) << " | "
       << std::setprecision(3) << v["latency"].value("p99_ms", 0.0) << " | "
       << v["latency"].value("stddev_ms", 0.0) << " |\n";
  }
  AppendAnalysisBlock(md, r.analysis["concurrency"]);

  // --- Memory ---
  md << "\n## Memory\n\n";
  md << "*Peak RSS is the maximum resident set size observed. Index-in-RAM "
        "assumption holds only if RSS + index size < available RAM.*\n\n";
  md << "### Measured results\n\n";
  md << "| Metric | Value |\n";
  md << "|:-------|------:|\n";
  md << "| Peak RSS | "
     << r.memory_benchmark.value("peak_rss_kb", 0) / 1024.0 << " MB |\n";
  md << "| Avg RSS | "
     << r.memory_benchmark.value("avg_rss_kb", 0) / 1024.0 << " MB |\n";
  md << "| Bytes per document (approx) | "
     << r.memory_benchmark.value("memory_per_document_bytes", 0) << " |\n";
  md << "| Bytes per term (approx) | "
     << r.memory_benchmark.value("memory_per_term_bytes", 0) << " |\n";
  md << "| Index file size | "
     << r.memory_benchmark.value("index_size_bytes", 0) / (1024.0 * 1024.0)
     << " MB |\n";
  md << "| Raw corpus size | "
     << r.memory_benchmark.value("raw_corpus_bytes", 0) / (1024.0 * 1024.0)
     << " MB |\n\n";

  // --- Dataset scaling ---
  md << "## Dataset Scaling\n\n";
  md << "*Shows how indexing time, query latency, and memory change as corpus "
        "size increases. Only valid within tested size range.*\n\n";
  md << "### Measured results\n\n";
  md << "| Docs | Index (s) | p50 (ms) | p99 (ms) | n | Peak RSS (MB) | "
        "Index (MB) |\n";
  md << "|-----:|----------:|---------:|---------:|--:|--------------:|"
        "-----------:|\n";
  for (const auto& sp : r.dataset_scaling) {
    md << "| " << sp.dataset_size << " | " << std::setprecision(2)
       << sp.indexing_time_sec << " | " << sp.query_latency.p50_ms << " | "
       << sp.query_latency.p99_ms << " | " << sp.query_latency.total_queries
       << " | " << static_cast<double>(sp.peak_memory_kb) / 1024.0 << " | "
       << static_cast<double>(sp.index_size_bytes) / (1024.0 * 1024.0)
       << " |\n";
  }

  // --- Quality ---
  md << "\n## Search Quality (Experimental)\n\n";
  if (r.quality.experimental) {
    md << "> **EXPERIMENTAL:** These metrics use synthetic relevance judgments "
          "(substring match). They do **not** reflect real-world retrieval "
          "quality. Do not use for product decisions.\n\n";
  }
  md << "**Evaluation dataset:** " << r.quality.evaluation_dataset << "  \n";
  md << "**Judgment source:** " << r.quality.judgment_source << "  \n";
  md << "**Methodology:** " << r.quality.methodology << "\n\n";
  md << "### Measured results\n\n";
  md << "| Metric | Value | Definition |\n";
  md << "|:-------|------:|:-----------|\n";
  md << "| Judgments generated | " << r.quality.num_judgments
     << " | Total query/relevance pairs created |\n";
  md << "| With relevant docs | " << r.quality.num_queries_with_relevance
     << " | Queries that matched ≥1 document |\n";
  md << "| Evaluated | " << r.quality.num_queries_evaluated
     << " | Queries included in metric averages |\n";
  md << "| Precision@10 | " << std::setprecision(4) << r.quality.precision_at_10
     << " | Relevant in top-10 / 10 |\n";
  md << "| Recall | " << r.quality.recall
     << " | Relevant retrieved / total relevant |\n";
  md << "| MAP | " << r.quality.map_score
     << " | Mean average precision |\n";
  md << "| MRR | " << r.quality.mrr
     << " | Mean reciprocal rank of first relevant hit |\n";
  md << "| NDCG@10 | " << r.quality.ndcg_at_10
     << " | Normalized DCG at rank 10 |\n\n";

  // --- Cache ---
  md << "## Cache Behavior\n\n";
  md << "*Cold: reload index from disk per query. Warm: repeat same query on "
        "resident index. OS page cache strongly influences cold-start times.*\n\n";
  md << "### Measured results\n\n";
  md << "| State | Iterations | Avg latency (ms) | σ (ms) |\n";
  md << "|:------|----------:|-------------------:|-------:|\n";
  md << "| Cold | " << r.cache.cold_iterations << " | "
     << std::setprecision(3) << r.cache.cold_first_query_ms << " | — |\n";
  md << "| Warm | " << r.cache.warm_iterations << " | "
     << r.cache.warm_repeated_query_ms << " | — |\n\n";
  md << "Estimated latency reduction (cold→warm): "
     << std::setprecision(1) << r.cache.cache_hit_ratio * 100.0
     << "%. This is not a true cache hit ratio — no application-level cache "
        "exists.\n\n";

  // --- Stress ---
  md << "## Stress Test\n\n";
  md << "*Increases concurrent threads while issuing a fixed number of "
        "queries per level. Identifies throughput plateaus and error rates.*\n\n";
  md << "### Measured results\n\n";
  md << "| Threads | QPS | p99 (ms) | Error rate |\n";
  md << "|--------:|----:|---------:|-----------:|\n";
  for (const auto& sl : r.stress_levels) {
    md << "| " << sl.concurrent_threads << " | " << std::setprecision(0)
       << sl.throughput_qps << " | " << std::setprecision(3)
       << sl.latency.p99_ms << " | " << std::setprecision(2)
       << sl.error_rate * 100.0 << "% |\n";
  }
  AppendAnalysisBlock(md, r.analysis["stress"]);

  // --- Actionable insights ---
  md << "\n## Actionable Insights\n\n";
  const auto& insights = r.analysis["actionable_insights"];
  md << "### What appears to scale\n\n";
  AppendJsonStringList(md, insights["scales_well"]);
  md << "\n### What does not scale (or was not tested)\n\n";
  AppendJsonStringList(md, insights["does_not_scale"]);
  md << "\n### Suggested optimization focus\n\n";
  AppendJsonStringList(md, insights["optimization_focus"]);
  md << "\n### Benchmarks to repeat after changes\n\n";
  AppendJsonStringList(md, insights["repeat_after_changes"]);
  md << "\n### Bottleneck assessment\n\n";
  md << "> " << insights.value("bottleneck_assessment", std::string("")) << "\n\n";

  if (!r.unsupported_features.empty()) {
    md << "## Excluded Query Types\n\n";
    for (auto& [k, v] : r.unsupported_features.items()) {
      md << "- **" << k << ":** " << v.get<std::string>() << "\n";
    }
    md << "\n";
  }

  md << "## Charts\n\n";
  md << "See `charts/` for visualizations. Charts reproduce measured data; "
        "interpretation is in the Analysis sections above.\n";

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
blockquote { border-left: 4px solid #f59e0b; margin: 1rem 0; padding: 0.5rem 1rem;
             background: #fffbeb; color: #92400e; }
table { border-collapse: collapse; width: 100%; margin: 1rem 0; }
th, td { border: 1px solid #ddd; padding: 8px; text-align: right; }
th { background: #f5f5f5; }
th:first-child, td:first-child { text-align: left; }
.charts { display: grid; grid-template-columns: 1fr 1fr; gap: 1rem; }
.charts img { max-width: 100%; border: 1px solid #eee; }
code { background: #f4f4f4; padding: 2px 6px; border-radius: 3px; }
em { color: #555; }
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
    if (line.rfind("> ⚠", 0) == 0) {
      if (in_table) { html << "</table>\n"; in_table = false; }
      html << "<blockquote>" << line.substr(2) << "</blockquote>\n";
    } else if (line.rfind("> **", 0) == 0 || line.rfind("> ", 0) == 0) {
      if (in_table) { html << "</table>\n"; in_table = false; }
      html << "<blockquote>" << line.substr(2) << "</blockquote>\n";
    } else if (line[0] == '#') {
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
    } else if (line.size() > 1 && line[0] == '-' && line[1] == ' ') {
      html << "<li>" << line.substr(2) << "</li>\n";
    } else if (line[0] == '*') {
      html << "<p><em>" << line.substr(1) << "</em></p>\n";
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
