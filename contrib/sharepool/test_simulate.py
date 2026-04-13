#!/usr/bin/env python3
"""Tests for the sharepool economic simulator."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys

import pytest

import simulate


COIN = 100_000_000


def share_run_trace(*, counts: dict[str, int], reward_window_work: int | None = None) -> dict:
    share_runs = [
        {"miner": miner, "count": count, "work": 1}
        for miner, count in counts.items()
    ]
    total_shares = sum(counts.values())
    return {
        "config": {
            "block_reward_roshi": 50 * COIN,
            "reward_window_work": reward_window_work or total_shares,
        },
        "share_runs": share_runs,
        "blocks": [{"block_id": "block-1", "share_id": f"share-{total_shares:06d}", "height": 1}],
    }


def test_90_10_work_split_produces_proportional_reward_leaves() -> None:
    result = simulate.run_trace_dict(share_run_trace(counts={"miner_a": 90, "miner_b": 10}))

    rewards = result.block_results[0].rewards_by_miner
    total = sum(rewards.values())

    assert rewards["miner_a"] / total == pytest.approx(0.90, abs=0.01)
    assert rewards["miner_b"] / total == pytest.approx(0.10, abs=0.01)
    assert len(result.block_results[0].leaves) == 2


def test_deterministic_replay_produces_identical_commitment_roots() -> None:
    trace = share_run_trace(counts={"miner_a": 90, "miner_b": 10})

    first = simulate.run_trace_dict(trace)
    second = simulate.run_trace_dict(json.loads(json.dumps(trace)))

    assert first.block_results[0].commitment_root == second.block_results[0].commitment_root
    assert first.to_json_dict() == second.to_json_dict()


def test_reorged_share_suffix_changes_only_affected_window_outputs() -> None:
    base_trace = {
        "config": {"block_reward_roshi": 12 * COIN, "reward_window_work": 6},
        "shares": [
            {"share_id": "s1", "miner": "miner_a", "work": 1},
            {"share_id": "s2", "miner": "miner_a", "work": 1, "parent_share": "s1"},
            {"share_id": "s3", "miner": "miner_b", "work": 1, "parent_share": "s2"},
            {"share_id": "s4", "miner": "miner_b", "work": 1, "parent_share": "s3"},
            {"share_id": "s5", "miner": "miner_a", "work": 1, "parent_share": "s4"},
            {"share_id": "s6", "miner": "miner_b", "work": 1, "parent_share": "s5"},
            {"share_id": "s7", "miner": "miner_b", "work": 1, "parent_share": "s6"},
            {"share_id": "s8", "miner": "miner_b", "work": 1, "parent_share": "s7"},
        ],
        "blocks": [
            {"block_id": "pre-reorg", "share_id": "s5", "height": 1},
            {"block_id": "post-reorg-old", "share_id": "s8", "height": 2},
        ],
    }
    reorg_trace = {
        **base_trace,
        "shares": [
            *base_trace["shares"][:5],
            {"share_id": "r6", "miner": "miner_a", "work": 1, "parent_share": "s5"},
            {"share_id": "r7", "miner": "miner_a", "work": 1, "parent_share": "r6"},
            {"share_id": "r8", "miner": "miner_a", "work": 1, "parent_share": "r7"},
        ],
        "blocks": [
            {"block_id": "pre-reorg", "share_id": "s5", "height": 1},
            {"block_id": "post-reorg-new", "share_id": "r8", "height": 2},
        ],
    }

    old_result = simulate.run_trace_dict(base_trace)
    new_result = simulate.run_trace_dict(reorg_trace)

    assert old_result.block_results[0].commitment_root == new_result.block_results[0].commitment_root
    assert old_result.block_results[1].commitment_root != new_result.block_results[1].commitment_root
    assert old_result.block_results[1].rewards_by_miner["miner_b"] > new_result.block_results[1].rewards_by_miner["miner_b"]
    assert new_result.block_results[1].rewards_by_miner["miner_a"] > old_result.block_results[1].rewards_by_miner["miner_a"]


def test_pending_entitlement_is_visible_before_block_found() -> None:
    result = simulate.run_trace_dict(
        {
            "config": {"block_reward_roshi": 10 * COIN, "reward_window_work": 10},
            "share_runs": [{"miner": "miner_a", "count": 7}, {"miner": "miner_b", "count": 3}],
            "pending_tip": "share-000010",
            "blocks": [],
        }
    )

    pending = result.pending.rewards_by_miner

    assert pending["miner_a"] == 7 * COIN
    assert pending["miner_b"] == 3 * COIN
    assert result.pending.commitment_root != "00" * 32


def test_share_withholding_advantage_is_measured_and_non_positive_for_baseline() -> None:
    result = simulate.run_trace_dict(
        {
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
    )

    metric = result.withholding

    assert metric["miner"] == "withholder"
    assert metric["honest_reward_roshi"] > metric["published_reward_roshi"]
    assert metric["advantage_percent"] == pytest.approx(0.0)


def test_reward_variance_for_10_percent_miner_over_100_blocks_is_measured() -> None:
    result = simulate.measure_reward_variance(miner_fraction=0.10, blocks=100, seed=42)

    assert result["blocks"] == 100
    assert result["miner_fraction"] == pytest.approx(0.10)
    assert result["mean_reward_share"] == pytest.approx(0.10, abs=0.05)
    assert result["coefficient_of_variation_percent"] >= 0


def test_required_cli_trace_outputs_proportional_rewards() -> None:
    sharepool_dir = Path(__file__).resolve().parent
    completed = subprocess.run(
        [
            sys.executable,
            "simulate.py",
            "--trace",
            "examples/two_miners_90_10.json",
            "--verbose",
        ],
        cwd=sharepool_dir,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    assert "miner_a" in completed.stdout
    assert "90.00%" in completed.stdout
    assert "miner_b" in completed.stdout
    assert "10.00%" in completed.stdout
    assert "commitment_root" in completed.stdout


def test_csv_trace_loader_accepts_share_and_block_rows(tmp_path: Path) -> None:
    trace_path = tmp_path / "trace.csv"
    trace_path.write_text(
        "\n".join(
            [
                "type,share_id,parent_share,miner,work,block_id,height",
                "share,s1,,miner_a,1,,",
                "share,s2,s1,miner_b,1,,",
                "block,s2,,miner_b,,csv-block,1",
            ]
        )
        + "\n",
        encoding="utf-8",
    )

    trace = simulate.load_trace(trace_path)
    trace["config"] = {"block_reward_roshi": 2 * COIN, "reward_window_work": 2}
    result = simulate.run_trace_dict(trace)

    assert result.block_results[0].block_id == "csv-block"
    assert result.block_results[0].rewards_by_miner == {
        "miner_a": COIN,
        "miner_b": COIN,
    }
