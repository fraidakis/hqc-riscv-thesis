#!/usr/bin/env python3
"""
scripts/report/compute_derived_counters.py

Reads per-region EXTENDED counter CSV output from bench binaries and computes
derived microarchitectural metrics:
  - L1D miss rate, L1D MPKI
  - dTLB MPKI
  - Branch misprediction rate (per 1000 instructions)
  - Stall frontend % (of cycles)
  - Stall backend % (of cycles)
  - For vector variants: vector_inst %, vector L/S ratio, VPU stall %
  - DRAM bytes estimate (L2_load_miss * 64) and arithmetic intensity

Appends a 'derived' section to each _latest.csv in the input directory.

Usage:
    python3 compute_derived_counters.py --input-dir <path> --variant <name> --levels 128,192,256
"""

import argparse
import csv
import os
import sys
from pathlib import Path


def parse_binary_csv(filepath: str) -> dict:
    """Parse a counter CSV file from the bench binary output.
    
    Returns dict keyed by operation name, with sub-dict of metric values.
    """
    results = {}
    header = None
    
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('#'):
                continue
            if header is None:
                header = [h.strip() for h in line.split(',')]
                continue
            parts = [p.strip() for p in line.split(',')]
            if len(parts) < 2:
                continue
            op = parts[0]
            row = {}
            for i, h in enumerate(header):
                if i < len(parts):
                    try:
                        # Try numeric conversion
                        val = parts[i].rstrip('%')
                        row[h] = float(val)
                    except ValueError:
                        row[h] = parts[i]
            results[op] = row
    
    return results


def compute_derived(row: dict, variant: str) -> dict:
    """Compute derived metrics from a single operation's counter row."""
    derived = {}
    
    cycles = row.get('median_cycles', 0)
    instret = row.get('median_instret', 0)
    l1d_miss = row.get('median_l1d_miss', row.get('median_l1d_load_miss', 0))
    l1d_access = row.get('median_l1d_access', 0)
    dtlb_miss = row.get('median_dtlb_miss', row.get('median_dtlb_load_miss', 0))
    br_mispred = row.get('median_br_mispred', 0)
    stall_fe = row.get('median_stall_fe', row.get('median_stall_frontend', 0))
    stall_be = row.get('median_stall_be', row.get('median_stall_backend', 0))
    vec_inst = row.get('median_vec_inst', row.get('median_vector_inst', 0))
    vpu_stall = row.get('median_vpu_stall', 0)
    
    # IPC
    if cycles > 0:
        derived['ipc'] = instret / cycles
    else:
        derived['ipc'] = 0.0
    
    # L1D miss rate
    if l1d_access > 0:
        derived['l1d_miss_rate'] = l1d_miss / l1d_access
    else:
        derived['l1d_miss_rate'] = 0.0
    
    # L1D MPKI (misses per 1000 instructions)
    if instret > 0:
        derived['l1d_mpki'] = (l1d_miss / instret) * 1000.0
    else:
        derived['l1d_mpki'] = 0.0
    
    # dTLB MPKI
    if instret > 0:
        derived['dtlb_mpki'] = (dtlb_miss / instret) * 1000.0
    else:
        derived['dtlb_mpki'] = 0.0
    
    # Branch misprediction rate (per 1000 instructions)
    if instret > 0:
        derived['br_mispred_pki'] = (br_mispred / instret) * 1000.0
    else:
        derived['br_mispred_pki'] = 0.0
    
    # Stall percentages (top-down lite)
    if cycles > 0:
        derived['stall_frontend_pct'] = stall_fe / cycles
        derived['stall_backend_pct'] = stall_be / cycles
    else:
        derived['stall_frontend_pct'] = 0.0
        derived['stall_backend_pct'] = 0.0
    
    # Vector-specific metrics
    if 'vector' in variant or 'rvv' in variant:
        if instret > 0:
            derived['vector_inst_pct'] = vec_inst / instret
        else:
            derived['vector_inst_pct'] = 0.0
        
        if vpu_stall > 0 and cycles > 0:
            derived['vpu_stall_pct'] = vpu_stall / cycles
        else:
            derived['vpu_stall_pct'] = 0.0
    
    # DRAM bytes estimate (L2 miss approximation — use L1D miss as proxy
    # since we don't always have L2 separately)
    # L2_load_miss ≈ L1D_miss that also miss L2 (heuristic: ~30% of L1D misses)
    # For now, use l1d_miss * 64 as upper-bound DRAM traffic estimate
    derived['dram_bytes_est'] = l1d_miss * 64
    
    # Arithmetic intensity (instructions per DRAM byte)
    if derived['dram_bytes_est'] > 0:
        derived['arith_intensity'] = instret / derived['dram_bytes_est']
    else:
        derived['arith_intensity'] = float('inf')
    
    return derived


def main():
    parser = argparse.ArgumentParser(description='Compute derived counter metrics')
    parser.add_argument('--input-dir', required=True, help='Directory with _latest.csv files')
    parser.add_argument('--variant', required=True, help='Variant name')
    parser.add_argument('--levels', default='128,192,256', help='Comma-separated levels')
    args = parser.parse_args()
    
    input_dir = Path(args.input_dir)
    if not input_dir.exists():
        print(f"ERROR: Input directory not found: {input_dir}", file=sys.stderr)
        sys.exit(1)
    
    levels = [l.strip() for l in args.levels.split(',')]
    variant = args.variant
    
    # Find all _latest.csv files in the input directory
    latest_files = sorted(input_dir.glob('*_latest.csv'))
    
    if not latest_files:
        print(f"  No _latest.csv files found in {input_dir}", file=sys.stderr)
        sys.exit(0)
    
    # Process each file
    for csv_path in latest_files:
        try:
            data = parse_binary_csv(str(csv_path))
        except Exception as e:
            print(f"  ⚠ Cannot parse {csv_path.name}: {e}", file=sys.stderr)
            continue
        
        if not data:
            continue
        
        # Compute derived metrics for each operation in the file
        derived_lines = []
        derived_lines.append("# --- derived metrics ---")
        derived_lines.append("op,ipc,l1d_miss_rate,l1d_mpki,dtlb_mpki,br_mispred_pki,"
                           "stall_frontend_pct,stall_backend_pct,"
                           "dram_bytes_est,arith_intensity")
        
        for op, row in data.items():
            if not isinstance(row, dict):
                continue
            d = compute_derived(row, variant)
            
            line = (f"{op},{d['ipc']:.4f},{d['l1d_miss_rate']:.6f},"
                   f"{d['l1d_mpki']:.3f},{d['dtlb_mpki']:.3f},{d['br_mispred_pki']:.3f},"
                   f"{d['stall_frontend_pct']:.4f},{d['stall_backend_pct']:.4f},"
                   f"{d['dram_bytes_est']:.0f},{d['arith_intensity']:.2f}")
            
            # Add vector columns if present
            if 'vector_inst_pct' in d:
                line += f",{d['vector_inst_pct']:.4f},{d.get('vpu_stall_pct', 0):.4f}"
            
            derived_lines.append(line)
        
        # Append derived metrics to the CSV file
        with open(csv_path, 'a') as f:
            f.write('\n')
            for line in derived_lines:
                f.write(line + '\n')
        
        print(f"  ✓ Derived metrics appended to {csv_path.name}")


if __name__ == '__main__':
    main()
