#!/usr/bin/env python3
"""Deterministic sharepool economic simulator.

The simulator is intentionally offline. It models the reward-window and payout
commitment math from specs/sharepool.md without depending on node state, RPC, or
consensus code.
"""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import random
import sys
from typing import Any


COIN = 100_000_000
DEFAULT_BLOCK_REWARD_ROSHI = 50 * COIN
DEFAULT_BLOCK_SPACING_SECONDS = 120
DEFAULT_SHARE_SPACING_SECONDS = 10
DEFAULT_REWARD_WINDOW_SHARES = 720
DEFAULT_SHARE_WORK = 1
ZERO_ROOT = "00" * 32
POOL02R_VARIANCE_MINER_FRACTION = 0.10
POOL02R_VARIANCE_BLOCKS = 100
POOL02R_CONTINUITY_SEED = 42
POOL02R_STRESS_SEEDS = tuple(range(1, 21))
POOL02R_MAX_CV_PERCENT = 10.0
POOL02R_MAX_WITHHOLDING_ADVANTAGE_PERCENT = 5.0
POOL02R_REVISED_CANDIDATES = (
    {
        "id": "rejected_10s",
        "name": "10-second rejected baseline",
        "target_share_spacing_seconds": 10,
        "reward_window_work": 720,
        "role": "comparison",
    },
    {
        "id": "secondary_2s",
        "name": "2-second secondary candidate",
        "target_share_spacing_seconds": 2,
        "reward_window_work": 3600,
        "role": "secondary",
    },
    {
        "id": "primary_1s",
        "name": "1-second primary candidate",
        "target_share_spacing_seconds": 1,
        "reward_window_work": 7200,
        "role": "primary",
    },
)


class SimulationError(RuntimeError):
    """User-facing simulator error."""


@dataclass(frozen=True)
class Share:
    share_id: str
    miner: str
    payout_script: bytes
    work: int
    parent_share: str | None = None
    time: int | None = None
    prev_block_hash: str | None = None
    withheld: bool = False


@dataclass(frozen=True)
class RewardLeaf:
    miner: str
    payout_script: bytes
    amount_roshi: int
    first_share_id: str
    last_share_id: str
    work: int

    def sort_key(self) -> tuple[bytes, bytes]:
        return hash256(self.payout_script), self.payout_script

    def serialize(self) -> bytes:
        return b"".join(
            (
                compact_size(len(self.payout_script)),
                self.payout_script,
                int(self.amount_roshi).to_bytes(8, "little", signed=True),
                share_id_bytes(self.first_share_id),
                share_id_bytes(self.last_share_id),
            )
        )

    def leaf_hash(self) -> bytes:
        return hash256(b"RNGSharepoolLeaf" + self.serialize())

    def to_json_dict(self) -> dict[str, Any]:
        return {
            "miner": self.miner,
            "payout_script_hex": self.payout_script.hex(),
            "amount_roshi": self.amount_roshi,
            "first_share_id": self.first_share_id,
            "last_share_id": self.last_share_id,
            "work": self.work,
        }


@dataclass(frozen=True)
class PayoutResult:
    block_id: str
    height: int | None
    tip_share_id: str
    commitment_root: str
    leaves: list[RewardLeaf]
    window_share_count: int
    window_work: int

    @property
    def rewards_by_miner(self) -> dict[str, int]:
        rewards: dict[str, int] = {}
        for leaf in self.leaves:
            rewards[leaf.miner] = rewards.get(leaf.miner, 0) + leaf.amount_roshi
        return rewards

    def to_json_dict(self) -> dict[str, Any]:
        return {
            "block_id": self.block_id,
            "height": self.height,
            "tip_share_id": self.tip_share_id,
            "commitment_root": self.commitment_root,
            "window_share_count": self.window_share_count,
            "window_work": self.window_work,
            "rewards_by_miner": dict(sorted(self.rewards_by_miner.items())),
            "leaves": [leaf.to_json_dict() for leaf in self.leaves],
        }


