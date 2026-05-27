# Literature Notes (Auto-Generated)

Markdown conversions of academic PDFs for AI agent consumption.

## Generation

```bash
# Convert all Chapter 4 referenced PDFs:
.venv/bin/python scripts/literature/convert_pdf.py --chapter4

# Convert a single PDF:
.venv/bin/python scripts/literature/convert_pdf.py "docs/literature/RISC-V/paper.pdf"

# Convert all PDFs under docs/literature/:
.venv/bin/python scripts/literature/convert_pdf.py --all

# Dry-run (show what would be converted):
.venv/bin/python scripts/literature/convert_pdf.py --chapter4 --dry-run
```

## Tool

Uses [PyMuPDF4LLM](https://pypi.org/project/pymupdf4llm/) for fast, reliable
conversion of digitally-born academic PDFs. No GPU required, ~3-5s per paper.

## Output Format

Each `.md` file has a YAML frontmatter header with source path and conversion
metadata, followed by the paper content with preserved headings, tables, and
references.

## Usage by AI Agent

These files are referenced in `docs/thesis-helper/reference_map.md` under
Chapter 4 (Related Work). The agent reads them directly when writing §4.x
sections of the thesis.
