#!/usr/bin/env python3
"""Convert academic PDF papers to Markdown for AI agent consumption.

Uses PyMuPDF4LLM for fast, reliable conversion of digitally-born academic PDFs.
Output mirrors the source structure:
  docs/references/pdf/<category>/paper.pdf
    → docs/references/markdown/<category>/paper-slug.md

Usage:
    # Convert a single PDF:
    python3 scripts/literature/convert_pdf.py "docs/references/pdf/riscv/paper.pdf"

    # Convert all PDFs under docs/references/pdf/:
    python3 scripts/literature/convert_pdf.py --all

    # Convert all PDFs referenced in Chapter 4 of reference_map.md:
    python3 scripts/literature/convert_pdf.py --chapter4

    # Dry-run (show what would be converted):
    python3 scripts/literature/convert_pdf.py --chapter4 --dry-run
"""

import argparse
import re
import sys
import time
from pathlib import Path

# Project root
ROOT = Path(__file__).resolve().parents[2]
PDF_DIR = ROOT / "docs" / "references" / "pdf"
MARKDOWN_DIR = ROOT / "docs" / "references" / "markdown"


def slugify(name: str) -> str:
    """Convert a filename to a clean slug for the output .md file."""
    # Remove leading ! characters (priority markers)
    name = name.lstrip("!")
    # Remove file extension
    name = Path(name).stem
    # Replace spaces and special chars with hyphens
    name = re.sub(r"[^\w\s-]", "", name)
    name = re.sub(r"[\s_]+", "-", name.strip())
    name = re.sub(r"-+", "-", name)
    return name.lower()[:80]


def find_all_pdfs() -> list[Path]:
    """Find all PDF files under docs/references/pdf/."""
    return sorted(PDF_DIR.rglob("*.pdf"))


def output_path_for(pdf_path: Path) -> Path:
    """Compute the mirrored markdown output path for a given PDF.

    docs/references/pdf/<category>/paper.pdf
      → docs/references/markdown/<category>/<slug>.md

    Falls back to MARKDOWN_DIR root for PDFs not under PDF_DIR.
    """
    try:
        rel = pdf_path.relative_to(PDF_DIR)
        # rel is e.g. riscv/paper.pdf  →  keep the category subdir
        out_dir = MARKDOWN_DIR / rel.parent
    except ValueError:
        out_dir = MARKDOWN_DIR
    return out_dir / f"{slugify(pdf_path.name)}.md"


def find_chapter4_pdfs() -> list[Path]:
    """Find PDFs referenced in Chapter 4 of reference_map.md."""
    ref_map = ROOT / "docs" / "thesis-helper" / "reference_map.md"
    if not ref_map.exists():
        print(f"ERROR: {ref_map} not found", file=sys.stderr)
        sys.exit(1)

    content = ref_map.read_text()
    # Extract section between "## Κεφάλαιο 4" and the next "---"
    match = re.search(
        r"## Κεφάλαιο 4.*?(?=\n---\n)", content, re.DOTALL
    )
    if not match:
        print("ERROR: Could not find Chapter 4 section in reference_map.md", file=sys.stderr)
        sys.exit(1)

    section = match.group(0)
    # Find all .pdf paths
    pdf_paths = re.findall(r"`([^`]+\.pdf)`", section)
    result = []
    for p in pdf_paths:
        full = ROOT / p
        if full.exists():
            result.append(full)
        else:
            print(f"  WARNING: {p} not found, skipping", file=sys.stderr)
    return result


def already_converted(pdf_path: Path) -> bool:
    """Return True if the markdown output for this PDF already exists."""
    return output_path_for(pdf_path).exists()


def convert_single(pdf_path: Path, output_dir: Path | None = None) -> Path | None:
    """Convert a single PDF to Markdown using PyMuPDF4LLM.

    Fast and reliable for digitally-born academic PDFs.
    Preserves headings, tables (as markdown), and text structure.
    If output_dir is None, the mirrored path under docs/references/markdown/ is used.
    """
    import pymupdf4llm

    output_file = output_path_for(pdf_path) if output_dir is None else output_dir / f"{slugify(pdf_path.name)}.md"

    print(f"  Converting: {pdf_path.name}")
    print(f"  Output:     {output_file.relative_to(ROOT)}")

    start = time.time()

    try:
        md_content = pymupdf4llm.to_markdown(str(pdf_path))
    except Exception as e:
        print(f"  ERROR: Conversion failed: {e}", file=sys.stderr)
        return None

    elapsed = time.time() - start

    # Build output with metadata header
    header = f"""---
source_pdf: {pdf_path.relative_to(ROOT)}
converted_at: {time.strftime("%Y-%m-%d %H:%M")}
conversion_time: {elapsed:.1f}s
tool: pymupdf4llm
---

"""
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(header + md_content)

    print(f"  Done in {elapsed:.1f}s ({len(md_content):,} chars)")
    return output_file


def main():
    parser = argparse.ArgumentParser(
        description="Convert academic PDFs to Markdown for AI agent consumption"
    )
    parser.add_argument("pdf", nargs="?", help="Path to a single PDF file to convert")
    parser.add_argument("--all", action="store_true", help="Convert all PDFs under docs/references/pdf/")
    parser.add_argument("--chapter4", action="store_true", help="Convert only Chapter 4 referenced PDFs")
    parser.add_argument("--output-dir", type=Path, default=None, help="Override output directory (default: mirrors pdf/ structure under markdown/)")
    parser.add_argument("--force", action="store_true", help="Re-convert even if markdown output already exists")
    parser.add_argument("--dry-run", action="store_true", help="Show what would be converted without doing it")
    args = parser.parse_args()

    if not any([args.pdf, args.all, args.chapter4]):
        parser.print_help()
        sys.exit(1)

    if args.pdf:
        pdf_path = Path(args.pdf)
        if not pdf_path.is_absolute():
            pdf_path = ROOT / pdf_path
        if not pdf_path.exists():
            print(f"ERROR: {pdf_path} not found", file=sys.stderr)
            sys.exit(1)
        pdfs = [pdf_path]
    elif args.chapter4:
        pdfs = find_chapter4_pdfs()
    else:
        pdfs = find_all_pdfs()

    # Filter already-converted unless --force
    if not args.force and not args.dry_run:
        pending = [p for p in pdfs if not already_converted(p)]
        skipped = len(pdfs) - len(pending)
        if skipped:
            print(f"Skipping {skipped} already-converted PDF(s) (use --force to re-convert)")
        pdfs = pending

    print(f"Found {len(pdfs)} PDF(s) to convert")
    print(f"Output root: {MARKDOWN_DIR.relative_to(ROOT)}/")
    print()

    if args.dry_run:
        for pdf in pdfs:
            out = output_path_for(pdf) if args.output_dir is None else args.output_dir / f"{slugify(pdf.name)}.md"
            print(f"  {pdf.relative_to(ROOT)}")
            print(f"    → {out.relative_to(ROOT)}")
        return

    results = []
    for pdf in pdfs:
        out = convert_single(pdf, args.output_dir)
        if out:
            results.append(out)
        print()

    print(f"Successfully converted {len(results)}/{len(pdfs)} files")
    if results:
        print(f"Output in: {MARKDOWN_DIR.relative_to(ROOT)}/")


if __name__ == "__main__":
    main()
