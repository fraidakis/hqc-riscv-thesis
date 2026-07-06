#!/usr/bin/env bash
# promote-slide.sh — Promote or restore a slide variant for the defense deck.
#
# Twin of docs/thesis/figures/promote-variants.sh, adapted for beamer sections.
#
# Convention (produced by @Skill — Slide Beautify & Variants):
#   base    : sections/<NN-name>.tex           (\input by main.tex)
#   variant : sections/<NN-name>_<suffix>.tex  (parked; semantic suffix, e.g.
#             _minimal, _withfig, _detailed, _textonly)
#
# Promotion swaps the variant's content INTO the base file (which main.tex
# already \input's), backing up the original as <base>.orig.tex. Because slide
# suffixes are semantic (not v1/v2), --variant takes the suffix and promotion is
# per-section (one base at a time), unlike the figure twin.
#
# Usage:
#   ./promote-slide.sh -s 10-results -v minimal   # promote 10-results_minimal
#   ./promote-slide.sh -s results   -v withfig    # fuzzy section match
#   ./promote-slide.sh -s 10-results --restore    # restore original from backup
#   ./promote-slide.sh -s 10-results --clean      # delete .orig.tex backup
#   ./promote-slide.sh -s 10-results --list       # list variants for the section
#   ./promote-slide.sh --list-all                 # list every section's variants

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # .../presentation
SECTIONS_DIR="${SCRIPT_DIR}/sections"

SECTION=""
VARIANT=""
RESTORE=false
CLEAN=false
LIST=false
LIST_ALL=false

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Promote a slide variant into its base file, or restore the original.

OPTIONS:
  -s, --section NAME   Section base name or fuzzy fragment (e.g. 10-results, results)
  -v, --variant SUF    Variant suffix to promote (e.g. minimal, withfig, detailed)
  -r, --restore        Restore the base from its .orig.tex backup
      --clean          Delete the .orig.tex backup (make promotion permanent)
  -l, --list           List variants + backup status for the section
      --list-all       List variants for every section
  -h, --help           Show this help

EXAMPLES:
  $(basename "$0") -s 10-results -v minimal
  $(basename "$0") -s 10-results -r
  $(basename "$0") --list-all
EOF
    exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -s|--section) SECTION="$2"; shift 2 ;;
        -v|--variant) VARIANT="$2"; shift 2 ;;
        -r|--restore) RESTORE=true; shift ;;
        --clean)      CLEAN=true; shift ;;
        -l|--list)    LIST=true; shift ;;
        --list-all)   LIST_ALL=true; shift ;;
        -h|--help)    usage 0 ;;
        *)            echo "Error: unknown option '$1'"; usage 1 ;;
    esac
done

# --- list-all mode (no section needed) --------------------------------------
if [[ "$LIST_ALL" == true ]]; then
    for f in "$SECTIONS_DIR"/*.tex; do
        [[ -e "$f" ]] || continue
        b="$(basename "$f" .tex)"
        [[ "$b" == *_* ]] && continue
        vs=()
        for vf in "${SECTIONS_DIR}/${b}"_*.tex; do
            [[ -e "$vf" ]] || continue
            vs+=("$(basename "$vf" .tex | sed "s/^${b}_//")")
        done
        [[ ${#vs[@]} -eq 0 ]] && continue
        marker=""; [[ -e "${SECTIONS_DIR}/${b}.orig.tex" ]] && marker="  [PROMOTED]"
        echo "${b}${marker}: ${vs[*]}"
    done
    exit 0
fi

# --- resolve section --------------------------------------------------------
if [[ -z "$SECTION" ]]; then echo "Error: --section is required."; usage 1; fi

resolve_section() {
    local input="${1%.tex}"
    if [[ -f "${SECTIONS_DIR}/${input}.tex" ]] && [[ "$input" != *_* ]]; then
        echo "$input"; return 0
    fi
    local matches=()
    for f in "$SECTIONS_DIR"/*.tex; do
        [[ -e "$f" ]] || continue
        local b; b="$(basename "$f" .tex)"
        [[ "$b" == *_* ]] && continue
        if echo "$b" | grep -qi "$input"; then matches+=("$b"); fi
    done
    if [[ ${#matches[@]} -eq 1 ]]; then echo "${matches[0]}"; return 0
    elif [[ ${#matches[@]} -gt 1 ]]; then
        echo "Error: ambiguous section '$input'. Matches:" >&2
        printf '  %s\n' "${matches[@]}" >&2
    fi
    return 1
}

BASE=$(resolve_section "$SECTION") || { echo "Error: could not resolve section '$SECTION'."; \
    echo "Available:"; ls -1 "$SECTIONS_DIR"/*.tex 2>/dev/null | xargs -I{} basename {} .tex \
    | grep -v '_' | sed 's/^/  /'; exit 1; }
TARGET="${SECTIONS_DIR}/${BASE}.tex"
BACKUP="${SECTIONS_DIR}/${BASE}.orig.tex"

# --- list mode --------------------------------------------------------------
if [[ "$LIST" == true ]]; then
    echo "Variants for ${BASE}:"
    found=false
    for vf in "${SECTIONS_DIR}/${BASE}"_*.tex; do
        [[ -e "$vf" ]] || continue
        found=true
        echo "  ${BASE}.tex  <-  $(basename "$vf")"
    done
    [[ "$found" == false ]] && echo "  (none)"
    [[ -e "$BACKUP" ]] && echo "" && echo "⚠️  Active backup: $(basename "$BACKUP") (a variant is currently promoted)."
    exit 0
fi

# --- restore mode -----------------------------------------------------------
if [[ "$RESTORE" == true ]]; then
    if [[ ! -e "$BACKUP" ]]; then echo "Nothing to restore — no ${BASE}.orig.tex backup."; exit 0; fi
    mv "$BACKUP" "$TARGET"
    echo "✓ Restored ${BASE}.tex from backup."
    exit 0
fi

# --- clean mode -------------------------------------------------------------
if [[ "$CLEAN" == true ]]; then
    if [[ ! -e "$BACKUP" ]]; then echo "Nothing to clean — no ${BASE}.orig.tex backup."; exit 0; fi
    rm "$BACKUP"
    echo "✗ Deleted backup ${BASE}.orig.tex. Promotion is now permanent."
    exit 0
fi

# --- promote mode -----------------------------------------------------------
if [[ -z "$VARIANT" ]]; then echo "Error: either --variant or --restore/--clean/--list is required."; usage 1; fi

VFILE="${SECTIONS_DIR}/${BASE}_${VARIANT}.tex"
if [[ ! -e "$VFILE" ]]; then
    echo "Error: variant file not found: $(basename "$VFILE")"
    echo "Available variants for ${BASE}:"
    ls "${SECTIONS_DIR}/${BASE}"_*.tex 2>/dev/null | xargs -I{} basename {} | sed 's/^/  /'
    exit 1
fi

if [[ -e "$BACKUP" ]]; then
    echo "⚠️  A backup already exists (${BASE}.orig.tex): a variant is already promoted."
    echo "   Run '--restore' first, or '--clean' to make the current one permanent."
    exit 1
fi

cp "$TARGET" "$BACKUP"
cp "$VFILE" "$TARGET"
echo "✓ Promoted ${BASE}.tex  <-  $(basename "$VFILE")"
echo "  Original backed up as ${BASE}.orig.tex"
echo "  Restore with: $(basename "$0") -s ${BASE} --restore"
