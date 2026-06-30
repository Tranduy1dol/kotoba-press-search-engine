#!/usr/bin/env python3
"""Generate benchmark visualization charts from results.json."""

import json
import sys
from pathlib import Path

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    print("matplotlib not installed; skipping chart generation", file=sys.stderr)
    sys.exit(0)


def load_results(path: Path) -> dict:
    with open(path) as f:
        return json.load(f)


def plot_latency_percentiles(results: dict, out_dir: Path) -> None:
    qb = results.get("query_benchmarks", {})
    types = []
    p50, p90, p95, p99 = [], [], [], []
    for name, data in qb.items():
        if not data.get("supported", True):
            continue
        types.append(name.replace("_", "\n"))
        p50.append(data.get("p50_ms", 0))
        p90.append(data.get("p90_ms", 0))
        p95.append(data.get("p95_ms", 0))
        p99.append(data.get("p99_ms", 0))

    if not types:
        return

    x = range(len(types))
    width = 0.2
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.bar([i - 1.5 * width for i in x], p50, width, label="p50")
    ax.bar([i - 0.5 * width for i in x], p90, width, label="p90")
    ax.bar([i + 0.5 * width for i in x], p95, width, label="p95")
    ax.bar([i + 1.5 * width for i in x], p99, width, label="p99")
    ax.set_xticks(list(x))
    ax.set_xticklabels(types, fontsize=8)
    ax.set_ylabel("Latency (ms)")
    ax.set_title("Query Latency Percentiles by Type")
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_dir / "latency_percentiles.png", dpi=120)
    plt.close(fig)


def plot_throughput_concurrency(results: dict, out_dir: Path) -> None:
    cs = results.get("concurrency_scaling", {})
    threads = sorted(int(k) for k in cs.keys())
    qps = [cs[str(t)]["latency"]["throughput_qps"] for t in threads]
    speedup = [cs[str(t)].get("speedup", 0) for t in threads]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    ax1.plot(threads, qps, "o-", color="#2563eb")
    ax1.set_xlabel("Threads")
    ax1.set_ylabel("Queries/sec")
    ax1.set_title("Throughput vs Concurrency")
    ax1.grid(True, alpha=0.3)

    ax2.plot(threads, speedup, "s-", color="#16a34a")
    ax2.plot(threads, threads, "--", color="#94a3b8", label="ideal")
    ax2.set_xlabel("Threads")
    ax2.set_ylabel("Speedup")
    ax2.set_title("Speedup vs Concurrency")
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_dir / "throughput_concurrency.png", dpi=120)
    plt.close(fig)


def plot_indexing_scaling(results: dict, out_dir: Path) -> None:
    data = results.get("indexing_scaling", [])
    if not data:
        return
    threads = [d["thread_count"] for d in data]
    dps = [d["docs_per_sec"] for d in data]
    fig, ax = plt.subplots(figsize=(8, 5))
    ax.bar(threads, dps, color="#7c3aed")
    ax.set_xlabel("Threads")
    ax.set_ylabel("Documents/sec")
    ax.set_title("Indexing Throughput Scaling")
    fig.tight_layout()
    fig.savefig(out_dir / "indexing_scaling.png", dpi=120)
    plt.close(fig)


def plot_memory_scaling(results: dict, out_dir: Path) -> None:
    data = results.get("dataset_scaling", [])
    if not data:
        return
    sizes = [d["dataset_size"] for d in data]
    mem = [d["peak_memory_kb"] / 1024 for d in data]
    idx = [d["index_size_bytes"] / (1024 * 1024) for d in data]

    fig, ax1 = plt.subplots(figsize=(8, 5))
    ax1.plot(sizes, mem, "o-", label="Peak RSS (MB)", color="#dc2626")
    ax1.plot(sizes, idx, "s-", label="Index size (MB)", color="#0891b2")
    ax1.set_xlabel("Dataset size (documents)")
    ax1.set_ylabel("MB")
    ax1.set_title("Memory and Index Size Scaling")
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_dir / "memory_scaling.png", dpi=120)
    plt.close(fig)


def plot_latency_histogram(results: dict, out_dir: Path) -> None:
    qb = results.get("query_benchmarks", {})
    data = qb.get("single_keyword", {})
    bins = data.get("histogram_bins", [])
    counts = data.get("histogram_counts", [])
    if len(bins) < 2 or not counts:
        return

    centers = [(bins[i] + bins[i + 1]) / 2 for i in range(len(counts))]
    fig, ax = plt.subplots(figsize=(8, 5))
    ax.bar(centers, counts, width=(bins[1] - bins[0]) * 0.9, color="#f59e0b")
    ax.set_xlabel("Latency (ms)")
    ax.set_ylabel("Count")
    ax.set_title("Single Keyword Query Latency Histogram")
    fig.tight_layout()
    fig.savefig(out_dir / "latency_histogram.png", dpi=120)
    plt.close(fig)


def plot_cpu_utilization(results: dict, out_dir: Path) -> None:
    data = results.get("indexing_scaling", [])
    if not data:
        return
    threads = [d["thread_count"] for d in data]
    cpu = [d.get("avg_cpu_percent", 0) for d in data]
    fig, ax = plt.subplots(figsize=(8, 5))
    ax.bar(threads, cpu, color="#059669")
    ax.set_xlabel("Indexing threads")
    ax.set_ylabel("Avg CPU %")
    ax.set_title("CPU Utilization During Indexing")
    ax.set_ylim(0, 100)
    fig.tight_layout()
    fig.savefig(out_dir / "cpu_utilization.png", dpi=120)
    plt.close(fig)


def main() -> None:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <results.json>", file=sys.stderr)
        sys.exit(1)

    results_path = Path(sys.argv[1])
    out_dir = results_path.parent / "charts"
    out_dir.mkdir(exist_ok=True)

    results = load_results(results_path)
    plot_latency_percentiles(results, out_dir)
    plot_throughput_concurrency(results, out_dir)
    plot_indexing_scaling(results, out_dir)
    plot_memory_scaling(results, out_dir)
    plot_latency_histogram(results, out_dir)
    plot_cpu_utilization(results, out_dir)
    print(f"Charts written to {out_dir}")


if __name__ == "__main__":
    main()
