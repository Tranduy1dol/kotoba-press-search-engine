#!/usr/bin/env python3
"""Format benchmark results.json as a GitHub PR comment."""

import json
import sys
from pathlib import Path


def fmt_num(n: float, decimals: int = 1) -> str:
    if n >= 1_000_000:
        return f"{n / 1_000_000:.{decimals}f}M"
    if n >= 1_000:
        return f"{n / 1_000:.{decimals}f}K"
    return f"{n:.{decimals}f}"


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: format_pr_comment.py <results.json>", file=sys.stderr)
        return 1

    results_path = Path(sys.argv[1])
    with open(results_path) as f:
        r = json.load(f)

    env = r.get("environment", {})
    commit = r.get("commit_hash", "unknown")
    timestamp = r.get("timestamp", "unknown")

    indexing = r.get("indexing_scaling", [])
    best_index_dps = max((x.get("docs_per_sec", 0) for x in indexing), default=0)

    concurrency = r.get("concurrency_scaling", {})
    best_qps = 0.0
    best_threads = 0
    for key in sorted(concurrency.keys(), key=lambda k: int(k)):
        qps = concurrency[key].get("latency", {}).get("throughput_qps", 0)
        if qps > best_qps:
            best_qps = qps
            best_threads = int(key)

    stress = r.get("stress_levels", [])
    max_stress_qps = max((s.get("throughput_qps", 0) for s in stress), default=0)
    saturation = next(
        (s.get("threads", 0) for s in stress if s.get("throughput_qps", 0) == max_stress_qps),
        0,
    )

    quality = r.get("quality", {})
    memory = r.get("memory_benchmark", {})
    corpus = r.get("corpus_stats", {})

    qb = r.get("query_benchmarks", {})
    supported_queries = [
        (name, data)
        for name, data in qb.items()
        if data.get("supported", True) and data.get("total_queries", 0) > 0
    ]

    lines = [
        "## Benchmark Results",
        "",
        "> CI runners have variable hardware — use these numbers for **same-PR "
        "regression checks**, not cross-machine comparisons.",
        "",
        f"**Commit:** `{commit}` &nbsp;|&nbsp; **Run:** {timestamp}",
        "",
        "### Summary",
        "",
        "| Metric | Value |",
        "|:-------|------:|",
        f"| Indexing throughput | {fmt_num(best_index_dps, 0)} docs/sec |",
        f"| Peak query throughput | {fmt_num(best_qps, 0)} qps ({best_threads} threads) |",
        f"| Max stress throughput | {fmt_num(max_stress_qps, 0)} qps ({saturation} threads) |",
        f"| Peak memory | {fmt_num(memory.get('peak_rss_kb', 0) / 1024, 1)} MB |",
        f"| Precision@10 | {quality.get('precision_at_10', 0):.3f} |",
        f"| NDCG@10 | {quality.get('ndcg_at_10', 0):.3f} |",
        "",
        "### Environment",
        "",
        f"- **Runner:** {env.get('os', 'unknown')}",
        f"- **CPU:** {env.get('cpu_model', 'unknown')} "
        f"({env.get('physical_cores', '?')}/{env.get('logical_cores', '?')} cores)",
        f"- **RAM:** {env.get('ram_gb', 0):.1f} GB",
        f"- **Build:** {env.get('build_type', 'unknown')} / {env.get('compiler', 'unknown')}",
        f"- **Corpus:** {corpus.get('num_documents', 0):,} docs, "
        f"{corpus.get('avg_tokens_per_document', 0):.0f} tokens/doc avg",
        "",
    ]

    if supported_queries:
        lines += ["### Query Latency (supported types)", "", "| Type | p50 | p99 | QPS |", "|:-----|----:|----:|----:|"]
        for name, data in sorted(supported_queries):
            lines.append(
                f"| {name} | {data.get('p50_ms', 0):.2f} ms | "
                f"{data.get('p99_ms', 0):.2f} ms | "
                f"{fmt_num(data.get('throughput_qps', 0), 0)} |"
            )
        lines.append("")

    unsupported = r.get("unsupported_features", {})
    if unsupported:
        lines += [
            "### Unsupported query types",
            "",
            "Skipped (not implemented in engine): "
            + ", ".join(f"`{k}`" for k in sorted(unsupported.keys())),
            "",
        ]

    scaling = r.get("dataset_scaling", [])
    if scaling:
        lines += ["### Dataset scaling", "", "| Docs | Index (s) | p50 (ms) | Index (MB) |", "|-----:|----------:|---------:|-----------:|"]
        for point in scaling:
            lat = point.get("query_latency", {})
            lines.append(
                f"| {point.get('dataset_size', 0):,} | "
                f"{point.get('indexing_time_sec', 0):.1f} | "
                f"{lat.get('p50_ms', 0):.2f} | "
                f"{point.get('index_size_bytes', 0) / (1024 * 1024):.1f} |"
            )
        lines.append("")

    lines += [
        "<details>",
        "<summary>Full report artifact</summary>",
        "",
        "Download the `benchmark-results` workflow artifact for `results.json`, "
        "`report.md`, and charts.",
        "",
        "</details>",
    ]

    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
