#!/usr/bin/env python3
"""
Plot: Spike Instret Estimation vs Real Hardware Speedup
karatsuba_zbc over ref_scalar — End-to-end KEM (KeyGen + Encaps + Decaps)
"""

import matplotlib.pyplot as plt
import numpy as np

# ── Data ─────────────────────────────────────────────────────────────────────
levels = ['HQC-128', 'HQC-192', 'HQC-256']
spike_speedup = [34.01, 45.36, 49.11]
hw_speedup = [34.61, 46.15, 59.14]

x = np.arange(len(levels))
bar_width = 0.32

# ── Style ────────────────────────────────────────────────────────────────────
plt.style.use('seaborn-v0_8-whitegrid')
fig, ax = plt.subplots(figsize=(9, 5.5))

# Colors — academic-friendly palette
color_spike = '#4C72B0'   # steel blue
color_hw = '#DD8452'      # warm orange

# ── Bars ─────────────────────────────────────────────────────────────────────
bars_spike = ax.bar(x - bar_width/2, spike_speedup, bar_width,
                    label='Spike ISS (instret)', color=color_spike,
                    edgecolor='white', linewidth=0.8, zorder=3)
bars_hw = ax.bar(x + bar_width/2, hw_speedup, bar_width,
                 label='Banana Pi BPI-F3 (cycles)', color=color_hw,
                 edgecolor='white', linewidth=0.8, zorder=3)

# ── Value labels on bars ─────────────────────────────────────────────────────
for bar, val in zip(bars_spike, spike_speedup):
    ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.8,
            f'×{val:.1f}', ha='center', va='bottom', fontsize=10,
            fontweight='bold', color=color_spike)

for bar, val in zip(bars_hw, hw_speedup):
    ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.8,
            f'×{val:.1f}', ha='center', va='bottom', fontsize=10,
            fontweight='bold', color=color_hw)



# ── Axes ─────────────────────────────────────────────────────────────────────
ax.set_xlabel('Security Level', fontsize=11, labelpad=8)
ax.set_ylabel('Speedup over ref_scalar (×)', fontsize=11, labelpad=8)
ax.set_title('End-to-End KEM Speedup: karatsuba_zbc vs ref_scalar\n'
             'Spike Instret Estimation vs Real Hardware (SpacemiT X60)',
             fontsize=12, fontweight='bold', pad=12)
ax.set_xticks(x)
ax.set_xticklabels(levels, fontsize=11)
ax.set_ylim(0, max(hw_speedup) * 1.25)
ax.yaxis.set_major_locator(plt.MultipleLocator(10))

# ── Legend ────────────────────────────────────────────────────────────────────
ax.legend(loc='upper left', fontsize=10, framealpha=0.9)

# ── Grid ─────────────────────────────────────────────────────────────────────
ax.grid(axis='y', alpha=0.4, zorder=0)
ax.grid(axis='x', visible=False)
ax.set_axisbelow(True)

# ── Tight layout & save ──────────────────────────────────────────────────────
plt.tight_layout()
outpath = 'results/reports/figures/speedup_cross_platform.pdf'
plt.savefig(outpath, dpi=300, bbox_inches='tight')
outpath_png = 'results/reports/figures/speedup_cross_platform.png'
plt.savefig(outpath_png, dpi=200, bbox_inches='tight')
print(f'Saved: {outpath}')
print(f'Saved: {outpath_png}')
plt.close()
