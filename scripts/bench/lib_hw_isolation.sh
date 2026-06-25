#!/usr/bin/env bash
# scripts/bench/lib_hw_isolation.sh
#
# Hardware measurement isolation helpers for BPI-F3.
# Provides IRQ migration, ASLR control, and continuous freq/temp monitoring.
#
# Usage: source this file, then call:
#   hw_isolation_setup <core> [--isolate]
#   hw_isolation_teardown
#   hw_isolation_start_monitor <core>
#   hw_isolation_stop_monitor <output_csv>
#
# All state changes are snapshot-and-restored on teardown.
# Never leaves the board in a modified state after a run.

# ── Configuration ─────────────────────────────────────────────────────────────
HW_ISOLATION_CORE="${HW_ISOLATION_CORE:-7}"
HW_ISOLATION_FREQ_NOMINAL_KHZ="${HW_ISOLATION_FREQ_NOMINAL_KHZ:-1600000}"
HW_ISOLATION_FREQ_THRESHOLD_KHZ="${HW_ISOLATION_FREQ_THRESHOLD_KHZ:-1550000}"
HW_ISOLATION_SAMPLE_INTERVAL_MS="${HW_ISOLATION_SAMPLE_INTERVAL_MS:-50}"

# ── Internal state ────────────────────────────────────────────────────────────
_hw_iso_irq_snapshot_file=""
_hw_iso_irq_migrated=0
_hw_iso_aslr_method=""   # "setarch" or "global"
_hw_iso_aslr_orig=""
_hw_iso_monitor_pid=""
_hw_iso_monitor_tmpfile=""
_hw_iso_active=0

# ══════════════════════════════════════════════════════════════════════════════
# IRQ affinity migration: move all migratable IRQs off the measurement core
# ══════════════════════════════════════════════════════════════════════════════

_hw_iso_snapshot_irq_affinity() {
    local snapshot_file="$1"
    > "$snapshot_file"
    for f in /proc/irq/*/smp_affinity_list; do
        [[ -r "$f" ]] || continue
        local irq_dir="${f%/smp_affinity_list}"
        local irq_num="${irq_dir##*/}"
        local current
        current="$(cat "$f" 2>/dev/null)" || continue
        echo "${irq_num}:${current}" >> "$snapshot_file"
    done
}