@dataclass(frozen=True)
class SimulationResult:
    block_results: list[PayoutResult]
    pending: PayoutResult
    withholding: dict[str, Any]
    variance: dict[str, Any]
    config: dict[str, Any]

    def to_json_dict(self) -> dict[str, Any]:
        return {
            "config": self.config,
            "pending": self.pending.to_json_dict(),
            "blocks": [block.to_json_dict() for block in self.block_results],
            "withholding": self.withholding,
            "variance": self.variance,
        }


class SharepoolSimulator:
    def __init__(self, shares: list[Share], config: dict[str, Any]) -> None:
        self.shares = shares
        self.config = config
        self.reward_window_work = int(config["reward_window_work"])
        self.block_reward_roshi = int(config["block_reward_roshi"])
        self.share_by_id: dict[str, Share] = {}
        for share in shares:
            if share.share_id in self.share_by_id:
                raise SimulationError(f"duplicate share_id: {share.share_id}")
            self.share_by_id[share.share_id] = share

    def default_tip(self) -> str:
        if not self.shares:
            raise SimulationError("trace contains no shares")
        return self.shares[-1].share_id

    def reward_window(self, tip_share_id: str) -> list[Share]:
        if tip_share_id not in self.share_by_id:
            raise SimulationError(f"unknown tip share: {tip_share_id}")

        window_newest_first: list[Share] = []
        seen: set[str] = set()
        accumulated_work = 0
        current: str | None = tip_share_id
        while current is not None and current not in seen:
            share = self.share_by_id.get(current)
            if share is None:
                break
            seen.add(current)
            window_newest_first.append(share)
            accumulated_work += share.work
            if accumulated_work >= self.reward_window_work:
                break
            current = share.parent_share

        if not window_newest_first:
            raise SimulationError(f"no eligible shares for tip: {tip_share_id}")
        return list(reversed(window_newest_first))

    def payout_for_tip(self, *, block_id: str, tip_share_id: str, height: int | None = None,
                       reward_roshi: int | None = None) -> PayoutResult:
        reward = self.block_reward_roshi if reward_roshi is None else int(reward_roshi)
        window = self.reward_window(tip_share_id)
        leaves = calculate_reward_leaves(window, reward)
        return PayoutResult(
            block_id=block_id,
            height=height,
            tip_share_id=tip_share_id,
            commitment_root=merkle_root(leaves),
            leaves=leaves,
            window_share_count=len(window),
            window_work=sum(share.work for share in window),
        )


