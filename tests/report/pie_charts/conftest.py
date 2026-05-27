"""Shared Hypothesis strategies for pie chart generator property-based tests."""

import string

from hypothesis import settings
from hypothesis import strategies as st

from scripts.report.generate_pie_charts import CATEGORY_MAP, CATEGORY_COLORS, TraceEntry

# Configure default Hypothesis settings for pie chart tests
settings.register_profile("pie_charts", max_examples=100)
settings.load_profile("pie_charts")

# ---------------------------------------------------------------------------
# Primitive strategies
# ---------------------------------------------------------------------------

# Positive instruction count (realistic range for trace data)
_POSITIVE_INSTRUCTIONS = st.integers(min_value=1, max_value=100_000_000)

# Percentage value (0.0 to 100.0)
_PERCENTAGE = st.floats(min_value=0.0, max_value=100.0, allow_nan=False, allow_infinity=False)

# Generic identifier-like function name (lowercase with underscores)
_GENERIC_FUNCTION_NAME = st.from_regex(r"[a-z][a-z0-9_]{2,30}", fullmatch=True)


# ---------------------------------------------------------------------------
# CSV content strategies
# ---------------------------------------------------------------------------


@st.composite
def csv_preamble_lines(draw: st.DrawFn, min_lines: int = 0, max_lines: int = 10) -> list[str]:
    """Generate preamble lines (each starting with '#').

    Produces comment lines like '# some text' but avoids generating
    '# variant:' lines (those are handled separately).
    """
    n = draw(st.integers(min_value=min_lines, max_value=max_lines))
    lines: list[str] = []
    for _ in range(n):
        # Generate comment text that does NOT start with 'variant:'
        text = draw(
            st.text(
                alphabet=string.ascii_letters + string.digits + " _-",
                min_size=1,
                max_size=40,
            ).filter(lambda t: not t.strip().startswith("variant:"))
        )
        lines.append(f"# {text}")
    return lines


@st.composite
def variant_name_strategy(draw: st.DrawFn) -> str:
    """Generate a variant name string (e.g., 'karatsuba_zbc', 'ref_scalar')."""
    return draw(
        st.from_regex(r"[a-z][a-z0-9_]{2,20}", fullmatch=True)
    )


@st.composite
def csv_data_row(draw: st.DrawFn) -> tuple[str, int, float]:
    """Generate a single CSV data row as (function_name, instructions, percentage)."""
    func_name = draw(_GENERIC_FUNCTION_NAME)
    instructions = draw(_POSITIVE_INSTRUCTIONS)
    percentage = draw(_PERCENTAGE)
    return (func_name, instructions, percentage)


@st.composite
def csv_content_strategy(
    draw: st.DrawFn,
    min_data_rows: int = 1,
    max_data_rows: int = 20,
    include_variant: bool = True,
) -> tuple[str, str, int]:
    """Generate complete CSV text with preamble and data rows.

    Returns a tuple of (csv_text, variant_name, num_data_rows).

    The CSV text consists of:
    - N preamble lines starting with '#'
    - A '# variant: X' line (if include_variant is True)
    - M data rows as 'function,instructions,percentage'
    """
    # Generate preamble
    preamble = draw(csv_preamble_lines(min_lines=0, max_lines=5))

    # Generate variant line
    variant = ""
    if include_variant:
        variant = draw(variant_name_strategy())
        # Insert variant line at a random position in the preamble
        insert_pos = draw(st.integers(min_value=0, max_value=len(preamble)))
        preamble.insert(insert_pos, f"# variant: {variant}")

    # Generate data rows
    num_rows = draw(st.integers(min_value=min_data_rows, max_value=max_data_rows))
    data_rows: list[str] = []
    for _ in range(num_rows):
        func_name, instructions, percentage = draw(csv_data_row())
        data_rows.append(f"{func_name},{instructions},{percentage:.1f}")

    # Combine into full CSV text
    all_lines = preamble + data_rows
    csv_text = "\n".join(all_lines)

    return (csv_text, variant, num_rows)


# ---------------------------------------------------------------------------
# Function name strategies
# ---------------------------------------------------------------------------


@st.composite
def matching_function_name(draw: st.DrawFn) -> tuple[str, str]:
    """Generate a function name that matches a pattern in CATEGORY_MAP.

    Returns (function_name, expected_category).
    The function name contains one of the CATEGORY_MAP keys as a substring.
    """
    # Pick a random pattern from CATEGORY_MAP
    pattern = draw(st.sampled_from(list(CATEGORY_MAP.keys())))
    category = CATEGORY_MAP[pattern]

    # Generate a prefix and suffix to wrap around the pattern
    prefix = draw(
        st.text(alphabet=string.ascii_lowercase + "_", min_size=0, max_size=10)
        .filter(lambda p: not any(k in (p + pattern) for k in CATEGORY_MAP if k != pattern and CATEGORY_MAP[k] != category))
    )
    suffix = draw(
        st.text(alphabet=string.ascii_lowercase + string.digits + "_", min_size=0, max_size=10)
        .filter(lambda s: not any(k in (pattern + s) for k in CATEGORY_MAP if k != pattern and CATEGORY_MAP[k] != category))
    )

    func_name = prefix + pattern + suffix
    return (func_name, category)


@st.composite
def non_matching_function_name(draw: st.DrawFn) -> str:
    """Generate a function name that does NOT match any pattern in CATEGORY_MAP.

    These names will be classified as 'Other'.
    """
    # Use a restricted alphabet and filter out any matches
    name = draw(
        st.from_regex(r"[a-z][a-z0-9]{2,20}", fullmatch=True)
        .filter(lambda n: not any(pattern in n for pattern in CATEGORY_MAP))
    )
    return name


@st.composite
def function_name_strategy(draw: st.DrawFn) -> tuple[str, str]:
    """Generate a function name with its expected category.

    Returns (function_name, expected_category).
    Randomly chooses between matching and non-matching names.
    """
    use_matching = draw(st.booleans())
    if use_matching:
        return draw(matching_function_name())
    else:
        name = draw(non_matching_function_name())
        return (name, "Other")


# ---------------------------------------------------------------------------
# TraceEntry list strategies
# ---------------------------------------------------------------------------


@st.composite
def trace_entry_strategy(draw: st.DrawFn) -> TraceEntry:
    """Generate a single TraceEntry with a positive instruction count."""
    func_name = draw(_GENERIC_FUNCTION_NAME)
    instructions = draw(_POSITIVE_INSTRUCTIONS)
    percentage = draw(_PERCENTAGE)
    return TraceEntry(function=func_name, instructions=instructions, percentage=percentage)


@st.composite
def trace_entry_list_strategy(
    draw: st.DrawFn,
    min_entries: int = 1,
    max_entries: int = 30,
) -> list[TraceEntry]:
    """Generate a list of TraceEntry objects with positive instruction counts.

    All entries have instructions >= 1, suitable for testing categorization
    and aggregation logic.
    """
    entries = draw(
        st.lists(
            trace_entry_strategy(),
            min_size=min_entries,
            max_size=max_entries,
        )
    )
    return entries