_hw_iso_migrate_irqs_off_core() {
    local core="$1"
    local migrated=0
    local failed=0

    for f in /proc/irq/*/smp_affinity_list; do
        [[ -w "$f" ]] || continue
        local current
        current="$(cat "$f" 2>/dev/null)" || continue

        # Check if this IRQ is currently allowed on our core
        if ! echo "$current" | grep -qE "(^|,)${core}(,|$|-)" 2>/dev/null; then
            # Parse range format: "0-7" includes core 7
            if ! echo "$current" | grep -qE "([0-9]+)-([0-9]+)" 2>/dev/null; then
                continue
            fi
            # Check if core falls within any range
            local in_range=0
            while IFS= read -r range; do
                if [[ "$range" =~ ^([0-9]+)-([0-9]+)$ ]]; then
                    local lo="${BASH_REMATCH[1]}" hi="${BASH_REMATCH[2]}"
                    if [[ "$core" -ge "$lo" && "$core" -le "$hi" ]]; then
                        in_range=1
                        break
                    fi
                elif [[ "$range" == "$core" ]]; then
                    in_range=1
                    break
                fi
            done <<< "$(echo "$current" | tr ',' '\n')"
            [[ "$in_range" -eq 0 ]] && continue
        fi

        # Build new affinity excluding our core
        # Simple approach: set to 0-<core-1> (cores 0..6 for core=7)
        local new_list="0-$((core - 1))"
        if echo "$new_list" > "$f" 2>/dev/null; then
            migrated=$((migrated + 1))
        else
            failed=$((failed + 1))
        fi
    done

    echo "  IRQ isolation: migrated ${migrated} IRQs off core ${core} (${failed} non-migratable, skipped)" >&2
}

_hw_iso_restore_irq_affinity() {
    local snapshot_file="$1"
    [[ -f "$snapshot_file" ]] || return 0

    local restored=0
    while IFS=: read -r irq_num affinity; do
        local target="/proc/irq/${irq_num}/smp_affinity_list"
        if [[ -w "$target" ]]; then
            echo "$affinity" > "$target" 2>/dev/null && restored=$((restored + 1))
        fi
    done < "$snapshot_file"

    echo "  IRQ restore: restored ${restored} IRQ affinities" >&2
    rm -f "$snapshot_file"
}

# ══════════════════════════════════════════════════════════════════════════════
# ASLR control: disable per-process via setarch -R
# ══════════════════════════════════════════════════════════════════════════════

_hw_iso_aslr_exec_prefix() {
    # Returns the exec prefix component for ASLR disable.
    # Prefer setarch -R (per-process, no global state change).
    if command -v setarch >/dev/null 2>&1; then
        echo "setarch $(uname -m) -R"
    else
        echo ""
    fi
}

_hw_iso_aslr_disable_global() {
    # Fallback: globally disable ASLR (snapshot + restore)
    _hw_iso_aslr_orig="$(cat /proc/sys/kernel/randomize_va_space 2>/dev/null)"
    echo 0 > /proc/sys/kernel/randomize_va_space 2>/dev/null
    _hw_iso_aslr_method="global"
}

_hw_iso_aslr_restore_global() {
    if [[ "$_hw_iso_aslr_method" == "global" && -n "$_hw_iso_aslr_orig" ]]; then
        echo "$_hw_iso_aslr_orig" > /proc/sys/kernel/randomize_va_space 2>/dev/null
    fi
}

# ══════════════════════════════════════════════════════════════════════════════
# Continuous frequency + temperature monitor
# ══════════════════════════════════════════════════════════════════════════════

hw_isolation_start_monitor() {
    local core="${1:-$HW_ISOLATION_CORE}"
    local interval_ms="${HW_ISOLATION_SAMPLE_INTERVAL_MS}"

    _hw_iso_monitor_tmpfile="$(mktemp /tmp/hw_freq_temp_XXXXXX.csv)"

    # Find the thermal zone (usually thermal_zone0 on BPI-F3)
    local thermal_path=""
    for tz in /sys/class/thermal/thermal_zone*/temp; do
        [[ -r "$tz" ]] && { thermal_path="$tz"; break; }
    done

    local freq_path="/sys/devices/system/cpu/cpu${core}/cpufreq/scaling_cur_freq"

    # Launch sampler on a non-measurement core (core 0)
    (
        echo "timestamp_ms,freq_khz,temp_mc" > "$_hw_iso_monitor_tmpfile"
        local t0
        t0="$(date +%s%N)"
        while true; do
            local now_ns freq temp
            now_ns="$(date +%s%N)"
            local elapsed_ms=$(( (now_ns - t0) / 1000000 ))
            freq="$(cat "$freq_path" 2>/dev/null || echo 0)"
            temp="$(cat "$thermal_path" 2>/dev/null || echo 0)"
            echo "${elapsed_ms},${freq},${temp}" >> "$_hw_iso_monitor_tmpfile"
            # Sleep interval (using fractional seconds)
            sleep "$(awk "BEGIN{printf \"%.3f\", ${interval_ms}/1000.0}")"
        done
    ) &
    _hw_iso_monitor_pid=$!

    # Pin monitor to non-measurement core
    taskset -p -c 0 "$_hw_iso_monitor_pid" >/dev/null 2>&1 || true
}

