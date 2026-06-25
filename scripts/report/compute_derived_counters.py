#!/usr/bin/env python3
"""
scripts/report/compute_derived_counters.py

Reads per-region EXTENDED counter CSV output from bench binaries and computes
derived microarchitectural metrics.  Appends a '# --- derived metrics ---'
section to each _latest.csv in the input directory.

New unified schema (all variants, NA for columns that don't apply):
  op, ipc, l1d_miss_rate, l1d_mpki, dtlb_mpki, br_mispred_pki,
  stall_frontend_pct, stall_backend_pct,
  vector_inst_pct, vector_ls_ratio, vpu_stall_pct,
  dram_bytes_est, arith_intensity, cqv

  - vector_inst_pct / vector_ls_ratio / vpu_stall_pct → NA for scalar variants
  - vector_ls_ratio = median_vec_load / median_vec_store (loads per store)
    Measures vector memory access pattern balance.
    High ratio → read-dominated kernel (typical for polynomial multiply).
    Guard: if vec_store == 0 and vec_load > 0 → inf; both 0 → NA.
  - cqv = iqr_cycles / median_cycles (coefficient of quartile variation)
  - CQV > 0.025 triggers a noisy-run warning on stderr.
  - All derived metrics computed from median values.

Column parsing is by header name (never by positional index).

Usage:
    python3 compute_derived_counters.py \\
        --input-dir <path> --variant <name> --levels 128,192,256
"""

import argparse
import sys
from pathlib import Path

# CQV threshold for noisy-run warning
CQV_WARN_THRESHOLD = 0.025

# Unified derived header for ALL variants
DERIVED_HEADER = (
    "op,ipc,l1d_miss_rate,l1d_mpki,dtlb_mpki,br_mispred_pki,"
    "stall_frontend_pct,stall_backend_pct,"
    "vector_inst_pct,vector_ls_ratio,vpu_stall_pct,"
    "dram_bytes_est,arith_intensity,cqv"
)


def parse_binary_csv(filepath: str) -> tuple[list[str], list[dict]]:
    """Parse a counter CSV file from the bench binary output.

    Returns (header_columns, rows) where each row is a dict keyed by header name.
    Skips comment lines (#) and the header line itself.
    Only returns rows that have at least a median_cycles column.
    """
    header = None
    rows = []

    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            # Stop at the derived-metrics section if already present
            if line == "# --- derived metrics ---":
                break
            cols = [c.strip() for c in line.split(",")]
            if header is None:
                header = cols
                continue
            if len(cols) < 2:
                continue
            row: dict = {}
            for i, h in enumerate(header):
                if i < len(cols):
                    v = cols[i].rstrip("%")
                    try:
                        row[h] = float(v)
                    except ValueError:
                        row[h] = cols[i]
            rows.append(row)

    return header or [], rows


def _get(row: dict, *keys, default: float = 0.0) -> float:
    """Return the first matching key found in row, as float."""
    for k in keys:
        if k in row:
            try:
                return float(str(row[k]).rstrip("%"))
            except (ValueError, TypeError):
                return default
    return default


def compute_derived(row: dict, variant: str) -> dict:
    """Compute derived metrics from a single operation's counter row.
    All metrics derived from median values.
    """
    d: dict = {}

    # Core counters (handle both column name variants for robustness)
    cycles   = _get(row, "median_cycles")
    instret  = _get(row, "instret", "median_instret")
    l1d_miss = _get(row, "median_l1d_miss", "median_l1d_load_miss")
    l1d_acc  = _get(row, "median_l1d_access")
    dtlb     = _get(row, "median_dtlb_miss", "median_dtlb_load_miss")
    br_mis   = _get(row, "median_br_mispred")
    stall_fe = _get(row, "median_stall_fe", "median_stall_frontend")
    stall_be = _get(row, "median_stall_be", "median_stall_backend")
    vec_inst = _get(row, "median_vec_inst", "median_vector_inst")
    vpu_stall = _get(row, "median_vpu_stall")

    # Quartile spread for CQV
    p25 = _get(row, "p25_cycles")
    p75 = _get(row, "p75_cycles")
    iqr = _get(row, "iqr_cycles", default=p75 - p25)

    # IPC (from median counts)
    d["ipc"] = instret / cycles if cycles > 0 else 0.0

    # L1D miss rate
    d["l1d_miss_rate"] = l1d_miss / l1d_acc if l1d_acc > 0 else 0.0

    # L1D MPKI
    d["l1d_mpki"] = (l1d_miss / instret) * 1000.0 if instret > 0 else 0.0

    # dTLB MPKI
    d["dtlb_mpki"] = (dtlb / instret) * 1000.0 if instret > 0 else 0.0

    # Branch misprediction per 1000 instructions
    d["br_mispred_pki"] = (br_mis / instret) * 1000.0 if instret > 0 else 0.0

    # Stall percentages (top-down lite)
    d["stall_frontend_pct"] = stall_fe / cycles if cycles > 0 else 0.0
    d["stall_backend_pct"]  = stall_be / cycles if cycles > 0 else 0.0

    # Vector-specific metrics (NA for scalar variants)
    is_vector = "vector" in variant.lower() or "rvv" in variant.lower()
    if is_vector:
        d["vector_inst_pct"] = vec_inst / instret if instret > 0 else 0.0
        # vector_ls_ratio = vec_load / vec_store (loads per store)
        # Measures vector memory access pattern; high ratio = read-dominated kernel
        vec_load  = _get(row, "median_vec_load")
        vec_store = _get(row, "median_vec_store")
        if vec_store > 0:
            d["vector_ls_ratio"] = vec_load / vec_store
        elif vec_load > 0:
            d["vector_ls_ratio"] = float("inf")  # loads but no stores
        else:
            d["vector_ls_ratio"] = "NA"
        d["vpu_stall_pct"]   = vpu_stall / cycles if cycles > 0 else 0.0
    else:
        d["vector_inst_pct"] = "NA"
        d["vector_ls_ratio"] = "NA"
        d["vpu_stall_pct"]   = "NA"

    # DRAM bytes estimate: L1D misses × 64 bytes/cacheline (upper bound proxy)
    d["dram_bytes_est"] = l1d_miss * 64

    # Arithmetic intensity (instructions per DRAM byte)
    if d["dram_bytes_est"] > 0:
        d["arith_intensity"] = instret / d["dram_bytes_est"]
    else:
        d["arith_intensity"] = float("inf")

    # CQV = IQR / median  (coefficient of quartile variation)
    # Falls back gracefully when quartile columns are absent (old CSVs)
    if cycles > 0 and iqr >= 0:
        d["cqv"] = iqr / cycles
    else:
        d["cqv"] = "NA"

    return d


