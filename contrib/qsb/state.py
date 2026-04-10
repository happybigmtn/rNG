#!/usr/bin/env python3
"""Helpers for QSB state and fixture files."""

from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path


STATE_SCHEMA_VERSION = 1


def utc_now_iso8601() -> str:
    return datetime.now(tz=timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def load_json(path: str | Path) -> dict:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def write_json(path: str | Path, data: dict) -> None:
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(data, indent=2, sort_keys=True) + "\n"
    tmp_path = target.with_suffix(target.suffix + ".tmp")
    tmp_path.write_text(payload, encoding="utf-8")
    tmp_path.replace(target)


def new_state(*, template_version: str, network: str, seed_hex: str, secret_preimage_hex: str,
              secret_hash_hex: str, metadata_commitment_hex: str, payload_sha256_hex: str,
              payload_bytes: int, payload_chunk_size: int, payload_chunk_count: int,
              funding: dict) -> dict:
    return {
        "schema_version": STATE_SCHEMA_VERSION,
        "created_at": utc_now_iso8601(),
        "network": network,
        "template_version": template_version,
        "seed_hex": seed_hex,
        "secret_preimage_hex": secret_preimage_hex,
        "secret_hash_hex": secret_hash_hex,
        "metadata_commitment_hex": metadata_commitment_hex,
        "payload_sha256_hex": payload_sha256_hex,
        "payload_bytes": payload_bytes,
        "payload_chunk_size": payload_chunk_size,
        "payload_chunk_count": payload_chunk_count,
        "funding": funding,
        "spend": {
            "consumed": False,
            "destination_address": None,
            "destination_script_pubkey_hex": None,
            "fee_sats": None,
            "tx_hex": None,
            "txid": None,
        },
    }


def new_fixture(*, template_version: str, seed_hex: str, secret_preimage_hex: str,
                secret_hash_hex: str, metadata_commitment_hex: str, payload_sha256_hex: str,
                payload_bytes: int, payload_chunk_size: int, payload_chunk_count: int,
                funding_script_pubkey_hex: str, funding_script_pubkey_sha256: str,
                funding_script_size: int) -> dict:
    return {
        "schema_version": STATE_SCHEMA_VERSION,
        "template_version": template_version,
        "seed_hex": seed_hex,
        "secret_preimage_hex": secret_preimage_hex,
        "secret_hash_hex": secret_hash_hex,
        "metadata_commitment_hex": metadata_commitment_hex,
        "payload_sha256_hex": payload_sha256_hex,
        "payload_bytes": payload_bytes,
        "payload_chunk_size": payload_chunk_size,
        "payload_chunk_count": payload_chunk_count,
        "funding": {
            "script_pubkey_hex": funding_script_pubkey_hex,
            "script_pubkey_sha256": funding_script_pubkey_sha256,
            "script_size": funding_script_size,
        },
    }
