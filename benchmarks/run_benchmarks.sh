#!/usr/bin/env bash
# Reproduce the full benchmark suite with one command.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
CONFIG="${CONFIG:-$ROOT/benchmarks/config/default.json}"
QUICK=false
SEED=""
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --quick) QUICK=true; shift ;;
    --config) CONFIG="$2"; shift 2 ;;
    --seed) SEED="$2"; shift 2 ;;
    --debug) BUILD_TYPE=Debug; shift ;;
    *) EXTRA_ARGS+=("$1"); shift ;;
  esac
done

echo "==> Building search engine (BUILD_TYPE=$BUILD_TYPE)"
cmake -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DBUILD_BENCHMARKS=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$BUILD_DIR" --target benchmark_suite -j"$(nproc)"

BENCH_BIN="$BUILD_DIR/benchmarks/benchmark_suite"
if [[ ! -x "$BENCH_BIN" ]]; then
  BENCH_BIN="$BUILD_DIR/benchmark_suite"
fi

ARGS=(--config "$CONFIG")
if [[ "$QUICK" == true ]]; then
  ARGS+=(--quick)
fi
if [[ -n "$SEED" ]]; then
  ARGS+=(--seed "$SEED")
fi
ARGS+=("${EXTRA_ARGS[@]}")

echo "==> Running benchmark suite"
cd "$ROOT"
"$BENCH_BIN" "${ARGS[@]}"

# Find latest results directory
RESULTS_DIR=$(ls -td "$ROOT/benchmarks/results"/*/ 2>/dev/null | head -1)
if [[ -n "$RESULTS_DIR" && -f "${RESULTS_DIR}results.json" ]]; then
  echo "==> Generating charts"
  if command -v python3 &>/dev/null; then
    python3 "$ROOT/benchmarks/scripts/generate_charts.py" \
      "${RESULTS_DIR}results.json" || true
  fi
  echo ""
  echo "Benchmark complete!"
  echo "  Report: ${RESULTS_DIR}report.md"
  echo "  HTML:   ${RESULTS_DIR}report.html"
  echo "  JSON:   ${RESULTS_DIR}results.json"
fi
