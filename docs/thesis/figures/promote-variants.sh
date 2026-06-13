#!/usr/bin/env bash
# promote-variants.sh — Promote or restore figure variants for a thesis chapter.
#
# Usage:
#   ./promote-variants.sh --chapter ch1-introduction --variant v1   # promote all _v1 variants
#   ./promote-variants.sh --chapter ch01 --variant v1               # fuzzy: matches ch1-introduction
#   ./promote-variants.sh --chapter ch1-introduction --restore      # restore originals
#   ./promote-variants.sh -c ch1-introduction --clean               # delete .orig.tex backups
#   ./promote-variants.sh -c ch2-background -v v2                   # short flags
#   ./promote-variants.sh -c ch1-introduction -r                    # short restore
#
# The script operates inside docs/thesis/figures/<chapter>/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ─── Defaults ────────────────────────────────────────────────────────────────
CHAPTER=""
VARIANT=""
RESTORE=false

# ─── Usage ───────────────────────────────────────────────────────────────────
usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Promote figure variants or restore originals for a thesis chapter.

OPTIONS:
  -c, --chapter CHAPTER   Chapter folder name or prefix (e.g. ch1, ch01, ch2-background)
  -v, --variant VERSION   Variant to promote (e.g. v1, v2, v3)
  -r, --restore           Restore all figures from .orig.tex backups
      --clean             Delete .orig.tex backups (use after confirming promoted variant is good)
  -l, --list              List available variants for the chapter
  -h, --help              Show this help

EXAMPLES:
  $(basename "$0") -c ch01 -v v1                  # fuzzy match → ch1-introduction
  $(basename "$0") -c ch1-introduction -v v1      # promote all v1 variants
  $(basename "$0") -c ch1-introduction -r         # restore originals
  $(basename "$0") -c ch1 --clean                 # delete backups permanently
  $(basename "$0") -c ch2-background -l           # list variants
EOF
    exit "${1:-0}"
}

# ─── Parse args ──────────────────────────────────────────────────────────────
LIST=false
CLEAN=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        -c|--chapter) CHAPTER="$2"; shift 2 ;;
        -v|--variant) VARIANT="$2"; shift 2 ;;
        -r|--restore) RESTORE=true; shift ;;
        --clean)      CLEAN=true; shift ;;
        -l|--list)    LIST=true; shift ;;
        -h|--help)    usage 0 ;;
        *)            echo "Error: unknown option '$1'"; usage 1 ;;
    esac
done

# ─── Validate & resolve chapter ──────────────────────────────────────────────
if [[ -z "$CHAPTER" ]]; then
    echo "Error: --chapter is required."
    usage 1
fi

# Fuzzy chapter resolution: accept "ch1", "ch01", "ch1-intro", full name, etc.
resolve_chapter() {
    local input="$1"
    # Exact match first
    if [[ -d "${SCRIPT_DIR}/${input}" ]]; then
        echo "$input"
        return 0
    fi
    # Normalize: strip leading "ch0?" prefix to just the digit(s)
    # e.g. "ch01" → "1", "ch1" → "1", "ch2-background" → already tried exact
    local num=""
    if [[ "$input" =~ ^ch0*([0-9]+)(.*)$ ]]; then
        num="${BASH_REMATCH[1]}"
        local suffix="${BASH_REMATCH[2]}"
    fi
    # Try glob match with the chapter number
    if [[ -n "$num" ]]; then
        local matches=()
        for d in "$SCRIPT_DIR"/ch"${num}"-*/; do
            [[ -d "$d" ]] && matches+=("$(basename "$d")")
        done
        # Also try ch0N- pattern (e.g. ch01-introduction if dirs use zero-padded)
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
    # No match
    return 1
}

RESOLVED=$(resolve_chapter "$CHAPTER") || {
    echo "Error: could not resolve chapter '$CHAPTER'."
    echo "Available chapters:"
    ls -1d "$SCRIPT_DIR"/ch*/ 2>/dev/null | xargs -I{} basename {} | sed 's/^/  /'
    exit 1
}
CHAPTER="$RESOLVED"
FIGURES_DIR="${SCRIPT_DIR}/${CHAPTER}"

