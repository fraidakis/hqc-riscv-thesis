#!/usr/bin/env bash
# compare-variants.sh — Build all figure variants for a chapter/appendix and
#                        open them in the default browser for visual comparison.
#
# Usage:
#   ./compare-variants.sh -c ch1-introduction              # all figures + variants
#   ./compare-variants.sh -c ch01                          # fuzzy match
#   ./compare-variants.sh -c ch1-introduction -f timeline  # only figures matching "timeline"
#   ./compare-variants.sh -c ch1-introduction -j 4         # 4 parallel build jobs
#   ./compare-variants.sh -c appendix-algebra              # appendix figures
#   ./compare-variants.sh --all                            # all chapters + appendices
#
# The script:
#   1. Discovers all base + variant .tex figure files in the chapter
#   2. Compiles each as a standalone PDF (parallel, reusing thesis preamble)
#   3. Opens one Chrome window per figure with base + variants as tabs

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
THESIS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${THESIS_DIR}/build/variant-compare"

# ─── Defaults ────────────────────────────────────────────────────────────────
CHAPTER=""
FILTER=""
JOBS="${JOBS:-6}"
ALL_CHAPTERS=false

# ─── Usage ───────────────────────────────────────────────────────────────────
usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Build all figure variants for a chapter/appendix and open them in Chrome for comparison.
Each base figure gets its own window with base + variants as tabs.

OPTIONS:
  -c, --chapter CHAPTER   Chapter or appendix folder name or prefix
                          (e.g. ch1, ch01, ch2-background, appendix-algebra)
  -f, --filter PATTERN    Only build figures whose name matches PATTERN (grep -i)
  -j, --jobs N            Number of parallel lualatex jobs (default: $JOBS)
      --all               Build variants for ALL chapters and appendices
  -h, --help              Show this help

EXAMPLES:
  $(basename "$0") -c ch01                          # all ch1 figures
  $(basename "$0") -c ch1-introduction -f timeline  # only timeline figures
  $(basename "$0") -c ch6-polymul -j 8              # 8 parallel jobs
  $(basename "$0") -c appendix-algebra              # appendix figures
  $(basename "$0") --all                            # every chapter + appendix
EOF
    exit "${1:-0}"
}

# ─── Parse args ──────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        -c|--chapter) CHAPTER="$2"; shift 2 ;;
        -f|--filter)  FILTER="$2"; shift 2 ;;
        -j|--jobs)    JOBS="$2"; shift 2 ;;
        --all)        ALL_CHAPTERS=true; shift ;;
        -h|--help)    usage 0 ;;
        *)            echo "Error: unknown option '$1'"; usage 1 ;;
    esac
done

