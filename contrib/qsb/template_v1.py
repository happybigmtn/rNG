#!/usr/bin/env python3
"""Versioned toy QSB template used for the bare-script feasibility spike.

This is intentionally narrower than the paper's full construction. The goal for
Milestone 1 is to prove that RNG can:

1. carry a large bare script directly in scriptPubKey,
2. fund that output with ordinary wallet inputs,
3. spend it again without teaching the wallet about the custom script, and
4. keep one-time secret state outside the wallet.
"""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import os
from typing import Iterable


TEMPLATE_VERSION = "rng-qsb-v1-toy"
TEMPLATE_MAGIC = b"RNGQSBV1"

DEFAULT_PAYLOAD_BYTES = 780
DEFAULT_CHUNK_SIZE = 260
MAX_SCRIPT_ELEMENT_SIZE = 520
MAX_SCRIPT_SIZE = 10_000
MAX_OPS_PER_SCRIPT = 201

OP_DROP = 0x75
OP_EQUALVERIFY = 0x88
OP_SHA256 = 0xA8
OP_TRUE = 0x51
OP_PUSHDATA1 = 0x4C
OP_PUSHDATA2 = 0x4D


@dataclass(frozen=True)
class TemplateMaterial:
    seed: bytes
    secret_preimage: bytes
    secret_hash: bytes
    metadata_commitment: bytes
    payload: bytes


def sha256(data: bytes) -> bytes:
    return hashlib.sha256(data).digest()


def push_data(data: bytes) -> bytes:
    length = len(data)
    if length == 0:
        return b"\x00"
    if length <= 75:
        return bytes([length]) + data
    if length <= 255:
        return bytes([OP_PUSHDATA1, length]) + data
    if length <= 65535:
        return bytes([OP_PUSHDATA2]) + length.to_bytes(2, "little") + data
    raise ValueError(f"push exceeds supported size: {length}")


def _expand(seed: bytes, label: bytes, length: int) -> bytes:
    output = bytearray()
    counter = 0
    while len(output) < length:
        output.extend(sha256(seed + label + counter.to_bytes(4, "big")))
        counter += 1
    return bytes(output[:length])


def parse_seed(seed_hex: str | None) -> bytes:
    if seed_hex is None:
        return os.urandom(32)
    seed = bytes.fromhex(seed_hex)
    if len(seed) < 16:
        raise ValueError("seed must be at least 16 bytes")
    return seed


def derive_material(seed: bytes, payload_bytes: int = DEFAULT_PAYLOAD_BYTES) -> TemplateMaterial:
    if payload_bytes <= MAX_SCRIPT_ELEMENT_SIZE:
        raise ValueError("payload_bytes must be larger than 520 to exercise the bare-script path")
    return TemplateMaterial(
        seed=seed,
        secret_preimage=_expand(seed, b"secret-preimage", 32),
        secret_hash=sha256(_expand(seed, b"secret-preimage", 32)),
        metadata_commitment=sha256(_expand(seed, b"metadata", 32)),
        payload=_expand(seed, b"payload", payload_bytes),
    )


def split_payload(payload: bytes, chunk_size: int = DEFAULT_CHUNK_SIZE) -> list[bytes]:
    if chunk_size <= 0:
        raise ValueError("chunk_size must be positive")
    if chunk_size > MAX_SCRIPT_ELEMENT_SIZE:
        raise ValueError("chunk_size must be <= 520")
    return [payload[i:i + chunk_size] for i in range(0, len(payload), chunk_size)]


def build_funding_script(material: TemplateMaterial, chunk_size: int = DEFAULT_CHUNK_SIZE) -> bytes:
    chunks = split_payload(material.payload, chunk_size)
    if len(chunks) < 2:
        raise ValueError("template requires at least two payload chunks")

    script = bytearray()
    script.append(OP_SHA256)
    script.extend(push_data(material.secret_hash))
    script.append(OP_EQUALVERIFY)

    for item in (
        TEMPLATE_MAGIC,
        bytes([1]),
        material.metadata_commitment,
        *chunks,
    ):
        script.extend(push_data(item))
        script.append(OP_DROP)

    script.append(OP_TRUE)
    validate_funding_script(bytes(script))
    return bytes(script)


def validate_funding_script(script: bytes) -> None:
    if len(script) <= MAX_SCRIPT_ELEMENT_SIZE:
        raise ValueError("funding script is not large enough to prove the >520 byte bare-script path")
    if len(script) > MAX_SCRIPT_SIZE:
        raise ValueError("funding script exceeds the 10,000-byte consensus limit")
    if _count_non_push_ops(script) > MAX_OPS_PER_SCRIPT:
        raise ValueError("funding script exceeds the 201-opcode consensus limit")


def script_sha256(script: bytes) -> str:
    return sha256(script).hex()


def material_to_fixture_dict(material: TemplateMaterial, script: bytes, chunk_size: int) -> dict:
    return {
        "template_version": TEMPLATE_VERSION,
        "seed_hex": material.seed.hex(),
        "secret_preimage_hex": material.secret_preimage.hex(),
        "secret_hash_hex": material.secret_hash.hex(),
        "metadata_commitment_hex": material.metadata_commitment.hex(),
        "payload_sha256_hex": sha256(material.payload).hex(),
        "payload_bytes": len(material.payload),
        "payload_chunk_size": chunk_size,
        "payload_chunk_count": len(split_payload(material.payload, chunk_size)),
        "funding": {
            "script_pubkey_hex": script.hex(),
            "script_pubkey_sha256": script_sha256(script),
            "script_size": len(script),
        },
    }


def _count_non_push_ops(script: bytes) -> int:
    count = 0
    index = 0
    while index < len(script):
        opcode = script[index]
        index += 1
        if opcode == 0:
            continue
        if opcode <= 75:
            index += opcode
            continue
        if opcode == OP_PUSHDATA1:
            if index >= len(script):
                raise ValueError("truncated OP_PUSHDATA1")
            push_len = script[index]
            index += 1 + push_len
            continue
        if opcode == OP_PUSHDATA2:
            if index + 1 >= len(script):
                raise ValueError("truncated OP_PUSHDATA2")
            push_len = int.from_bytes(script[index:index + 2], "little")
            index += 2 + push_len
            continue
        count += 1
    return count


def iter_payload_chunks(material: TemplateMaterial, chunk_size: int = DEFAULT_CHUNK_SIZE) -> Iterable[bytes]:
    return split_payload(material.payload, chunk_size)
