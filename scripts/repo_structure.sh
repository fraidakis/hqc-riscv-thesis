#!/bin/bash

# Ignore everything starting with 'build' for the main tree
tree -L 10 -I '.git|node_modules|build*' > repo_tree.txt

# Loop through all folders starting with 'build' and append them with limited depth
for d in build*; do 
  if [ -d "$d" ]; then
    echo -e "\n├── $d" >> repo_tree.txt
    tree -L 2 "$d" | tail -n +2 >> repo_tree.txt
  fi
done