# ─── List mode ───────────────────────────────────────────────────────────────
if [[ "$LIST" == true ]]; then
    echo "Variants in $CHAPTER:"
    echo ""
    # Find all base figures that have variants
    for base in "$FIGURES_DIR"/*_v[0-9]*.tex; do
        [[ -e "$base" ]] || continue
        basename "$base"
    done | sort | while read -r vfile; do
        # Extract base name and version
        base_name="${vfile%%_v[0-9]*}"
        version="${vfile#${base_name}_}"
        version="${version%.tex}"
        echo "  ${base_name}.tex  ←  ${vfile}  (${version})"
    done
    echo ""
    # Check for active backups
    backups=("$FIGURES_DIR"/*.orig.tex)
    if [[ -e "${backups[0]}" ]]; then
        echo "⚠️  Active backups found (variants currently promoted):"
        for b in "$FIGURES_DIR"/*.orig.tex; do
            echo "  $(basename "$b")"
        done
    fi
    exit 0
fi

# ─── Restore mode ───────────────────────────────────────────────────────────
if [[ "$RESTORE" == true ]]; then
    restored=0
    for orig in "$FIGURES_DIR"/*.orig.tex; do
        [[ -e "$orig" ]] || continue
        target="${orig%.orig.tex}.tex"
        mv "$orig" "$target"
        echo "✓ Restored: $(basename "$target")"
        restored=$((restored + 1))
    done
    if [[ $restored -eq 0 ]]; then
        echo "Nothing to restore — no .orig.tex backups found in $CHAPTER."
    else
        echo ""
        echo "Done: $restored figure(s) restored to original."
    fi
    exit 0
fi

# ─── Clean mode (delete backups) ────────────────────────────────────────────
if [[ "$CLEAN" == true ]]; then
    cleaned=0
    for orig in "$FIGURES_DIR"/*.orig.tex; do
        [[ -e "$orig" ]] || continue
        rm "$orig"
        echo "✗ Deleted backup: $(basename "$orig")"
        cleaned=$((cleaned + 1))
    done
    if [[ $cleaned -eq 0 ]]; then
        echo "Nothing to clean — no .orig.tex backups found in $CHAPTER."
    else
        echo ""
        echo "Done: $cleaned backup(s) deleted. Promoted variants are now permanent."
    fi
    exit 0
fi

# ─── Promote mode ───────────────────────────────────────────────────────────
if [[ -z "$VARIANT" ]]; then
    echo "Error: either --variant or --restore is required."
    usage 1
fi

# Validate variant format
if [[ ! "$VARIANT" =~ ^v[0-9]+$ ]]; then
    echo "Error: variant must be in format 'v1', 'v2', etc. Got: '$VARIANT'"
    exit 1
fi

promoted=0
skipped=0
errors=0

# Find all variant files matching the requested version
for variant_file in "$FIGURES_DIR"/*_"${VARIANT}".tex; do
    [[ -e "$variant_file" ]] || continue

    # Derive the base filename: timeline-pqc_v1.tex → timeline-pqc.tex
    variant_basename="$(basename "$variant_file")"
    base_name="${variant_basename%_${VARIANT}.tex}"
    target="${FIGURES_DIR}/${base_name}.tex"

    if [[ ! -e "$target" ]]; then
        echo "⚠️  Skip: no base file for $(basename "$variant_file")"
        skipped=$((skipped + 1))
        continue
    fi

    # Backup (only if not already backed up)
    backup="${FIGURES_DIR}/${base_name}.orig.tex"
    if [[ -e "$backup" ]]; then
        echo "⚠️  Backup already exists for ${base_name}.tex (previous promote active)"
        echo "   Run --restore first, or manually remove ${base_name}.orig.tex"
        skipped=$((skipped + 1))
        continue
    fi

    cp "$target" "$backup"
    cp "$variant_file" "$target"
    echo "✓ Promoted: ${base_name}.tex ← ${variant_basename}"
    promoted=$((promoted + 1))
done

if [[ $promoted -eq 0 && $skipped -eq 0 ]]; then
    echo "No ${VARIANT} variants found in $CHAPTER."
    echo "Available variant files:"
    ls "$FIGURES_DIR"/*_v*.tex 2>/dev/null | xargs -I{} basename {} | sed 's/^/  /'
    exit 1
fi

echo ""
echo "Done: $promoted figure(s) promoted to ${VARIANT}, $skipped skipped."
echo "To restore originals: $(basename "$0") -c $CHAPTER --restore"