hw_isolation_stop_monitor() {
    local output_summary="${1:-}"

    if [[ -n "$_hw_iso_monitor_pid" ]]; then
        kill "$_hw_iso_monitor_pid" 2>/dev/null || true
        wait "$_hw_iso_monitor_pid" 2>/dev/null || true
        _hw_iso_monitor_pid=""
    fi

    if [[ ! -f "$_hw_iso_monitor_tmpfile" ]]; then
        echo ""
        return
    fi

    # Compute summary statistics from the monitor data
    local result
    result="$(awk -F',' '
    NR == 1 { next }  # skip header
    {
        freq = $2 + 0
        temp = $3 + 0
        n++
        if (n == 1 || freq < freq_min) freq_min = freq
        if (n == 1 || freq > freq_max) freq_max = freq
        if (n == 1 || temp > temp_max) temp_max = temp
        freq_arr[n] = freq
        freq_sum += freq
    }
    END {
        if (n == 0) { print "n_freq_samples=0,throttled=0"; exit }
        # median (simple: sort array)
        for (i = 1; i <= n; i++)
            for (j = i+1; j <= n; j++)
                if (freq_arr[i] > freq_arr[j]) {
                    t = freq_arr[i]; freq_arr[i] = freq_arr[j]; freq_arr[j] = t
                }
        if (n % 2 == 1) freq_med = freq_arr[int(n/2)+1]
        else freq_med = (freq_arr[n/2] + freq_arr[n/2+1]) / 2

        # Convert to MHz
        freq_min_mhz = freq_min / 1000
        freq_med_mhz = freq_med / 1000
        freq_max_mhz = freq_max / 1000
        temp_max_c = temp_max / 1000.0

        # Throttle detection
        throttled = 0
        threshold = '"${HW_ISOLATION_FREQ_THRESHOLD_KHZ}"'
        if (freq_min < threshold) throttled = 1

        printf "freq_min_mhz=%.0f,freq_median_mhz=%.0f,freq_max_mhz=%.0f,temp_max_c=%.1f,n_freq_samples=%d,throttled=%d\n", \
            freq_min_mhz, freq_med_mhz, freq_max_mhz, temp_max_c, n, throttled
    }' "$_hw_iso_monitor_tmpfile")"

    # Check for throttling and warn
    if echo "$result" | grep -q "throttled=1"; then
        echo "⚠ WARNING: THROTTLING DETECTED during measurement! $result" >&2
    fi

    # Output the summary
    echo "$result"

    # Store to output file if requested
    if [[ -n "$output_summary" ]]; then
        echo "$result" > "$output_summary"
    fi

    rm -f "$_hw_iso_monitor_tmpfile"
    _hw_iso_monitor_tmpfile=""
}

# ══════════════════════════════════════════════════════════════════════════════
# Public API: setup / teardown / exec prefix
# ══════════════════════════════════════════════════════════════════════════════

# hw_isolation_setup <core> [--no-irq]
# Sets up measurement isolation. Call before benchmark execution.
hw_isolation_setup() {
    local core="${1:-$HW_ISOLATION_CORE}"
    local skip_irq=0

    shift || true
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --no-irq) skip_irq=1; shift ;;
            *) shift ;;
        esac
    done

    _hw_iso_active=1
    HW_ISOLATION_CORE="$core"

    # 1. Snapshot + migrate IRQs
    if [[ "$skip_irq" -eq 0 ]]; then
        _hw_iso_irq_snapshot_file="$(mktemp /tmp/hw_irq_snapshot_XXXXXX.txt)"
        _hw_iso_snapshot_irq_affinity "$_hw_iso_irq_snapshot_file"
        _hw_iso_migrate_irqs_off_core "$core"
        _hw_iso_irq_migrated=1
    fi

    # 2. ASLR: prefer setarch (per-process), note method for provenance
    if command -v setarch >/dev/null 2>&1; then
        _hw_iso_aslr_method="setarch"
    else
        _hw_iso_aslr_disable_global
    fi
}

