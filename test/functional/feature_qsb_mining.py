#!/usr/bin/env python3
# Copyright (c) 2026-present The RNG developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Mine queued QSB candidates and prove unmodified peers accept the blocks."""

from copy import deepcopy
import json
from pathlib import Path

from test_framework.messages import (
    COutPoint,
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


def fixture_secret_preimage_hex(fixture):
    return "".join(fixture["secret_preimage_hex_parts"])


class QSBMiningTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [["-enableqsboperator"], []]
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

        self.wallet = MiniWallet(self.nodes[0])

        self.log.info("Mine spendable test funds on the QSB-enabled node and sync the peer")
        self.generate(self.wallet, 101)
        self.wallet.rescan_utxos()

        self.log.info("The non-operator peer must not expose the local-only QSB RPC")
        assert_raises_rpc_error(-32601, "Method not found", self.nodes[1].submitqsbtransaction, "00")

        funding_tx = self.build_funding_tx(fixture["funding"]["script_pubkey_hex"])
        spend_tx = self.build_spend_tx(funding_tx, fixture_secret_preimage_hex(fixture))

        self.log.info("A QSB spend stays unavailable until the funding output is mined")
        assert_raises_rpc_error(-25, "input-availability", self.nodes[0].submitqsbtransaction, spend_tx.serialize().hex())

        self.log.info("Queue the QSB funding transaction and mine a block that contains it")
        funding_result = self.nodes[0].submitqsbtransaction(funding_tx.serialize().hex())
        assert funding_result["accepted"]
        funding_blockhash = self.generatetodescriptor(self.nodes[0], 1, self.wallet.get_descriptor())[0]
        self.nodes[0].syncwithvalidationinterfacequeue()
        self.nodes[1].syncwithvalidationinterfacequeue()
        assert funding_tx.txid_hex in self.nodes[0].getblock(funding_blockhash)["tx"]
        assert funding_tx.txid_hex in self.nodes[1].getblock(funding_blockhash)["tx"]
        assert_equal(self.nodes[0].listqsbtransactions(), [])

        self.log.info("Queue the matching QSB spend and mine it into the next block")
        spend_result = self.nodes[0].submitqsbtransaction(spend_tx.serialize().hex())
        assert spend_result["accepted"]
        spend_blockhash = self.generatetodescriptor(self.nodes[0], 1, self.wallet.get_descriptor())[0]
        self.nodes[0].syncwithvalidationinterfacequeue()
        self.nodes[1].syncwithvalidationinterfacequeue()
        assert spend_tx.txid_hex in self.nodes[0].getblock(spend_blockhash)["tx"]
        assert spend_tx.txid_hex in self.nodes[1].getblock(spend_blockhash)["tx"]
        assert_equal(self.nodes[0].listqsbtransactions(), [])


if __name__ == "__main__":
    QSBMiningTest(__file__).main()
