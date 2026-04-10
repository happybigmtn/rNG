#!/usr/bin/env python3
# Copyright (c) 2026-present The RNG developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Exercise the local-only QSB operator RPC queue."""

from copy import deepcopy
import json
from pathlib import Path

from test_framework.messages import (
    COutPoint,
    COIN,
    CTransaction,
    CTxIn,
    CTxOut,
    SEQUENCE_FINAL,
)
from test_framework.script import CScript
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)
from test_framework.wallet import MiniWallet


class QSBRPCTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [[]]
        self.uses_wallet = None
        self.rpc_timeout = 300

    def build_funding_tx(self, template_script_hex):
        tx = deepcopy(self.wallet.create_self_transfer()["tx"])
        tx.vout[0] = CTxOut(tx.vout[0].nValue, CScript(bytes.fromhex(template_script_hex)))
        return tx

    def build_spend_tx(self, funding_tx, secret_preimage_hex):
        tx = CTransaction()
        tx.vin = [
            CTxIn(
                COutPoint(int(funding_tx.txid_hex, 16), 0),
                CScript([bytes.fromhex(secret_preimage_hex)]),
                SEQUENCE_FINAL,
            )
        ]
        tx.vout = [CTxOut(funding_tx.vout[0].nValue - 1000, self.wallet.get_output_script())]
        tx.version = 2
        return tx

    def run_test(self):
        fixture_path = Path(self.config["environment"]["SRCDIR"]) / "contrib" / "qsb" / "fixtures" / "rng_qsb_v1_toy_seed_00010203.json"
        fixture = json.loads(fixture_path.read_text(encoding="utf-8"))

        self.log.info("QSB RPCs must stay hidden on nodes that do not opt in")
        assert_raises_rpc_error(-32601, "Method not found", self.nodes[0].submitqsbtransaction, "00")

        self.log.info("Restart with the operator-only QSB RPC surface enabled and generate spendable test funds")
        self.restart_node(0, extra_args=["-enableqsboperator"])
        self.wallet = MiniWallet(self.nodes[0])
        self.generatetodescriptor(self.nodes[0], 101, self.wallet.get_descriptor(), sync_fun=self.no_op)
        self.wallet.rescan_utxos()

        funding_tx = self.build_funding_tx(fixture["funding"]["script_pubkey_hex"])
        conflicting_funding_tx = deepcopy(funding_tx)
        conflicting_funding_tx.vout[0].nValue -= 1
        spend_tx = self.build_spend_tx(funding_tx, fixture["secret_preimage_hex"])
        standard_tx = self.wallet.create_self_transfer()["tx"]

        self.log.info("An enabled node starts with an empty local-only QSB queue")
        assert_equal(self.nodes[0].listqsbtransactions(), [])

        self.log.info("Reject ordinary transactions at the template-matching layer")
        assert_raises_rpc_error(-25, "template-matching", self.nodes[0].submitqsbtransaction, standard_tx.serialize().hex())

        self.log.info("Queue a supported QSB funding transaction without touching the public mempool")
        funding_result = self.nodes[0].submitqsbtransaction(funding_tx.serialize().hex())
        assert funding_result["accepted"]
        assert not funding_result["already_queued"]
        assert_equal(funding_result["txid"], funding_tx.txid_hex)
        assert_equal(funding_result["type"], "funding")
        assert funding_tx.txid_hex not in self.nodes[0].getrawmempool()

        queued = self.nodes[0].listqsbtransactions()
        assert_equal(len(queued), 1)
        assert_equal(queued[0]["txid"], funding_tx.txid_hex)
        assert_equal(queued[0]["type"], "funding")

        self.log.info("Duplicate submissions are idempotent")
        duplicate_result = self.nodes[0].submitqsbtransaction(funding_tx.serialize().hex())
        assert duplicate_result["accepted"]
        assert duplicate_result["already_queued"]
        assert_equal(duplicate_result["txid"], funding_tx.txid_hex)

        self.log.info("Conflicting queued candidates are rejected before they can coexist")
        assert_raises_rpc_error(-25, "input-conflict", self.nodes[0].submitqsbtransaction, conflicting_funding_tx.serialize().hex())

        self.log.info("A spend cannot be queued until its QSB funding output exists on-chain or in mempool")
        assert_raises_rpc_error(-25, "input-availability", self.nodes[0].submitqsbtransaction, spend_tx.serialize().hex())

        self.log.info("Queued candidates can be removed explicitly")
        remove_result = self.nodes[0].removeqsbtransaction(funding_tx.txid_hex)
        assert remove_result["removed"]
        assert_equal(self.nodes[0].listqsbtransactions(), [])


if __name__ == "__main__":
    QSBRPCTest(__file__).main()