# hw_isolation_teardown
# Restores all system state modified by setup.
hw_isolation_teardown() {
    [[ "$_hw_iso_active" -eq 0 ]] && return 0

    # Stop monitor if still running
    if [[ -n "$_hw_iso_monitor_pid" ]]; then
        hw_isolation_stop_monitor >/dev/null 2>&1
    fi

    # Restore IRQ affinity
    if [[ "$_hw_iso_irq_migrated" -eq 1 && -n "$_hw_iso_irq_snapshot_file" ]]; then
        _hw_iso_restore_irq_affinity "$_hw_iso_irq_snapshot_file"
        _hw_iso_irq_migrated=0
    fi

    # Restore ASLR if globally changed
    _hw_iso_aslr_restore_global

    _hw_iso_active=0
}

# hw_isolation_exec_prefix <core>
# Returns the full exec prefix string for a benchmark invocation.
# Includes: sudo nice, taskset, setarch -R (ASLR disable).
hw_isolation_exec_prefix() {
    local core="${1:-$HW_ISOLATION_CORE}"
    local prefix="sudo nice -n -20 taskset -c ${core}"

    # Add setarch -R for ASLR disable (per-process)
    if [[ "$_hw_iso_aslr_method" == "setarch" ]]; then
        prefix="sudo nice -n -20 taskset -c ${core} setarch $(uname -m) -R"
    fi

    echo "$prefix"
}

# hw_isolation_exec_prefix_unprivileged <core>
# Same but without sudo (for dev mode).
hw_isolation_exec_prefix_unprivileged() {
    local core="${1:-$HW_ISOLATION_CORE}"
    local prefix="taskset -c ${core}"

    if [[ "$_hw_iso_aslr_method" == "setarch" ]]; then
        prefix="taskset -c ${core} setarch $(uname -m) -R"
    fi

    echo "$prefix"
}

# hw_isolation_provenance_comments
# Returns CSV comment lines describing what isolation was applied.
hw_isolation_provenance_comments() {
    local irq_status="off"
    local aslr_status="off"
    local isolcpus_status="off"

    if [[ "$_hw_iso_irq_migrated" -eq 1 ]]; then
        irq_status="on"
    fi
    if [[ -n "$_hw_iso_aslr_method" ]]; then
        aslr_status="off (via ${_hw_iso_aslr_method})"
    fi
    # Check kernel cmdline for isolcpus
    if grep -q "isolcpus=" /proc/cmdline 2>/dev/null; then
        isolcpus_status="on"
    fi

    echo "# isolation_irq: ${irq_status}"
    echo "# isolation_aslr: ${aslr_status}"
    echo "# isolation_isolcpus: ${isolcpus_status}"
    echo "# isolation_core: ${HW_ISOLATION_CORE}"
}

# ══════════════════════════════════════════════════════════════════════════════
# isolcpus boot configuration helper (opt-in, requires reboot)
# ══════════════════════════════════════════════════════════════════════════════

hw_isolation_print_isolcpus_instructions() {
    local core="${1:-7}"
    cat >&2 <<EOF

════════════════════════════════════════════════════════════════════════════════
  OPTIONAL: Enable kernel-level CPU isolation (requires reboot)
════════════════════════════════════════════════════════════════════════════════

  Add to kernel command line (extlinux.conf or uEnv.txt):
    isolcpus=${core} nohz_full=${core} rcu_nocbs=${core}

  On BPI-F3 (Bianbu 3.0):
    sudo sed -i 's/\(append.*\)/\1 isolcpus=${core} nohz_full=${core} rcu_nocbs=${core}/' /boot/extlinux/extlinux.conf
    sudo reboot

  To revert:
    sudo sed -i 's/ isolcpus=${core} nohz_full=${core} rcu_nocbs=${core}//' /boot/extlinux/extlinux.conf
    sudo reboot

  NOTE: This is NOT required for normal benchmarking. IRQ migration alone
  provides significant noise reduction. Only use isolcpus if you need
  kernel-level scheduler isolation for sub-0.5% CQV targets.
════════════════════════════════════════════════════════════════════════════════
EOF
}

# Ensure teardown runs on script exit
trap hw_isolation_teardown EXIT INT TERM