def hash256(data: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def compact_size(value: int) -> bytes:
    if value < 0:
        raise ValueError("CompactSize cannot encode negative values")
    if value < 0xFD:
        return bytes([value])
    if value <= 0xFFFF:
        return b"\xFD" + value.to_bytes(2, "little")
    if value <= 0xFFFFFFFF:
        return b"\xFE" + value.to_bytes(4, "little")
    return b"\xFF" + value.to_bytes(8, "little")


def share_id_bytes(share_id: str) -> bytes:
    if len(share_id) == 64:
        try:
            return bytes.fromhex(share_id)
        except ValueError:
            pass
    return hash256(b"RNGShareId" + share_id.encode("utf-8"))


def payout_script_for_miner(miner: str) -> bytes:
    return b"\x00\x14" + hashlib.sha256(miner.encode("utf-8")).digest()[:20]


def parse_payout_script(raw: Any, miner: str) -> bytes:
    if raw is None or raw == "":
        return payout_script_for_miner(miner)
    try:
        return bytes.fromhex(str(raw))
    except ValueError as exc:
        raise SimulationError(f"invalid payout_script hex for miner {miner}: {raw}") from exc


def normalize_config(raw_config: dict[str, Any] | None) -> dict[str, Any]:
    config = dict(raw_config or {})
    block_spacing = int(config.get("block_spacing_seconds", DEFAULT_BLOCK_SPACING_SECONDS))
    share_spacing = int(config.get("share_spacing_seconds", DEFAULT_SHARE_SPACING_SECONDS))
    expected_window_shares = int(config.get("reward_window_shares", DEFAULT_REWARD_WINDOW_SHARES))
    share_work = int(config.get("share_work", DEFAULT_SHARE_WORK))
    config.setdefault("block_reward_roshi", DEFAULT_BLOCK_REWARD_ROSHI)
    config.setdefault("block_spacing_seconds", block_spacing)
    config.setdefault("share_spacing_seconds", share_spacing)
    config.setdefault("share_target_ratio", block_spacing // share_spacing)
    config.setdefault("reward_window_shares", expected_window_shares)
    config.setdefault("reward_window_work", expected_window_shares * share_work)
    return config


def expand_shares(trace: dict[str, Any]) -> list[Share]:
    shares: list[Share] = []

    def append_share(entry: dict[str, Any], default_id: str | None = None) -> None:
        miner = str(entry.get("miner") or entry.get("payout") or "")
        if not miner:
            raise SimulationError("share is missing miner")
        parent = entry.get("parent_share")
        if parent is None and shares:
            parent = shares[-1].share_id
        work = int(entry.get("work", DEFAULT_SHARE_WORK))
        if work <= 0:
            raise SimulationError("share work must be positive")
        share_id = str(entry.get("share_id") or entry.get("id") or default_id or f"share-{len(shares) + 1:06d}")
        shares.append(
            Share(
                share_id=share_id,
                miner=miner,
                payout_script=parse_payout_script(entry.get("payout_script"), miner),
                work=work,
                parent_share=str(parent) if parent else None,
                time=int(entry["time"]) if "time" in entry and entry["time"] not in (None, "") else None,
                prev_block_hash=str(entry["prev_block_hash"]) if entry.get("prev_block_hash") else None,
                withheld=bool(entry.get("withheld", False)),
            )
        )

    for entry in trace.get("shares", []):
        append_share(dict(entry))

    next_index = len(shares) + 1
    for run in trace.get("share_runs", []):
        run = dict(run)
        count = int(run.get("count", 0))
        if count <= 0:
            raise SimulationError("share run count must be positive")
        prefix = str(run.get("id_prefix", "share"))
        for offset in range(count):
            entry = dict(run)
            entry.pop("count", None)
            entry["share_id"] = run.get("share_id") or f"{prefix}-{next_index + offset:06d}"
            append_share(entry)
        next_index += count

    return shares


def calculate_reward_leaves(window: list[Share], reward_roshi: int) -> list[RewardLeaf]:
    total_work = sum(share.work for share in window)
    if total_work <= 0:
        raise SimulationError("reward window has no work")

    grouped: dict[bytes, dict[str, Any]] = {}
    for share in window:
        group = grouped.setdefault(
            share.payout_script,
            {
                "miner": share.miner,
                "work": 0,
                "first_share_id": share.share_id,
                "last_share_id": share.share_id,
            },
        )
        group["work"] += share.work
        group["last_share_id"] = share.share_id

    amounts: dict[bytes, int] = {}
    for payout_script, group in grouped.items():
        amounts[payout_script] = (reward_roshi * int(group["work"])) // total_work

    remainder = reward_roshi - sum(amounts.values())
    for payout_script in sorted(grouped, key=lambda script: (hash256(script), script)):
        if remainder <= 0:
            break
        amounts[payout_script] += 1
        remainder -= 1

    leaves = [
        RewardLeaf(
            miner=str(group["miner"]),
            payout_script=payout_script,
            amount_roshi=amounts[payout_script],
            first_share_id=str(group["first_share_id"]),
            last_share_id=str(group["last_share_id"]),
            work=int(group["work"]),
        )
        for payout_script, group in grouped.items()
    ]
    return sorted(leaves, key=lambda leaf: leaf.sort_key())


def merkle_root(leaves: list[RewardLeaf]) -> str:
    if not leaves:
        return ZERO_ROOT
    level = [leaf.leaf_hash() for leaf in sorted(leaves, key=lambda leaf: leaf.sort_key())]
    while len(level) > 1:
        if len(level) % 2 == 1:
            level.append(level[-1])
        level = [
            hash256(level[index] + level[index + 1])
            for index in range(0, len(level), 2)
        ]
    return level[0].hex()


def run_trace_dict(trace: dict[str, Any]) -> SimulationResult:
    config = normalize_config(trace.get("config"))
    shares = expand_shares(trace)
    simulator = SharepoolSimulator(shares, config)
    pending_tip = str(trace.get("pending_tip") or simulator.default_tip())
    pending = simulator.payout_for_tip(block_id="pending", tip_share_id=pending_tip)

    block_results: list[PayoutResult] = []
    for index, block in enumerate(trace.get("blocks", []), start=1):
        block = dict(block)
        tip_share_id = str(block.get("share_id") or block.get("tip_share_id") or simulator.default_tip())
        block_results.append(
            simulator.payout_for_tip(
                block_id=str(block.get("block_id", f"block-{index}")),
                tip_share_id=tip_share_id,
                height=int(block["height"]) if "height" in block and block["height"] not in (None, "") else None,
                reward_roshi=int(block["reward_roshi"]) if block.get("reward_roshi") not in (None, "") else None,
            )
        )

    withholding = measure_withholding(trace, shares, config, block_results)
    variance = measure_reward_variance(
        miner_fraction=float(config.get("variance_miner_fraction", 0.10)),
        blocks=int(config.get("variance_blocks", 100)),
        seed=int(config.get("variance_seed", 42)),
        reward_window_work=int(config.get("variance_reward_window_work", DEFAULT_REWARD_WINDOW_SHARES)),
    )
    return SimulationResult(
        block_results=block_results,
        pending=pending,
        withholding=withholding,
        variance=variance,
        config=config,
    )


def load_trace(path: Path) -> dict[str, Any]:
    if path.suffix.lower() == ".csv":
        return load_csv_trace(path)
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def load_csv_trace(path: Path) -> dict[str, Any]:
    shares: list[dict[str, Any]] = []
    blocks: list[dict[str, Any]] = []
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            row_type = (row.get("type") or "share").strip().lower()
            cleaned = {key: value for key, value in row.items() if value not in (None, "")}
            if row_type == "share":
                cleaned.pop("type", None)
                shares.append(cleaned)
            elif row_type == "block":
                cleaned.pop("type", None)
                blocks.append(cleaned)
            elif row_type == "config":
                continue
            else:
                raise SimulationError(f"unsupported CSV row type: {row_type}")
    return {"shares": shares, "blocks": blocks}


def measure_withholding(trace: dict[str, Any], shares: list[Share], config: dict[str, Any],
                        honest_blocks: list[PayoutResult]) -> dict[str, Any]:
    configured = dict(trace.get("withholding") or {})
    withheld_miners = sorted({share.miner for share in shares if share.withheld})
    miner = configured.get("miner") or (withheld_miners[0] if withheld_miners else None)
    if miner is None:
        return {
            "miner": None,
            "withheld_share_count": 0,
            "withheld_work": 0,
            "honest_reward_roshi": 0,
            "published_reward_roshi": 0,
            "advantage_percent": 0.0,
            "note": "no shares marked withheld in trace",
        }

    full_by_id = {share.share_id: share for share in shares}
    published_shares = [
        share for share in shares
        if not (share.miner == miner and share.withheld)
    ]
    published_by_id = {share.share_id: share for share in published_shares}
    published_simulator = SharepoolSimulator(published_shares, config)

    honest_reward = sum(block.rewards_by_miner.get(str(miner), 0) for block in honest_blocks)
    published_reward = 0
    for block in trace.get("blocks", []):
        original_tip = str(block.get("share_id") or block.get("tip_share_id") or "")
        published_tip = nearest_published_ancestor(original_tip, full_by_id, published_by_id)
        if published_tip is None:
            continue
        published_block = published_simulator.payout_for_tip(
            block_id=str(block.get("block_id", "block")),
            tip_share_id=published_tip,
            height=int(block["height"]) if "height" in block and block["height"] not in (None, "") else None,
            reward_roshi=int(block["reward_roshi"]) if block.get("reward_roshi") not in (None, "") else None,
        )
        published_reward += published_block.rewards_by_miner.get(str(miner), 0)

    delta = published_reward - honest_reward
    advantage_percent = 0.0
    if honest_reward > 0 and delta > 0:
        advantage_percent = 100.0 * delta / honest_reward
    return {
        "miner": str(miner),
        "withheld_share_count": sum(1 for share in shares if share.miner == miner and share.withheld),
        "withheld_work": sum(share.work for share in shares if share.miner == miner and share.withheld),
        "honest_reward_roshi": honest_reward,
        "published_reward_roshi": published_reward,
        "delta_roshi": delta,
        "advantage_percent": round(advantage_percent, 8),
    }


def nearest_published_ancestor(original_tip: str, full_by_id: dict[str, Share],
                               published_by_id: dict[str, Share]) -> str | None:
    current = original_tip
    seen: set[str] = set()
    while current and current not in seen:
        if current in published_by_id:
            return current
        seen.add(current)
        share = full_by_id.get(current)
        if share is None or share.parent_share is None:
            return None
        current = share.parent_share
    return None


def measure_reward_variance(*, miner_fraction: float = 0.10, blocks: int = 100, seed: int = 42,
                            shares_per_block: int | None = None,
                            reward_window_work: int = DEFAULT_REWARD_WINDOW_SHARES) -> dict[str, Any]:
    if not 0 < miner_fraction < 1:
        raise SimulationError("miner_fraction must be between 0 and 1")
    if blocks <= 0:
        raise SimulationError("blocks must be positive")
    if shares_per_block is None:
        shares_per_block = DEFAULT_BLOCK_SPACING_SECONDS // DEFAULT_SHARE_SPACING_SECONDS

    rng = random.Random(seed)
    shares: list[dict[str, Any]] = []
    block_events: list[dict[str, Any]] = []
    for block_index in range(1, blocks + 1):
        for _ in range(shares_per_block):
            share_index = len(shares) + 1
            miner = "miner_10pct" if rng.random() < miner_fraction else "miner_rest"
            shares.append({"share_id": f"v{share_index}", "miner": miner, "work": 1})
        block_events.append(
            {
                "block_id": f"variance-{block_index}",
                "height": block_index,
                "share_id": shares[-1]["share_id"],
                "reward_roshi": DEFAULT_BLOCK_REWARD_ROSHI,
            }
        )

    trace = {
        "config": {
            "block_reward_roshi": DEFAULT_BLOCK_REWARD_ROSHI,
            "reward_window_work": reward_window_work,
        },
        "shares": shares,
        "blocks": block_events,
    }
    simulator = SharepoolSimulator(expand_shares(trace), normalize_config(trace["config"]))
    rewards = [
        simulator.payout_for_tip(
            block_id=block["block_id"],
            tip_share_id=block["share_id"],
            height=block["height"],
            reward_roshi=block["reward_roshi"],
        ).rewards_by_miner.get("miner_10pct", 0)
        for block in block_events
    ]
    mean_reward = sum(rewards) / len(rewards)
    variance = sum((reward - mean_reward) ** 2 for reward in rewards) / len(rewards)
    stddev = math.sqrt(variance)
    mean_share = mean_reward / DEFAULT_BLOCK_REWARD_ROSHI
    cv = 0.0 if mean_reward == 0 else 100.0 * stddev / mean_reward
    return {
        "miner": "miner_10pct",
        "miner_fraction": miner_fraction,
        "blocks": blocks,
        "shares_per_block": shares_per_block,
        "reward_window_work": reward_window_work,
        "seed": seed,
        "mean_reward_roshi": round(mean_reward, 8),
        "mean_reward_share": round(mean_share, 8),
        "coefficient_of_variation_percent": round(cv, 8),
    }


def built_in_withholding_trace() -> dict[str, Any]:
    return {
        "config": {"block_reward_roshi": 20 * COIN, "reward_window_work": 20},
        "shares": [
            *[
                {"share_id": f"a{i}", "miner": "honest", "work": 1}
                for i in range(1, 17)
            ],
            *[
                {"share_id": f"w{i}", "miner": "withholder", "work": 1, "withheld": i > 2}
                for i in range(1, 5)
            ],
        ],
        "blocks": [{"block_id": "block-1", "share_id": "w4", "height": 1}],
        "withholding": {"miner": "withholder"},
    }


def revised_candidate_sweep() -> dict[str, Any]:
    withholding = dict(run_trace_dict(built_in_withholding_trace()).withholding)
    withholding["status"] = (
        "pass"
        if withholding["advantage_percent"] < POOL02R_MAX_WITHHOLDING_ADVANTAGE_PERCENT
        else "fail"
    )

    candidates = [
        revised_candidate_result(candidate)
        for candidate in POOL02R_REVISED_CANDIDATES
    ]

    passing_primary = [
        candidate["id"]
        for candidate in candidates
        if candidate["role"] == "primary" and candidate["status"] == "pass"
    ]
    return {
        "name": "POOL-02R revised sharepool constants sweep",
        "network": {
            "name": "mainnet-candidate",
            "block_spacing_seconds": DEFAULT_BLOCK_SPACING_SECONDS,
        },
        "metric": {
            "miner_fraction": POOL02R_VARIANCE_MINER_FRACTION,
            "blocks": POOL02R_VARIANCE_BLOCKS,
            "continuity_seed": POOL02R_CONTINUITY_SEED,
            "stress_seeds": list(POOL02R_STRESS_SEEDS),
            "stddev": "population",
            "cold_start_blocks_included": True,
        },
        "thresholds": {
            "withholding_advantage_percent": POOL02R_MAX_WITHHOLDING_ADVANTAGE_PERCENT,
            "max_cv_percent": POOL02R_MAX_CV_PERCENT,
        },
        "withholding": withholding,
        "candidates": candidates,
        "recommended_candidate_ids": passing_primary,
        "notes": [
            "10-second candidate is retained only as a rejected baseline comparison.",
            "2-second candidate passes seed 42 but fails at least one required stress seed.",
            "1-second candidate is the only revised candidate that passes every POOL-02R variance threshold.",
        ],
    }


def revised_candidate_result(candidate: dict[str, Any]) -> dict[str, Any]:
    share_spacing = int(candidate["target_share_spacing_seconds"])
    shares_per_block = DEFAULT_BLOCK_SPACING_SECONDS // share_spacing
    reward_window_work = int(candidate["reward_window_work"])
    seed_42 = measure_reward_variance(
        miner_fraction=POOL02R_VARIANCE_MINER_FRACTION,
        blocks=POOL02R_VARIANCE_BLOCKS,
        seed=POOL02R_CONTINUITY_SEED,
        shares_per_block=shares_per_block,
        reward_window_work=reward_window_work,
    )
    stress = [
        measure_reward_variance(
            miner_fraction=POOL02R_VARIANCE_MINER_FRACTION,
            blocks=POOL02R_VARIANCE_BLOCKS,
            seed=seed,
            shares_per_block=shares_per_block,
            reward_window_work=reward_window_work,
        )
        for seed in POOL02R_STRESS_SEEDS
    ]
    stress_summary = variance_stress_summary(stress)
    passes = (
        seed_42["coefficient_of_variation_percent"] < POOL02R_MAX_CV_PERCENT
        and not stress_summary["failing_seeds"]
    )
    if passes:
        status = "pass"
    elif candidate["role"] == "comparison":
        status = "comparison_fail"
    else:
        status = "fail"

    window_horizon_blocks = reward_window_work / shares_per_block
    return {
        "id": candidate["id"],
        "name": candidate["name"],
        "role": candidate["role"],
        "status": status,
        "target_share_spacing_seconds": share_spacing,
        "shares_per_block": shares_per_block,
        "reward_window_work": reward_window_work,
        "window_horizon_blocks": round(window_horizon_blocks, 8),
        "window_horizon_seconds": int(reward_window_work * share_spacing),
        "cold_start": {
            "included": True,
            "blocks_before_full_window": math.ceil(window_horizon_blocks),
        },
        "seed_42": seed_42,
        "stress_seeds": stress,
        "stress_summary": stress_summary,
    }


def variance_stress_summary(results: list[dict[str, Any]]) -> dict[str, Any]:
    cvs = [float(result["coefficient_of_variation_percent"]) for result in results]
    failing = [
        {
            "seed": int(result["seed"]),
            "coefficient_of_variation_percent": result["coefficient_of_variation_percent"],
        }
        for result in results
        if float(result["coefficient_of_variation_percent"]) >= POOL02R_MAX_CV_PERCENT
    ]
    return {
        "min_cv_percent": round(min(cvs), 8),
        "max_cv_percent": round(max(cvs), 8),
        "mean_cv_percent": round(sum(cvs) / len(cvs), 8),
        "failing_seeds": failing,
    }


def built_in_baseline_trace() -> dict[str, Any]:
    return {
        "config": {
            "block_reward_roshi": DEFAULT_BLOCK_REWARD_ROSHI,
            "reward_window_work": 100,
        },
        "share_runs": [
            {"miner": "miner_a", "count": 90, "work": 1},
            {"miner": "miner_b", "count": 10, "work": 1},
        ],
        "pending_tip": "share-000100",
        "blocks": [{"block_id": "baseline", "height": 1, "share_id": "share-000100"}],
    }


def print_verbose(result: SimulationResult) -> None:
    print("Sharepool simulator")
    print(f"reward_window_work={result.config['reward_window_work']}")
    print_payout("pending", result.pending)
    for block in result.block_results:
        print_payout(block.block_id, block)
    print(
        "withholding: "
        f"miner={result.withholding['miner']} "
        f"withheld_shares={result.withholding['withheld_share_count']} "
        f"advantage_percent={result.withholding['advantage_percent']:.4f}"
    )
    print(
        "variance_10pct_100_blocks: "
        f"mean_share={100 * result.variance['mean_reward_share']:.2f}% "
        f"cv={result.variance['coefficient_of_variation_percent']:.2f}% "
        f"seed={result.variance['seed']}"
    )


def print_payout(label: str, payout: PayoutResult) -> None:
    total = sum(payout.rewards_by_miner.values())
    print(
        f"{label}: commitment_root={payout.commitment_root} "
        f"window_shares={payout.window_share_count} window_work={payout.window_work}"
    )
    for miner, amount in sorted(payout.rewards_by_miner.items()):
        percent = 0.0 if total == 0 else 100.0 * amount / total
        print(f"  {miner}: {amount} roshi ({percent:.2f}%)")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace", type=Path, help="JSON or CSV share trace to replay")
    parser.add_argument("--scenario", choices=["baseline"], help="built-in scenario to run")
    parser.add_argument("--sweep", choices=["revised-candidates"], help="run a deterministic parameter sweep")
    parser.add_argument("--verbose", action="store_true", help="print a human-readable report")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        if args.sweep == "revised-candidates":
            if args.trace or args.scenario:
                raise SimulationError("--sweep cannot be combined with --trace or --scenario")
            print(json.dumps(revised_candidate_sweep(), indent=2, sort_keys=True))
            return 0
        if args.trace:
            trace = load_trace(args.trace)
        elif args.scenario == "baseline":
            trace = built_in_baseline_trace()
        else:
            raise SimulationError("pass --trace <path> or --scenario baseline")
        result = run_trace_dict(trace)
        if args.verbose:
            print_verbose(result)
        else:
            print(json.dumps(result.to_json_dict(), indent=2, sort_keys=True))
    except (OSError, json.JSONDecodeError, SimulationError, ValueError) as exc:
        print(f"simulate.py: error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
