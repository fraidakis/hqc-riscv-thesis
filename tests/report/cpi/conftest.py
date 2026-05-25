"""Shared Hypothesis strategies for CPI model property-based tests."""

import string

from hypothesis import settings
from hypothesis import strategies as st

# Configure default Hypothesis settings for CPI tests
settings.register_profile("cpi", max_examples=100)
settings.load_profile("cpi")

# ---------------------------------------------------------------------------
# Primitive strategies
# ---------------------------------------------------------------------------

# RISC-V-like mnemonic: lowercase letters optionally followed by a dot suffix
# Examples: "add", "vadd.vv", "vle64.v", "clmul"
_MNEMONIC_BASE = st.text(
    alphabet=string.ascii_lowercase, min_size=2, max_size=8
)
_MNEMONIC_SUFFIX = st.one_of(
    st.just(""),
    st.builds(lambda s: f".{s}", st.text(alphabet=string.ascii_lowercase, min_size=1, max_size=3)),
)


@st.composite
def riscv_mnemonics(draw: st.DrawFn) -> str:
    """Generate a plausible RISC-V mnemonic string."""
    base = draw(_MNEMONIC_BASE)
    suffix = draw(_MNEMONIC_SUFFIX)
    return base + suffix


# Valid instruction class names: uppercase identifiers like "ALU_1CYCLE", "VECTOR_MEM"
_CLASS_NAME = st.from_regex(r"[A-Z][A-Z0-9_]{2,15}", fullmatch=True)

# Cycle cost in the valid range [1, 500]
_COST = st.integers(min_value=1, max_value=500)

# Positive instruction count
_POSITIVE_COUNT = st.integers(min_value=1, max_value=10_000_000)


# ---------------------------------------------------------------------------
# Cost table strategies
# ---------------------------------------------------------------------------


@st.composite
def valid_regex_pattern(draw: st.DrawFn) -> str:
    """Generate a valid regex pattern suitable for matching mnemonics.

    Produces patterns like: "^add$", "^vadd\\..*", "^clmul.*"
    """
    # Use literal mnemonic bases to avoid generating invalid regex
    base = draw(st.text(alphabet=string.ascii_lowercase, min_size=2, max_size=6))
    style = draw(st.sampled_from(["exact", "prefix", "dot_suffix"]))
    if style == "exact":
        return f"^{base}$"
    elif style == "prefix":
        return f"^{base}.*"
    else:
        return f"^{base}\\..*"


@st.composite
def instruction_class_entry(draw: st.DrawFn) -> dict:
    """Generate a single valid cost table entry (InstructionClass-like dict)."""
    name = draw(_CLASS_NAME)
    pattern = draw(valid_regex_pattern())
    cost = draw(_COST)
    return {"name": name, "pattern": pattern, "cost": cost}


@st.composite
def valid_cost_table_dict(draw: st.DrawFn) -> dict:
    """Generate a valid cost table JSON structure.

    Returns a dict with:
      - "name": str
      - "description": str
      - "default_cost": int in [1, 500]
      - "classes": list of instruction class entries (1-15 entries)
    """
    name = draw(st.text(alphabet=string.ascii_lowercase + "-_", min_size=3, max_size=20))
    description = draw(st.text(min_size=5, max_size=50))
    default_cost = draw(_COST)
    classes = draw(st.lists(instruction_class_entry(), min_size=1, max_size=15))

    # Ensure unique class names
    seen_names: set[str] = set()
    unique_classes = []
    for cls in classes:
        if cls["name"] not in seen_names:
            seen_names.add(cls["name"])
            unique_classes.append(cls)

    if not unique_classes:
        # Fallback: guarantee at least one entry
        unique_classes = [draw(instruction_class_entry())]

    return {
        "name": name,
        "description": description,
        "default_cost": default_cost,
        "classes": unique_classes,
    }


# ---------------------------------------------------------------------------
# Instruction mix strategies
# ---------------------------------------------------------------------------


@st.composite
def instruction_mix(draw: st.DrawFn, min_mnemonics: int = 1, max_mnemonics: int = 30) -> dict[str, int]:
    """Generate a valid instruction mix: dict mapping mnemonic -> positive count.

    The mnemonics are RISC-V-like strings with positive integer counts.
    """
    mnemonics = draw(
        st.lists(
            riscv_mnemonics(),
            min_size=min_mnemonics,
            max_size=max_mnemonics,
            unique=True,
        )
    )
    mix: dict[str, int] = {}
    for mnemonic in mnemonics:
        count = draw(_POSITIVE_COUNT)
        mix[mnemonic] = count
    return mix


# ---------------------------------------------------------------------------
# Function-instruction mapping strategies
# ---------------------------------------------------------------------------

_FUNCTION_NAME = st.from_regex(r"[a-z_][a-z0-9_]{2,30}", fullmatch=True)


@st.composite
def function_instruction_mapping(
    draw: st.DrawFn, min_functions: int = 1, max_functions: int = 10
) -> dict[str, dict[str, int]]:
    """Generate a mapping of function names to instruction mixes.

    Returns: dict[function_name, dict[mnemonic, count]]
    Each function has its own instruction mix (subset of RISC-V-like mnemonics).
    """
    func_names = draw(
        st.lists(
            _FUNCTION_NAME,
            min_size=min_functions,
            max_size=max_functions,
            unique=True,
        )
    )
    mapping: dict[str, dict[str, int]] = {}
    for fname in func_names:
        mix = draw(instruction_mix(min_mnemonics=1, max_mnemonics=15))
        mapping[fname] = mix
    return mapping
