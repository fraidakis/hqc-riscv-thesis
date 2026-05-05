# Canonical Benchmark CSVs

This folder stores curated benchmark CSV outputs intended to be versioned.

Guidelines:
- Commit only curated CSV and summary files promoted from a reproducible run-id.
- Do not commit transient raw logs/stderr/time traces.
- Use `scripts/bench/run_spike_suite_sync.sh --promote <name>` to populate a canonical set.

Source-of-truth per run:
- `analysis/benchmarks/runs/<run_id>/summary.csv`
- `analysis/benchmarks/runs/<run_id>/curated/*.csv`