# ─── Resolve chapter/appendix (reused fuzzy logic from promote-variants.sh) ───
resolve_chapter() {
    local input="$1"
    # Exact match
    if [[ -d "${SCRIPT_DIR}/${input}" ]]; then
        echo "$input"
        return 0
    fi
    # Chapter fuzzy match: ch1 -> ch1-introduction, ch01 -> ch1-introduction
    local num=""
    if [[ "$input" =~ ^ch0*([0-9]+)(.*)$ ]]; then
        num="${BASH_REMATCH[1]}"
    fi
    if [[ -n "$num" ]]; then
        local matches=()
        for d in "$SCRIPT_DIR"/ch"${num}"-*/; do
            [[ -d "$d" ]] && matches+=("$(basename "$d")")
        done
        if [[ ${#matches[@]} -eq 0 ]]; then
            for d in "$SCRIPT_DIR"/ch0"${num}"-*/; do
                [[ -d "$d" ]] && matches+=("$(basename "$d")")
            done
        fi
        if [[ ${#matches[@]} -eq 1 ]]; then
            echo "${matches[0]}"
            return 0
        elif [[ ${#matches[@]} -gt 1 ]]; then
            echo "Error: ambiguous chapter '$input'. Matches:" >&2
            printf '  %s\n' "${matches[@]}" >&2
            return 1
        fi
    fi
    # Appendix fuzzy match: appendix-alg -> appendix-algebra
    if [[ "$input" =~ ^appendix ]]; then
        local matches=()
        for d in "$SCRIPT_DIR"/${input}*/; do
            [[ -d "$d" ]] && matches+=("$(basename "$d")")
        done
        if [[ ${#matches[@]} -eq 1 ]]; then
            echo "${matches[0]}"
            return 0
        elif [[ ${#matches[@]} -gt 1 ]]; then
            echo "Error: ambiguous appendix '$input'. Matches:" >&2
            printf '  %s\n' "${matches[@]}" >&2
            return 1
        fi
    fi
    return 1
}

# ─── Determine chapters to process ──────────────────────────────────────────
CHAPTERS=()
if [[ "$ALL_CHAPTERS" == true ]]; then
    for d in "$SCRIPT_DIR"/ch*/ "$SCRIPT_DIR"/appendix-*/; do
        [[ -d "$d" ]] && CHAPTERS+=("$(basename "$d")")
    done
elif [[ -n "$CHAPTER" ]]; then
    RESOLVED=$(resolve_chapter "$CHAPTER") || {
        echo "Error: could not resolve chapter/appendix '$CHAPTER'."
        echo "Available:"
        ls -1d "$SCRIPT_DIR"/ch*/ "$SCRIPT_DIR"/appendix-*/ 2>/dev/null | xargs -I{} basename {} | sed 's/^/  /'
        exit 1
    }
    CHAPTERS+=("$RESOLVED")
else
    echo "Error: --chapter or --all is required."
    usage 1
fi

# ─── Create standalone wrapper template ──────────────────────────────────────
mkdir -p "$BUILD_DIR"

# This wrapper loads the thesis preamble (fonts, colors, TikZ styles) but
# completely neutralizes TikZ externalization so figures render inline.
# Strategy: We hook into the TeX jobname mechanism. packages.tex already
# checks \jobname to decide whether to call \nofiles. We use a different
# approach: after loading packages.tex (which activates externalization),
# we forcefully disable it and make \tikzsetnextfilename a no-op.
WRAPPER_TEMPLATE="$BUILD_DIR/_standalone-wrapper.tex"
cat > "$WRAPPER_TEMPLATE" << 'LATEX'
% Auto-generated standalone wrapper for figure variant comparison
\documentclass[a4paper,12pt]{scrbook}

% Load the full thesis preamble (this activates tikz/external)
\input{preamble/packages}
\input{preamble/settings}
\input{figures/thesis-tikz-styles}
\input{preamble/macros}

% === NEUTRALIZE EXTERNALIZATION after packages loaded ===
% Disable export for ALL figures — they render inline into the PDF.
\tikzset{external/export=false}
% Also disable the "only named" filter (irrelevant when export=false)
\tikzset{external/only named=false}
% \tikzsetnextfilename normally registers a figure for externalization;
% make it harmless.
\renewcommand{\tikzsetnextfilename}[1]{}

% Suppress glossary warnings
\makenoidxglossaries

% Suppress hyperref link generation (standalone, no cross-refs)
\hypersetup{draft=true}

\begin{document}
\pagestyle{empty}
\INPUT_FIGURE
\end{document}
LATEX

# ─── Collect figure files ────────────────────────────────────────────────────
declare -a FIGURE_FILES=()

for ch in "${CHAPTERS[@]}"; do
    FIGURES_DIR="${SCRIPT_DIR}/${ch}"
    for f in "$FIGURES_DIR"/*.tex; do
        [[ -e "$f" ]] || continue
        fname="$(basename "$f")"
        # Skip non-figure files (e.g. README, style files)
        [[ "$fname" == thesis-tikz-styles.tex ]] && continue
        # Apply filter if set
        if [[ -n "$FILTER" ]]; then
            echo "$fname" | grep -qi "$FILTER" || continue
        fi
        FIGURE_FILES+=("$f")
    done
done

if [[ ${#FIGURE_FILES[@]} -eq 0 ]]; then
    echo "No figure files found."
    [[ -n "$FILTER" ]] && echo "  (filter: '$FILTER')"
    exit 1
fi

echo "Found ${#FIGURE_FILES[@]} figure file(s) to build."

# ─── Build each figure as standalone PDF (parallel via xargs) ─────────────────
# Create a build script for a single figure
BUILD_ONE="$BUILD_DIR/_build-one.sh"
cat > "$BUILD_ONE" << 'SCRIPT'
#!/usr/bin/env bash
# Build a single figure standalone — called by xargs in parallel.
# Do NOT use set -e: lualatex may return non-zero on warnings but still
# produce a valid PDF.
FIGURE_TEX="$1"
BUILD_DIR="$2"
THESIS_DIR="$3"

fname="$(basename "$FIGURE_TEX" .tex)"
chapter="$(basename "$(dirname "$FIGURE_TEX")")"
out_name="${chapter}--${fname}"
out_dir="${BUILD_DIR}/${out_name}"
mkdir -p "$out_dir"

# Create the per-figure wrapper by substituting the figure input path
rel_path="figures/${chapter}/${fname}"
sed "s|\\\\INPUT_FIGURE|\\\\input{${rel_path}}|" \
    "${BUILD_DIR}/_standalone-wrapper.tex" > "${out_dir}/wrapper.tex"

# Compile from the thesis directory so all \input paths resolve.
# CRITICAL: -jobname=main ensures tikz/external recognizes this as the
# "main document" run (not a figure subprocess), so export=false works.
# Output PDF is therefore main.pdf (not wrapper.pdf).
cd "$THESIS_DIR"
lualatex -shell-escape -interaction=batchmode -jobname=main \
    -output-directory="${out_dir}" \
    "${out_dir}/wrapper.tex" > /dev/null 2>&1 || true

if [[ -f "${out_dir}/main.pdf" ]]; then
    # Check if the PDF has actual pages (not "no pages of output")
    if command -v pdfinfo >/dev/null 2>&1; then
        pages=$(pdfinfo "${out_dir}/main.pdf" 2>/dev/null | grep "^Pages:" | awk '{print $2}')
        if [[ "${pages:-0}" -ge 1 ]]; then
            echo "✓ ${chapter}/${fname}.tex"
            exit 0
        fi
    else
        # No pdfinfo: check file size (empty PDFs are <1KB)
        size=$(stat --printf='%s' "${out_dir}/main.pdf" 2>/dev/null || echo 0)
        if [[ "$size" -gt 2000 ]]; then
            echo "✓ ${chapter}/${fname}.tex"
            exit 0
        fi
    fi
fi
echo "✗ ${chapter}/${fname}.tex (see ${out_dir}/main.log)"
exit 1
SCRIPT
chmod +x "$BUILD_ONE"

echo ""
echo "=== Building ${#FIGURE_FILES[@]} figures ($JOBS parallel jobs) ==="
echo ""

# Run builds in parallel (|| true: don't abort if some figures fail)
printf '%s\n' "${FIGURE_FILES[@]}" | \
    xargs -P "$JOBS" -I{} bash "$BUILD_ONE" {} "$BUILD_DIR" "$THESIS_DIR" || true

echo ""

# ─── Count results ──────────────────────────────────────────────────────────
# Only count PDFs for figures we actually built (not stale leftovers)
built=0
for f in "${FIGURE_FILES[@]}"; do
    fname="$(basename "$f" .tex)"
    chapter="$(basename "$(dirname "$f")")"
    out_dir="${BUILD_DIR}/${chapter}--${fname}"
    if [[ -f "${out_dir}/main.pdf" ]] && [[ "$(stat --printf='%s' "${out_dir}/main.pdf" 2>/dev/null || echo 0)" -gt 2000 ]]; then
        ((built++)) || true
    fi
done
total=${#FIGURE_FILES[@]}
echo "=== Build complete: ${built}/${total} figures rendered ==="
echo ""

# ─── Open in Chrome: one new window per figure, variants as tabs ─────────────
# Detect Chrome binary
CHROME=""
for candidate in google-chrome google-chrome-stable chromium-browser chromium; do
    if command -v "$candidate" >/dev/null 2>&1; then
        CHROME="$candidate"
        break
    fi
done
if [[ -z "$CHROME" ]]; then
    echo "Chrome not found. PDFs are in: ${BUILD_DIR}/"
    exit 0
fi

# Group built PDFs by base figure name and open each group in a new window.
# Convention: base = "foo-bar.tex", variants = "foo-bar_suffix.tex".
# A file is a "base" if a .tex without any _suffix exists (i.e., it has no
# underscore-separated variant tag). Variants are files that start with
# a known base name followed by "_".
for ch in "${CHAPTERS[@]}"; do
    FIGURES_DIR="${SCRIPT_DIR}/${ch}"

    # Step 1: Identify base figures (files with no _suffix that could be a variant)
    declare -A BASE_NAMES=()
    for f in "$FIGURES_DIR"/*.tex; do
        [[ -e "$f" ]] || continue
        fname="$(basename "$f" .tex)"
        [[ "$fname" == thesis-tikz-styles ]] && continue
        if [[ -n "$FILTER" ]]; then
            echo "$fname" | grep -qi "$FILTER" || continue
        fi
        # A file is a base if there's no longer prefix that is also a file.
        # Simple heuristic: if removing _lastpart gives another existing .tex, it's a variant.
        # Otherwise it's a base.
        candidate="${fname%_*}"
        if [[ "$candidate" != "$fname" ]] && [[ -f "${FIGURES_DIR}/${candidate}.tex" ]]; then
            # This is a variant of $candidate — skip, it'll be collected later
            :
        else
            BASE_NAMES["$fname"]=1
        fi
    done

    for base in $(echo "${!BASE_NAMES[@]}" | tr ' ' '\n' | sort); do
        # Collect PDFs for this figure group (base + all _* variants)
        pdfs=()
        pdf_labels=()
        if [[ -f "${BUILD_DIR}/${ch}--${base}/main.pdf" ]]; then
            pdfs+=("${BUILD_DIR}/${ch}--${base}/main.pdf")
            pdf_labels+=("$base")
        fi
        for vf in "${FIGURES_DIR}/${base}"_*.tex; do
            [[ -e "$vf" ]] || continue
            vname="$(basename "$vf" .tex)"
            if [[ -n "$FILTER" ]]; then
                echo "$vname" | grep -qi "$FILTER" || continue
            fi
            if [[ -f "${BUILD_DIR}/${ch}--${vname}/main.pdf" ]]; then
                pdfs+=("${BUILD_DIR}/${ch}--${vname}/main.pdf")
                pdf_labels+=("$vname")
            fi
        done

        if [[ ${#pdfs[@]} -gt 0 ]]; then
            echo "Opening: ${base} (${#pdfs[@]} tab(s): ${pdf_labels[*]})"
            "$CHROME" --new-window "${pdfs[@]}" >/dev/null 2>&1 &
            sleep 0.3  # small delay so Chrome registers each window separately
        fi
    done
    unset BASE_NAMES
done

echo ""
echo "Done. Each figure group opened in its own Chrome window."