def fmt(v, fmt_str: str = "") -> str:
    """Format a value; pass 'NA' strings through unchanged."""
    if v == "NA" or v == float("inf"):
        return "NA"
    if fmt_str:
        return format(v, fmt_str)
    return str(v)


def main() -> None:
    parser = argparse.ArgumentParser(description="Compute derived counter metrics")
    parser.add_argument("--input-dir", required=True, help="Directory with _latest.csv files")
    parser.add_argument("--variant", required=True, help="Variant name (e.g. scalar_zb)")
    parser.add_argument("--levels", default="128,192,256", help="Comma-separated levels")
    args = parser.parse_args()

    input_dir = Path(args.input_dir)
    if not input_dir.exists():
        print(f"ERROR: Input directory not found: {input_dir}", file=sys.stderr)
        sys.exit(1)

    variant = args.variant

    latest_files = sorted(input_dir.glob("*_latest.csv"))
    if not latest_files:
        print(f"  No _latest.csv files found in {input_dir}", file=sys.stderr)
        sys.exit(0)

    noisy_runs: list[str] = []

    for csv_path in latest_files:
        try:
            _header, rows = parse_binary_csv(str(csv_path))
        except Exception as e:
            print(f"  ⚠ Cannot parse {csv_path.name}: {e}", file=sys.stderr)
            continue

        if not rows:
            continue

        derived_lines = [
            "",
            "# --- derived metrics ---",
            DERIVED_HEADER,
        ]

        for row in rows:
            op = row.get("op", str(row.get(list(row.keys())[0], "?")))
            if not isinstance(op, str):
                op = str(op)

            d = compute_derived(row, variant)

            # CQV noise check
            cqv_val = d["cqv"]
            if cqv_val != "NA":
                try:
                    cqv_f = float(cqv_val)
                    if cqv_f > CQV_WARN_THRESHOLD:
                        msg = (
                            f"  ⚠ NOISY RUN: {csv_path.name} op={op} "
                            f"cqv={cqv_f:.4f} > {CQV_WARN_THRESHOLD} — consider re-running"
                        )
                        print(msg, file=sys.stderr)
                        noisy_runs.append(f"{csv_path.name}:{op} cqv={cqv_f:.4f}")
                except (ValueError, TypeError):
                    pass

            line = (
                f"{op},"
                f"{fmt(d['ipc'], '.4f')},"
                f"{fmt(d['l1d_miss_rate'], '.6f')},"
                f"{fmt(d['l1d_mpki'], '.3f')},"
                f"{fmt(d['dtlb_mpki'], '.3f')},"
                f"{fmt(d['br_mispred_pki'], '.3f')},"
                f"{fmt(d['stall_frontend_pct'], '.4f')},"
                f"{fmt(d['stall_backend_pct'], '.4f')},"
                f"{fmt(d['vector_inst_pct'], '.4f') if d['vector_inst_pct'] != 'NA' else 'NA'},"
                f"{d['vector_ls_ratio']},"
                f"{fmt(d['vpu_stall_pct'], '.4f') if d['vpu_stall_pct'] != 'NA' else 'NA'},"
                f"{fmt(d['dram_bytes_est'], '.0f')},"
                f"{fmt(d['arith_intensity'], '.2f') if d['arith_intensity'] != float('inf') else 'NA'},"
                f"{fmt(d['cqv'], '.4f') if d['cqv'] != 'NA' else 'NA'}"
            )
            derived_lines.append(line)

        # Read existing content, strip any old derived section, append new one
        original = csv_path.read_text()
        # Strip old derived section if present (handle with or without leading blank line)
        for marker in ["\n# --- derived metrics ---", "\n\n# --- derived metrics ---"]:
            if marker in original:
                original = original[: original.index(marker)]
                break

        with open(csv_path, "w") as f:
            f.write(original.rstrip("\n") + "\n")
            for line in derived_lines:
                f.write(line + "\n")

        print(f"  ✓ Derived metrics written to {csv_path.name}")

    if noisy_runs:
        print(
            f"\n  ⚠ {len(noisy_runs)} noisy run(s) flagged (cqv > {CQV_WARN_THRESHOLD}):",
            file=sys.stderr,
        )
        for r in noisy_runs:
            print(f"    {r}", file=sys.stderr)


if __name__ == "__main__":
    main()
