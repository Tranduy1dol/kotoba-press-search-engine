#!/usr/bin/env python3
"""Format benchmark results.json as an honest GitHub PR comment."""

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
    corpus = r.get("corpus_stats", {})
    scope = r.get("scope", {})
    warnings = r.get("sanity_warnings", [])
    quality = r.get("quality", {})
    analysis = r.get("analysis", {})
    insights = analysis.get("actionable_insights", {})

    indexing = r.get("indexing_scaling", [])
    single_thread_dps = indexing[0]["docs_per_sec"] if indexing else 0

    concurrency = r.get("concurrency_scaling", {})
    conc_points = sorted(
        ((int(k), v) for k, v in concurrency.items()),
        key=lambda x: x[0],
    )
    baseline_qps = (
        conc_points[0][1]["latency"]["throughput_qps"] if conc_points else 0
    )
    peak_qps = max(
        (v["latency"]["throughput_qps"] for _, v in conc_points), default=0
    )
    peak_threads = next(
        (
            t
            for t, v in conc_points
            if v["latency"]["throughput_qps"] == peak_qps
        ),
        0,
    )

    qb = r.get("query_benchmarks", {})
    sk = qb.get("single_keyword", {})

    lines = [
        "## Benchmark Results",
        "",
        "> **CI note:** GitHub runners have variable hardware. Use these numbers "
        "for same-PR regression checks only — not cross-machine comparisons.",
        "",
        f"**Commit:** `{commit}` | **Run:** {timestamp} | "
        f"**Seed:** `{r.get('random_seed', '?')}`",
        "",
        "### Scope",
        "",
        f"- **Corpus:** {corpus.get('dataset_type', 'unknown')}, "
        f"{corpus.get('num_documents', 0):,} docs "
        f"({corpus.get('avg_document_length', 0):.0f} tokens/doc avg, "
        f"{corpus.get('total_corpus_bytes', 0) / (1024 * 1024):.1f} MB text)",
        f"- **Runs per config:** {'1 (no confidence intervals)' if r.get('single_run', True) else 'multiple'}",
        f"- **Largest scaling size tested:** "
        f"{max((p.get('dataset_size', 0) for p in r.get('dataset_scaling', [])), default=0):,} docs",
        "",
        "### Observed (measured)",
        "",
        f"- Single-thread indexing: {fmt_num(single_thread_dps, 0)} docs/sec",
        f"- Single-keyword search: p50={sk.get('p50_ms', 0):.2f} ms, "
        f"p99={sk.get('p99_ms', 0):.2f} ms (n={sk.get('total_queries', 0)})",
        f"- Concurrency peak: {fmt_num(peak_qps, 0)} qps at {peak_threads} threads "
        f"(baseline {fmt_num(baseline_qps, 0)} qps @ 1 thread)",
        f"- Peak RSS: {fmt_num(r.get('memory_benchmark', {}).get('peak_rss_kb', 0) / 1024, 1)} MB",
        "",
        "### Unknown / not measured",
        "",
    ]

    bottleneck = insights.get(
        "bottleneck_assessment",
        "Primary bottleneck not determined — no profiler data collected.",
    )
    lines.append(f"- {bottleneck}")

    if quality.get("experimental", True):
        lines.append(
            "- Retrieval quality is **experimental** (synthetic judgments) — "
            f"P@10={quality.get('precision_at_10', 0):.3f} not meaningful for production"
        )

    limitations = scope.get("limitations", [])
    if limitations:
        lines += ["", "### Limitations", ""]
        for lim in limitations[:4]:
            lines.append(f"- {lim}")

    if warnings:
        lines += ["", "### Warnings", ""]
        for w in warnings[:5]:
            lines.append(f"- ⚠ {w}")
        if len(warnings) > 5:
            lines.append(f"- …and {len(warnings) - 5} more (see full report)")

    unsupported = r.get("unsupported_features", {})
    if unsupported:
        lines += [
            "",
            "### Excluded",
            "",
            "Query types not implemented: "
            + ", ".join(f"`{k}`" for k in sorted(unsupported.keys())),
        ]

    lines += [
        "",
        "<details>",
        "<summary>Full report artifact</summary>",
        "",
        "Download the `benchmark-results` workflow artifact for complete "
        "measured results, analysis, and charts.",
        "",
        "</details>",
    ]

    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
