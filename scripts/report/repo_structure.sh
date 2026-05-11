#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
OUT_FILE="${REPO_ROOT}/repo_tree.txt"

# Ignore everything starting with 'build' for the main tree.
tree -L 10 -I '.git|node_modules|build*' "${REPO_ROOT}" > "${OUT_FILE}"

# Loop through all folders starting with 'build' and append them with limited depth.
for d in "${REPO_ROOT}"/build*; do
  if [[ -d "$d" ]]; then
    d_name="$(basename "$d")"
    echo -e "\n├── ${d_name}" >> "${OUT_FILE}"
    tree -L 2 "$d" | tail -n +2 >> "${OUT_FILE}"
  fi
done