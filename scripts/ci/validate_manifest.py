#!/usr/bin/env python3
"""Validate experiments/manifest.yaml against experiments/manifest.schema.json."""

from __future__ import annotations

import json
import datetime as dt
from pathlib import Path

import yaml


REPO_ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = REPO_ROOT / "experiments" / "manifest.yaml"
SCHEMA_PATH = REPO_ROOT / "experiments" / "manifest.schema.json"


def _fallback_validate(manifest: dict, schema: dict) -> None:
    experiments = manifest.get("experiments")
    if not isinstance(experiments, list) or not experiments:
        raise ValueError("manifest.experiments must be a non-empty list")

    allowed_status = set(
        schema["properties"]["experiments"]["items"]["properties"]["status"]["enum"]
    )
    required = schema["properties"]["experiments"]["items"]["required"]

    for idx, item in enumerate(experiments):
        if not isinstance(item, dict):
            raise ValueError(f"experiments[{idx}] must be an object")
        for key in required:
            if key not in item:
                raise ValueError(f"experiments[{idx}] missing required key: {key}")
        if item["status"] not in allowed_status:
            raise ValueError(
                f"experiments[{idx}].status has invalid value: {item['status']}"
            )


def _normalize_yaml_scalars(value):
    if isinstance(value, dt.date):
        return value.isoformat()
    if isinstance(value, list):
        return [_normalize_yaml_scalars(item) for item in value]
    if isinstance(value, dict):
        return {k: _normalize_yaml_scalars(v) for k, v in value.items()}
    return value


def main() -> int:
    schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
    manifest = yaml.safe_load(MANIFEST_PATH.read_text(encoding="utf-8"))
    manifest = _normalize_yaml_scalars(manifest)

    try:
        from jsonschema import Draft202012Validator, FormatChecker

        validator = Draft202012Validator(schema, format_checker=FormatChecker())
        errors = sorted(validator.iter_errors(manifest), key=lambda e: list(e.path))
        if errors:
            for err in errors:
                where = ".".join(str(x) for x in err.path) or "<root>"
                print(f"[manifest] ERROR at {where}: {err.message}")
            return 1
    except ModuleNotFoundError:
        # Keep CI usable even if jsonschema is not preinstalled.
        _fallback_validate(manifest, schema)

    print("[manifest] OK: manifest.yaml conforms to manifest.schema.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
