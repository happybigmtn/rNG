#!/usr/bin/env python3
"""Tests for the sharepool settlement-state reference model."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys

import settlement_model


def test_self_test_passes_and_matches_committed_vectors() -> None:
    report = settlement_model.self_test()
    committed = settlement_model.load_report(settlement_model.DEFAULT_REPORT_PATH)
    assert report == committed


def test_initial_state_vector_is_unclaimed_and_value_complete() -> None:
    report = settlement_model.make_report()
    initial = next(vector for vector in report["vectors"] if vector["scenario"] == "initial_state")

    assert initial["claimed_flags"] == [0, 0, 0]
    assert initial["remaining_value_roshi"] == 50 * settlement_model.COIN
    assert initial["descriptor"]["leaf_count"] == 3


def test_valid_claim_transition_conserves_settlement_value() -> None:
    report = settlement_model.make_report()
    vector = next(vector for vector in report["vectors"] if vector["scenario"] == "one_valid_claim_transition")
    template = vector["template"]

    assert template["payout_output_value"] + template["successor_output_value"] == 50 * settlement_model.COIN
    assert vector["new_state_hash"] is not None
    assert vector["new_claimed_flags"] == [0, 1, 0]


def test_final_claim_has_no_successor_output() -> None:
    report = settlement_model.make_report()
    vector = next(vector for vector in report["vectors"] if vector["scenario"] == "final_claim_transition")
    template = vector["template"]

    assert template["successor_output_value"] is None
    assert template["successor_state_hash"] is None
    assert vector["new_state_hash"] is None


def test_duplicate_claim_vector_records_expected_rejection() -> None:
    report = settlement_model.make_report()
    vector = next(vector for vector in report["vectors"] if vector["scenario"] == "duplicate_claim_rejection")

    assert vector["expected_error"] == "leaf 1 is already claimed"


def test_non_settlement_inputs_fund_fee_without_draining_settlement_value() -> None:
    report = settlement_model.make_report()
    vector = next(vector for vector in report["vectors"] if vector["scenario"] == "non_settlement_fee_funding")
    template = vector["template"]

    settlement_outputs = template["payout_output_value"] + template["successor_output_value"]
    assert settlement_outputs == 50 * settlement_model.COIN
    assert template["extra_input_value"] == 50_000
    assert template["extra_output_value"] == 40_000
    assert template["fee_roshi"] == 10_000


def test_cli_print_report_and_self_test_are_wired() -> None:
    sharepool_dir = Path(__file__).resolve().parent

    printed = subprocess.run(
        [sys.executable, "settlement_model.py", "--print-report"],
        cwd=sharepool_dir,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    self_test = subprocess.run(
        [sys.executable, "settlement_model.py", "--self-test", "--print-report"],
        cwd=sharepool_dir,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    assert json.loads(printed.stdout) == settlement_model.make_report()
    assert json.loads(self_test.stdout) == settlement_model.make_report()
