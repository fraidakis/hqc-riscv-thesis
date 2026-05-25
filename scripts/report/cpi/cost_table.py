"""CostTable module for instruction classification and cycle cost assignment.

Loads, validates, and queries cost tables mapping RISC-V instruction mnemonics
(via regex patterns) to their estimated cycle latencies on a target microarchitecture.
"""

from __future__ import annotations

import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class InstructionClass:
    """A single instruction class entry in the cost table."""

    name: str       # e.g., "VECTOR_CRYPTO"
    pattern: str    # regex pattern, e.g., "^vclmul.*"
    cost: int       # cycle latency, 1-500


class CostTable:
    """Ordered collection of instruction class patterns with cycle costs.

    Uses first-match regex semantics: the first pattern that matches a mnemonic
    determines the assigned class and cost.
    """

    def __init__(
        self,
        classes: list[InstructionClass],
        default_cost: int = 1,
        name: str = "",
        description: str = "",
    ) -> None:
        self.classes = classes
        self.default_cost = default_cost
        self.name = name
        self.description = description
        # Pre-compile regex patterns for performance
        self._compiled: list[tuple[InstructionClass, re.Pattern[str]]] = []
        for ic in classes:
            self._compiled.append((ic, re.compile(ic.pattern)))
        # Track unmatched mnemonics (unique set)
        self._unmatched: set[str] = set()

    @classmethod
    def from_json(cls, path: Path) -> CostTable:
        """Load and validate a cost table from a JSON file.

        Raises:
            FileNotFoundError: if path does not exist
            ValueError: if JSON is invalid or entries have non-positive-integer costs
        """
        path = Path(path)
        if not path.exists():
            raise FileNotFoundError(
                f"Cost table file not found: {path}"
            )

        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except json.JSONDecodeError as e:
            raise ValueError(
                f"Invalid JSON in cost table file '{path}': {e}"
            ) from e

        # Validate and extract fields
        name = data.get("name", "")
        description = data.get("description", "")
        default_cost = data.get("default_cost", 1)

        if not isinstance(default_cost, int) or default_cost < 1 or default_cost > 500:
            raise ValueError(
                f"Invalid default_cost '{default_cost}' in cost table: "
                f"must be a positive integer in [1, 500]"
            )

        classes_data = data.get("classes", [])
        if not isinstance(classes_data, list):
            raise ValueError("'classes' field must be a list")

        instruction_classes: list[InstructionClass] = []
        for entry in classes_data:
            entry_name = entry.get("name", "")
            entry_pattern = entry.get("pattern", "")
            entry_cost = entry.get("cost")

            # Validate cost is a positive integer in [1, 500]
            if not isinstance(entry_cost, int) or entry_cost < 1 or entry_cost > 500:
                raise ValueError(
                    f"Invalid cost for entry '{entry_name}': "
                    f"got '{entry_cost}', must be a positive integer in [1, 500]"
                )

            # Validate that pattern is a valid regex
            try:
                re.compile(entry_pattern)
            except re.error as e:
                raise ValueError(
                    f"Invalid regex pattern for entry '{entry_name}': "
                    f"'{entry_pattern}' — {e}"
                ) from e

            instruction_classes.append(
                InstructionClass(name=entry_name, pattern=entry_pattern, cost=entry_cost)
            )

        return cls(
            classes=instruction_classes,
            default_cost=default_cost,
            name=name,
            description=description,
        )

    @classmethod
    def default(cls) -> CostTable:
        """Return the built-in Rocket-like single-issue in-order cost table."""
        classes = [
            InstructionClass(
                name="ALU_1CYCLE",
                pattern=r"^(add|sub|and|or|xor|sll|srl|sra|lui|auipc|addi|andi|ori|xori|slli|srli|srai|mv|nop|li|not|neg|seqz|snez|sltz|sgtz)$",
                cost=1,
            ),
            InstructionClass(
                name="ALU_ZBB",
                pattern=r"^(andn|orn|xnor|clz|ctz|cpop|max|maxu|min|minu|sext|zext|rev8|orc\.b|rori?|roli?)$",
                cost=1,
            ),
            InstructionClass(
                name="BRANCH",
                pattern=r"^(beq|bne|blt|bge|bltu|bgeu|bnez|beqz|blez|bgez|bltz|bgtz|j|jal|jalr|jr|ret|call|tail)$",
                cost=1,
            ),
            InstructionClass(
                name="LOAD",
                pattern=r"^(lb|lh|lw|ld|lbu|lhu|lwu)$",
                cost=2,
            ),
            InstructionClass(
                name="STORE",
                pattern=r"^(sb|sh|sw|sd)$",
                cost=1,
            ),
            InstructionClass(
                name="MUL",
                pattern=r"^(mul|mulh|mulhu|mulhsu|mulw)$",
                cost=5,
            ),
            InstructionClass(
                name="DIV",
                pattern=r"^(div|divu|rem|remu|divw|divuw|remw|remuw)$",
                cost=34,
            ),
            InstructionClass(
                name="CLMUL",
                pattern=r"^(clmul|clmulh|clmulr)$",
                cost=3,
            ),
            InstructionClass(
                name="VECTOR_ALU",
                pattern=r"^(vadd|vsub|vand|vor|vxor|vsll|vsrl|vsra|vmin|vmax|vmseq|vmsne|vmsltu|vmslt|vmv|vnot|vneg)\..*",
                cost=2,
            ),
            InstructionClass(
                name="VECTOR_MUL",
                pattern=r"^(vmul|vmulh|vmulhu|vmacc|vnmsac|vmadd|vnmsub)\..*",
                cost=8,
            ),
            InstructionClass(
                name="VECTOR_MEM",
                pattern=r"^(vle|vse|vlse|vsse|vluxei|vsuxei|vlseg|vsseg).*",
                cost=4,
            ),
            InstructionClass(
                name="VECTOR_CRYPTO",
                pattern=r"^(vclmul|vclmulh|vghsh|vgmul)\..*",
                cost=6,
            ),
            InstructionClass(
                name="VECTOR_PERM",
                pattern=r"^(vslide|vrgather|vcompress|vmerge|vmv)\..*",
                cost=3,
            ),
            InstructionClass(
                name="VECTOR_CFG",
                pattern=r"^vsetv?l?i?$",
                cost=1,
            ),
            InstructionClass(
                name="CSR",
                pattern=r"^(csrr|csrw|csrrs|csrrc|csrrwi|csrrsi|csrrci|rdcycle|rdinstret|rdtime)$",
                cost=1,
            ),
            InstructionClass(
                name="FENCE",
                pattern=r"^(fence|fence\.i|sfence).*",
                cost=1,
            ),
        ]
        return cls(
            classes=classes,
            default_cost=1,
            name="rocket-like-in-order",
            description="Single-issue in-order RISC-V core (Rocket-class)",
        )

    def classify(self, mnemonic: str) -> tuple[str, int]:
        """Return (class_name, cost) for a mnemonic using first-match semantics.

        Returns ("UNKNOWN", default_cost) if no pattern matches.
        Tracks unmatched mnemonics for later warning emission.
        """
        for ic, compiled_re in self._compiled:
            if compiled_re.match(mnemonic):
                return (ic.name, ic.cost)

        # No match found — track as unmatched
        self._unmatched.add(mnemonic)
        return ("UNKNOWN", self.default_cost)

    def is_vector(self, class_name: str) -> bool:
        """Return True if class_name is a vector sub-class (starts with VECTOR_)."""
        return class_name.startswith("VECTOR_")

    @property
    def unmatched_mnemonics(self) -> set[str]:
        """Return the set of unique unmatched mnemonics seen so far."""
        return self._unmatched

    def emit_unmatched_warnings(self) -> None:
        """Emit a single warning to stderr listing each unmatched mnemonic exactly once.

        Clears the unmatched set after emission.
        """
        if self._unmatched:
            mnemonics_sorted = sorted(self._unmatched)
            print(
                f"WARNING: {len(mnemonics_sorted)} unmatched mnemonic(s) "
                f"assigned default cost ({self.default_cost}): "
                f"{', '.join(mnemonics_sorted)}",
                file=sys.stderr,
            )
            self._unmatched.clear()
