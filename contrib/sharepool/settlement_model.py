#!/usr/bin/env python3
"""Deterministic settlement-state reference model for RNG sharepool.

This model sits one layer below simulate.py:

- simulate.py proves reward-window math and payout-root determinism
- settlement_model.py proves the many-claim accounting state machine for one
  compact pooled-reward settlement output
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import sys
from typing import Any

import simulate


COIN = simulate.COIN
DEFAULT_REPORT_PATH = Path(__file__).resolve().parent / "reports" / "pool-07b-settlement-vectors.json"


class SettlementModelError(RuntimeError):
    """User-facing settlement model error."""


def hash256(data: bytes) -> bytes:
    return simulate.hash256(data)


def compact_size(value: int) -> bytes:
    return simulate.compact_size(value)


def next_power_of_two(value: int) -> int:
    if value <= 1:
        return 1
    return 1 << (value - 1).bit_length()


def merkle_levels(leaf_hashes: list[bytes]) -> list[list[bytes]]:
    if not leaf_hashes:
        raise SettlementModelError("cannot build a Merkle tree with zero leaves")
    levels = [list(leaf_hashes)]
    while len(levels[-1]) > 1:
        current = list(levels[-1])
        if len(current) % 2 == 1:
            current.append(current[-1])
        parent_level = [
            hash256(current[index] + current[index + 1])
            for index in range(0, len(current), 2)
        ]
        levels.append(parent_level)
    return levels


def merkle_branch(leaf_hashes: list[bytes], index: int) -> list[bytes]:
    if not 0 <= index < len(leaf_hashes):
        raise SettlementModelError(f"leaf index out of range: {index}")
    levels = merkle_levels(leaf_hashes)
    branch: list[bytes] = []
    current_index = index
    for level in levels[:-1]:
        sibling_index = current_index ^ 1
        if sibling_index >= len(level):
            sibling_index = len(level) - 1
        branch.append(level[sibling_index])
        current_index //= 2
    return branch


def verify_merkle_branch(*, leaf_hash: bytes, index: int, branch: list[bytes]) -> bytes:
    if index < 0:
        raise SettlementModelError("leaf index must be non-negative")
    current = leaf_hash
    current_index = index
    for sibling in branch:
        if len(sibling) != 32:
            raise SettlementModelError("Merkle branch sibling must be 32 bytes")
        if current_index % 2 == 0:
            current = hash256(current + sibling)
        else:
            current = hash256(sibling + current)
        current_index //= 2
    return current


@dataclass(frozen=True)
class SettlementLeaf:
    payout_script: bytes
    amount_roshi: int
    first_share_id: str
    last_share_id: str

    @classmethod
    def from_reward_leaf(cls, leaf: simulate.RewardLeaf) -> "SettlementLeaf":
        return cls(
            payout_script=leaf.payout_script,
            amount_roshi=leaf.amount_roshi,
            first_share_id=leaf.first_share_id,
            last_share_id=leaf.last_share_id,
        )

    def sort_key(self) -> tuple[bytes, bytes]:
        return hash256(self.payout_script), self.payout_script

    def serialize(self) -> bytes:
        return b"".join(
            (
                compact_size(len(self.payout_script)),
                self.payout_script,
                int(self.amount_roshi).to_bytes(8, "little", signed=True),
                simulate.share_id_bytes(self.first_share_id),
                simulate.share_id_bytes(self.last_share_id),
            )
        )

    def leaf_hash(self) -> bytes:
        return hash256(b"RNGSharepoolLeaf" + self.serialize())

    def to_json_dict(self) -> dict[str, Any]:
        return {
            "payout_script_hex": self.payout_script.hex(),
            "amount_roshi": self.amount_roshi,
            "first_share_id": self.first_share_id,
            "last_share_id": self.last_share_id,
        }


@dataclass(frozen=True)
class SettlementDescriptor:
    version: int
    payout_root: bytes
    leaf_count: int

    def serialize(self) -> bytes:
        if self.leaf_count <= 0:
            raise SettlementModelError("leaf_count must be positive")
        return b"".join(
            (
                compact_size(self.version),
                self.payout_root,
                compact_size(self.leaf_count),
            )
        )

    def descriptor_hash(self) -> bytes:
        return hash256(b"RNGSharepoolDescriptor" + self.serialize())

    def to_json_dict(self) -> dict[str, Any]:
        return {
            "version": self.version,
            "payout_root": self.payout_root.hex(),
            "leaf_count": self.leaf_count,
            "descriptor_hash": self.descriptor_hash().hex(),
        }


@dataclass(frozen=True)
class SettlementState:
    descriptor: SettlementDescriptor
    leaves: tuple[SettlementLeaf, ...]
    claimed_flags: tuple[bool, ...]
    remaining_value_roshi: int

    @classmethod
    def initial(cls, leaves: list[SettlementLeaf]) -> "SettlementState":
        ordered_leaves = tuple(sorted(leaves, key=lambda leaf: leaf.sort_key()))
        if not ordered_leaves:
            raise SettlementModelError("settlement requires at least one payout leaf")
        total_value = sum(leaf.amount_roshi for leaf in ordered_leaves)
        descriptor = SettlementDescriptor(
            version=1,
            payout_root=payout_root(ordered_leaves),
            leaf_count=len(ordered_leaves),
        )
        return cls(
            descriptor=descriptor,
            leaves=ordered_leaves,
            claimed_flags=tuple(False for _ in ordered_leaves),
            remaining_value_roshi=total_value,
        )

    def status_tree_size(self) -> int:
        return next_power_of_two(max(1, self.descriptor.leaf_count))

    def status_leaf_hash(self, index: int, *, claimed: bool) -> bytes:
        return hash256(
            b"RNGSharepoolClaimFlag"
            + compact_size(index)
            + (b"\x01" if claimed else b"\x00")
        )

    def status_leaf_hashes(self) -> list[bytes]:
        hashes = [
            self.status_leaf_hash(index, claimed=self.claimed_flags[index])
            for index in range(self.descriptor.leaf_count)
        ]
        for index in range(self.descriptor.leaf_count, self.status_tree_size()):
            hashes.append(self.status_leaf_hash(index, claimed=True))
        return hashes

    def claim_status_root(self) -> bytes:
        return merkle_levels(self.status_leaf_hashes())[-1][0]

    def state_hash(self) -> bytes:
        return hash256(
            b"RNGSharepoolState"
            + self.descriptor.serialize()
            + self.claim_status_root()
        )

    def payout_branch(self, index: int) -> list[bytes]:
        payout_hashes = [leaf.leaf_hash() for leaf in self.leaves]
        return merkle_branch(payout_hashes, index)

    def status_branch(self, index: int) -> list[bytes]:
        return merkle_branch(self.status_leaf_hashes(), index)

    def leaf(self, index: int) -> SettlementLeaf:
        try:
            return self.leaves[index]
        except IndexError as exc:
            raise SettlementModelError(f"leaf index out of range: {index}") from exc

    def claimed_count(self) -> int:
        return sum(1 for flag in self.claimed_flags if flag)

    def to_json_dict(self) -> dict[str, Any]:
        return {
            "descriptor": self.descriptor.to_json_dict(),
            "claim_status_root": self.claim_status_root().hex(),
            "state_hash": self.state_hash().hex(),
            "remaining_value_roshi": self.remaining_value_roshi,
            "claimed_flags": [int(flag) for flag in self.claimed_flags],
            "leaves": [leaf.to_json_dict() for leaf in self.leaves],
        }


@dataclass(frozen=True)
class ClaimTemplate:
    payout_output_script: bytes
    payout_output_value: int
    successor_output_value: int | None
    successor_state_hash: bytes | None
    extra_input_value: int = 0
    extra_output_value: int = 0
    fee_roshi: int = 0

    def to_json_dict(self) -> dict[str, Any]:
        return {
            "payout_output_script_hex": self.payout_output_script.hex(),
            "payout_output_value": self.payout_output_value,
            "successor_output_value": self.successor_output_value,
            "successor_state_hash": self.successor_state_hash.hex() if self.successor_state_hash else None,
            "extra_input_value": self.extra_input_value,
            "extra_output_value": self.extra_output_value,
            "fee_roshi": self.fee_roshi,
        }


def payout_root(leaves: tuple[SettlementLeaf, ...] | list[SettlementLeaf]) -> bytes:
    ordered = list(sorted(leaves, key=lambda leaf: leaf.sort_key()))
    return merkle_levels([leaf.leaf_hash() for leaf in ordered])[-1][0]


def base_reward_leaves() -> list[SettlementLeaf]:
    trace = {
        "config": {"block_reward_roshi": 50 * COIN, "reward_window_work": 10},
        "share_runs": [
            {"miner": "miner_a", "count": 5},
            {"miner": "miner_b", "count": 3},
            {"miner": "miner_c", "count": 2},
        ],
        "blocks": [{"block_id": "block-1", "share_id": "share-000010", "height": 1}],
    }
    result = simulate.run_trace_dict(trace)
    return [SettlementLeaf.from_reward_leaf(leaf) for leaf in result.block_results[0].leaves]


def claim_template_for_state(
    state: SettlementState,
    *,
    leaf_index: int,
    extra_input_value: int = 0,
    extra_output_value: int = 0,
    fee_roshi: int = 0,
) -> tuple[SettlementLeaf, SettlementState | None, ClaimTemplate]:
    if leaf_index < 0 or leaf_index >= state.descriptor.leaf_count:
        raise SettlementModelError(f"leaf index out of range: {leaf_index}")
    if state.claimed_flags[leaf_index]:
        raise SettlementModelError(f"leaf {leaf_index} is already claimed")
    if extra_input_value < 0 or extra_output_value < 0 or fee_roshi < 0:
        raise SettlementModelError("extra_input_value, extra_output_value, and fee_roshi must be non-negative")

    leaf = state.leaf(leaf_index)
    new_claimed_flags = list(state.claimed_flags)
    new_claimed_flags[leaf_index] = True
    successor_value = state.remaining_value_roshi - leaf.amount_roshi
    if successor_value < 0:
        raise SettlementModelError("claimed amount exceeds remaining settlement value")

    successor_state: SettlementState | None
    successor_state_hash: bytes | None
    successor_output_value: int | None
    if successor_value == 0:
        successor_state = None
        successor_state_hash = None
        successor_output_value = None
    else:
        successor_state = SettlementState(
            descriptor=state.descriptor,
            leaves=state.leaves,
            claimed_flags=tuple(new_claimed_flags),
            remaining_value_roshi=successor_value,
        )
        successor_state_hash = successor_state.state_hash()
        successor_output_value = successor_value

    template = ClaimTemplate(
        payout_output_script=leaf.payout_script,
        payout_output_value=leaf.amount_roshi,
        successor_output_value=successor_output_value,
        successor_state_hash=successor_state_hash,
        extra_input_value=extra_input_value,
        extra_output_value=extra_output_value,
        fee_roshi=fee_roshi,
    )
    return leaf, successor_state, template


def verify_claim(
    state: SettlementState,
    *,
    leaf_index: int,
    leaf: SettlementLeaf,
    payout_branch: list[bytes],
    status_branch: list[bytes],
    template: ClaimTemplate,
) -> SettlementState | None:
    if leaf_index < 0 or leaf_index >= state.descriptor.leaf_count:
        raise SettlementModelError(f"leaf index out of range: {leaf_index}")
    if state.claimed_flags[leaf_index]:
        raise SettlementModelError(f"leaf {leaf_index} is already claimed")

    reconstructed_payout_root = verify_merkle_branch(
        leaf_hash=leaf.leaf_hash(),
        index=leaf_index,
        branch=payout_branch,
    )
    if reconstructed_payout_root != state.descriptor.payout_root:
        raise SettlementModelError("payout proof does not reconstruct the committed payout root")

    old_status_root = verify_merkle_branch(
        leaf_hash=state.status_leaf_hash(leaf_index, claimed=False),
        index=leaf_index,
        branch=status_branch,
    )
    if old_status_root != state.claim_status_root():
        raise SettlementModelError("status proof does not reconstruct the current claim-status root")

    reconstructed_state_hash = hash256(
        b"RNGSharepoolState" + state.descriptor.serialize() + old_status_root
    )
    if reconstructed_state_hash != state.state_hash():
        raise SettlementModelError("state hash does not match the current settlement state")

    if template.payout_output_script != leaf.payout_script:
        raise SettlementModelError("payout output script does not match the committed payout leaf")
    if template.payout_output_value != leaf.amount_roshi:
        raise SettlementModelError("payout output value does not match the committed payout leaf")

    expected_leaf = state.leaf(leaf_index)
    if leaf != expected_leaf:
        raise SettlementModelError("leaf data does not match the committed payout leaf")

    new_claimed_flags = list(state.claimed_flags)
    new_claimed_flags[leaf_index] = True
    successor_value = state.remaining_value_roshi - leaf.amount_roshi
    if successor_value < 0:
        raise SettlementModelError("claim would overdraw the settlement output")

    if successor_value == 0:
        if template.successor_output_value is not None or template.successor_state_hash is not None:
            raise SettlementModelError("final claim must not create a successor settlement output")
        successor_state = None
    else:
        successor_state = SettlementState(
            descriptor=state.descriptor,
            leaves=state.leaves,
            claimed_flags=tuple(new_claimed_flags),
            remaining_value_roshi=successor_value,
        )
        if template.successor_output_value != successor_value:
            raise SettlementModelError("successor settlement output value does not preserve the remainder")
        if template.successor_state_hash != successor_state.state_hash():
            raise SettlementModelError("successor settlement output state hash is incorrect")

    settlement_output_value = template.payout_output_value + (template.successor_output_value or 0)
    if settlement_output_value != state.remaining_value_roshi:
        raise SettlementModelError("settlement outputs do not conserve the settlement input value")

    total_inputs = state.remaining_value_roshi + template.extra_input_value
    total_outputs = settlement_output_value + template.extra_output_value
    actual_fee = total_inputs - total_outputs
    if actual_fee != template.fee_roshi:
        raise SettlementModelError("fee must be paid entirely from non-settlement inputs")

    return successor_state


def make_report() -> dict[str, Any]:
    leaves = base_reward_leaves()
    initial_state = SettlementState.initial(leaves)

    initial_vector = {
        "scenario": "initial_state",
        **initial_state.to_json_dict(),
    }

    valid_leaf, valid_successor, valid_template = claim_template_for_state(initial_state, leaf_index=1)
    valid_vector = {
        "scenario": "one_valid_claim_transition",
        "leaf_index": 1,
        "old_state_hash": initial_state.state_hash().hex(),
        "old_claim_status_root": initial_state.claim_status_root().hex(),
        "payout_branch": [node.hex() for node in initial_state.payout_branch(1)],
        "status_branch": [node.hex() for node in initial_state.status_branch(1)],
        "leaf": valid_leaf.to_json_dict(),
        "template": valid_template.to_json_dict(),
        "new_state_hash": valid_successor.state_hash().hex() if valid_successor else None,
        "new_claim_status_root": valid_successor.claim_status_root().hex() if valid_successor else None,
        "new_claimed_flags": [int(flag) for flag in valid_successor.claimed_flags] if valid_successor else None,
    }

    partial_state = verify_claim(
        initial_state,
        leaf_index=1,
        leaf=valid_leaf,
        payout_branch=initial_state.payout_branch(1),
        status_branch=initial_state.status_branch(1),
        template=valid_template,
    )
    assert partial_state is not None

    second_leaf, second_successor, second_template = claim_template_for_state(partial_state, leaf_index=0)
    assert second_successor is not None
    almost_final_state = verify_claim(
        partial_state,
        leaf_index=0,
        leaf=second_leaf,
        payout_branch=partial_state.payout_branch(0),
        status_branch=partial_state.status_branch(0),
        template=second_template,
    )
    assert almost_final_state is not None

    final_leaf, final_successor, final_template = claim_template_for_state(almost_final_state, leaf_index=2)
    final_vector = {
        "scenario": "final_claim_transition",
        "leaf_index": 2,
        "old_state_hash": almost_final_state.state_hash().hex(),
        "old_claim_status_root": almost_final_state.claim_status_root().hex(),
        "payout_branch": [node.hex() for node in almost_final_state.payout_branch(2)],
        "status_branch": [node.hex() for node in almost_final_state.status_branch(2)],
        "leaf": final_leaf.to_json_dict(),
        "template": final_template.to_json_dict(),
        "new_state_hash": final_successor.state_hash().hex() if final_successor else None,
        "new_claim_status_root": final_successor.claim_status_root().hex() if final_successor else None,
        "new_claimed_flags": [int(flag) for flag in final_successor.claimed_flags] if final_successor else None,
    }

    duplicate_vector = {
        "scenario": "duplicate_claim_rejection",
        "old_state_hash": partial_state.state_hash().hex(),
        "leaf_index": 1,
        "expected_error": "leaf 1 is already claimed",
    }

    fee_leaf, fee_successor, fee_template = claim_template_for_state(
        initial_state,
        leaf_index=0,
        extra_input_value=50_000,
        extra_output_value=40_000,
        fee_roshi=10_000,
    )
    fee_vector = {
        "scenario": "non_settlement_fee_funding",
        "leaf_index": 0,
        "old_state_hash": initial_state.state_hash().hex(),
        "old_claim_status_root": initial_state.claim_status_root().hex(),
        "payout_branch": [node.hex() for node in initial_state.payout_branch(0)],
        "status_branch": [node.hex() for node in initial_state.status_branch(0)],
        "leaf": fee_leaf.to_json_dict(),
        "template": fee_template.to_json_dict(),
        "new_state_hash": fee_successor.state_hash().hex() if fee_successor else None,
        "new_claim_status_root": fee_successor.claim_status_root().hex() if fee_successor else None,
        "new_claimed_flags": [int(flag) for flag in fee_successor.claimed_flags] if fee_successor else None,
    }

    report = {
        "metadata": {
            "model": "rng-sharepool-settlement-v1",
            "leaf_count": initial_state.descriptor.leaf_count,
            "status_tree_size": initial_state.status_tree_size(),
            "total_reward_roshi": initial_state.remaining_value_roshi,
        },
        "vectors": [
            initial_vector,
            valid_vector,
            final_vector,
            duplicate_vector,
            fee_vector,
        ],
    }
    return report


def self_test() -> dict[str, Any]:
    report = make_report()
    vectors = {vector["scenario"]: vector for vector in report["vectors"]}

    leaves = base_reward_leaves()
    state = SettlementState.initial(leaves)
    assert vectors["initial_state"]["state_hash"] == state.state_hash().hex()
    assert vectors["initial_state"]["claim_status_root"] == state.claim_status_root().hex()
    assert vectors["initial_state"]["claimed_flags"] == [0, 0, 0]

    valid = vectors["one_valid_claim_transition"]
    valid_leaf = state.leaf(valid["leaf_index"])
    valid_next = verify_claim(
        state,
        leaf_index=valid["leaf_index"],
        leaf=valid_leaf,
        payout_branch=[bytes.fromhex(node) for node in valid["payout_branch"]],
        status_branch=[bytes.fromhex(node) for node in valid["status_branch"]],
        template=ClaimTemplate(
            payout_output_script=bytes.fromhex(valid["template"]["payout_output_script_hex"]),
            payout_output_value=valid["template"]["payout_output_value"],
            successor_output_value=valid["template"]["successor_output_value"],
            successor_state_hash=bytes.fromhex(valid["template"]["successor_state_hash"]) if valid["template"]["successor_state_hash"] else None,
            extra_input_value=valid["template"]["extra_input_value"],
            extra_output_value=valid["template"]["extra_output_value"],
            fee_roshi=valid["template"]["fee_roshi"],
        ),
    )
    assert valid_next is not None
    assert valid["new_state_hash"] == valid_next.state_hash().hex()
    assert valid["new_claimed_flags"] == [0, 1, 0]

    second_leaf, second_successor, second_template = claim_template_for_state(valid_next, leaf_index=0)
    assert second_successor is not None
    almost_final = verify_claim(
        valid_next,
        leaf_index=0,
        leaf=second_leaf,
        payout_branch=valid_next.payout_branch(0),
        status_branch=valid_next.status_branch(0),
        template=second_template,
    )
    assert almost_final is not None

    final = vectors["final_claim_transition"]
    final_next = verify_claim(
        almost_final,
        leaf_index=final["leaf_index"],
        leaf=almost_final.leaf(final["leaf_index"]),
        payout_branch=[bytes.fromhex(node) for node in final["payout_branch"]],
        status_branch=[bytes.fromhex(node) for node in final["status_branch"]],
        template=ClaimTemplate(
            payout_output_script=bytes.fromhex(final["template"]["payout_output_script_hex"]),
            payout_output_value=final["template"]["payout_output_value"],
            successor_output_value=final["template"]["successor_output_value"],
            successor_state_hash=bytes.fromhex(final["template"]["successor_state_hash"]) if final["template"]["successor_state_hash"] else None,
            extra_input_value=final["template"]["extra_input_value"],
            extra_output_value=final["template"]["extra_output_value"],
            fee_roshi=final["template"]["fee_roshi"],
        ),
    )
    assert final_next is None
    assert final["new_state_hash"] is None

    duplicate = vectors["duplicate_claim_rejection"]
    try:
        _, _, duplicate_template = claim_template_for_state(valid_next, leaf_index=duplicate["leaf_index"])
        verify_claim(
            valid_next,
            leaf_index=duplicate["leaf_index"],
            leaf=valid_next.leaf(duplicate["leaf_index"]),
            payout_branch=valid_next.payout_branch(duplicate["leaf_index"]),
            status_branch=valid_next.status_branch(duplicate["leaf_index"]),
            template=duplicate_template,
        )
    except SettlementModelError as exc:
        assert str(exc) == duplicate["expected_error"]
    else:
        raise AssertionError("duplicate claim did not raise SettlementModelError")

    fee = vectors["non_settlement_fee_funding"]
    fee_next = verify_claim(
        state,
        leaf_index=fee["leaf_index"],
        leaf=state.leaf(fee["leaf_index"]),
        payout_branch=[bytes.fromhex(node) for node in fee["payout_branch"]],
        status_branch=[bytes.fromhex(node) for node in fee["status_branch"]],
        template=ClaimTemplate(
            payout_output_script=bytes.fromhex(fee["template"]["payout_output_script_hex"]),
            payout_output_value=fee["template"]["payout_output_value"],
            successor_output_value=fee["template"]["successor_output_value"],
            successor_state_hash=bytes.fromhex(fee["template"]["successor_state_hash"]) if fee["template"]["successor_state_hash"] else None,
            extra_input_value=fee["template"]["extra_input_value"],
            extra_output_value=fee["template"]["extra_output_value"],
            fee_roshi=fee["template"]["fee_roshi"],
        ),
    )
    assert fee_next is not None
    assert fee["new_state_hash"] == fee_next.state_hash().hex()

    committed_report = load_report(DEFAULT_REPORT_PATH)
    assert committed_report == report
    return report


def load_report(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--print-report", action="store_true", help="Print the deterministic settlement report to stdout")
    parser.add_argument("--write-report", type=Path, help="Write the deterministic settlement report JSON to a path")
    parser.add_argument("--self-test", action="store_true", help="Run the built-in settlement-model assertions and compare against the committed report")
    args = parser.parse_args(argv)

    if args.self_test:
        report = self_test()
        if args.print_report:
            json.dump(report, sys.stdout, indent=2, sort_keys=True)
            sys.stdout.write("\n")
        return 0

    report = make_report()
    if args.write_report:
        args.write_report.parent.mkdir(parents=True, exist_ok=True)
        with args.write_report.open("w", encoding="utf-8") as handle:
            json.dump(report, handle, indent=2, sort_keys=True)
            handle.write("\n")
    if args.print_report or not args.write_report:
        json.dump(report, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
