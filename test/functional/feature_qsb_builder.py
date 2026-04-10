#!/usr/bin/env python3
# Copyright (c) 2026-present The RNG developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Exercise the deterministic parts of the QSB builder feasibility spike."""

import json
from pathlib import Path
import subprocess
import sys

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_process_error,
)


FIXTURE_SEED = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
FIXTURE_NAME = "rng_qsb_v1_toy_seed_00010203.json"


class QSBBuilderTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 0
        self.uses_wallet = False

    def setup_network(self):
        pass

    def run_qsb(self, *args, check=True):
        command = [sys.executable, str(self.qsb_script), *args]
        return subprocess.run(
            command,
            check=check,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )

    def run_test(self):
        base_dir = Path(self.config["environment"]["SRCDIR"])
        self.qsb_script = base_dir / "contrib" / "qsb" / "qsb.py"
        self.fixture_path = base_dir / "contrib" / "qsb" / "fixtures" / FIXTURE_NAME

        self.log.info("Generate the deterministic fixture and compare it with the checked-in reference")
        produced_fixture_path = Path(self.options.tmpdir) / "qsb-fixture.json"
        self.run_qsb(
            "fixture",
            "--seed", FIXTURE_SEED,
            "--state-file", str(produced_fixture_path),
        )
        actual_fixture = json.loads(produced_fixture_path.read_text(encoding="utf-8"))
        expected_fixture = json.loads(self.fixture_path.read_text(encoding="utf-8"))
        assert_equal(actual_fixture, expected_fixture)
        assert_greater_than(actual_fixture["funding"]["script_size"], 520)
        assert actual_fixture["funding"]["script_size"] < 10_000

        self.log.info("Build a synthetic state file so the external spend path can be verified without a funded wallet")
        state_path = Path(self.options.tmpdir) / "qsb-state.json"
        synthetic_state = {
            "schema_version": 1,
            "created_at": "2026-04-09T00:00:00Z",
            "network": "regtest",
            "template_version": expected_fixture["template_version"],
            "seed_hex": expected_fixture["seed_hex"],
            "secret_preimage_hex": expected_fixture["secret_preimage_hex"],
            "secret_hash_hex": expected_fixture["secret_hash_hex"],
            "metadata_commitment_hex": expected_fixture["metadata_commitment_hex"],
            "payload_sha256_hex": expected_fixture["payload_sha256_hex"],
            "payload_bytes": expected_fixture["payload_bytes"],
            "payload_chunk_size": expected_fixture["payload_chunk_size"],
            "payload_chunk_count": expected_fixture["payload_chunk_count"],
            "funding": {
                "amount": "1.00000000",
                "amount_sats": 100000000,
                "script_pubkey_hex": expected_fixture["funding"]["script_pubkey_hex"],
                "script_pubkey_sha256": expected_fixture["funding"]["script_pubkey_sha256"],
                "script_size": expected_fixture["funding"]["script_size"],
                "tx_hex": "00",
                "txid": "11" * 32,
                "vout": 0,
                "fundrawtransaction_fee": 0.00001000,
                "change_position": 1,
            },
            "spend": {
                "consumed": False,
                "destination_address": None,
                "destination_script_pubkey_hex": None,
                "fee_sats": None,
                "tx_hex": None,
                "txid": None,
            },
        }
        state_path.write_text(json.dumps(synthetic_state, indent=2) + "\n", encoding="utf-8")

        self.log.info("Build the matching external spend and record one-time key use in the state file")
        self.run_qsb(
            "toy-spend",
            "--state-file", str(state_path),
            "--destination-script-hex", "51",
            "--fee-sats", "1000",
        )
        updated_state = json.loads(state_path.read_text(encoding="utf-8"))
        assert updated_state["spend"]["consumed"]
        assert_equal(updated_state["spend"]["destination_script_pubkey_hex"], "51")
        assert_equal(updated_state["spend"]["fee_sats"], 1000)
        assert len(updated_state["spend"]["txid"]) == 64
        assert len(updated_state["spend"]["tx_hex"]) > 20

        self.log.info("The state file must refuse one-time secret reuse")
        assert_raises_process_error(
            1,
            "QSB state already spent",
            self.run_qsb,
            "toy-spend",
            "--state-file", str(state_path),
            "--destination-script-hex", "51",
        )


if __name__ == "__main__":
    QSBBuilderTest(__file__).main()
