#!/usr/bin/env python3
# Copyright (c) 2026-present The RNG developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Verify activated sharepool P2P share relay."""

from test_framework.messages import (
    CBlockHeader,
    CShareRecord,
    msg_getshare,
    msg_share,
)
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class ShareReceiver(P2PInterface):
    def __init__(self):
        super().__init__()
        self.received_share_ids = []
        self.received_shares = []

    def on_shareinv(self, message):
        self.received_share_ids.extend(message.share_ids)
        self.send_without_ping(msg_getshare(message.share_ids))

    def on_share(self, message):
        self.received_shares.extend(message.shares)


def uint256_from_rpc_hex(value):
    return int(value, 16)


class SharepoolRelayTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [
            ["-vbparams=sharepool:0:9999999999:0"],
            ["-vbparams=sharepool:0:9999999999:0"],
        ]

    def run_test(self):
        self.connect_nodes(0, 1)

        self.log.info("Attach long-lived peers before activation mining")
        observer = self.nodes[1].add_p2p_connection(ShareReceiver())
        sender = self.nodes[0].add_p2p_connection(P2PInterface())

        self.log.info("Activate sharepool on regtest")
        address = self.nodes[0].get_deterministic_priv_key().address
        for _ in range(6):
            self.generatetoaddress(self.nodes[0], 72, address, sync_fun=self.sync_all)
        self.wait_until(lambda: self.nodes[0].getdeploymentinfo()["deployments"]["sharepool"]["active"])
        self.wait_until(lambda: self.nodes[1].getdeploymentinfo()["deployments"]["sharepool"]["active"])

        self.log.info("Build a valid share from an already-mined block header")
        block_hash = self.generatetoaddress(self.nodes[0], 1, address, sync_fun=self.sync_all)[0]
        block = self.nodes[0].getblock(block_hash)

        header = CBlockHeader()
        header.nVersion = block["version"]
        header.hashPrevBlock = uint256_from_rpc_hex(block["previousblockhash"])
        header.hashMerkleRoot = uint256_from_rpc_hex(block["merkleroot"])
        header.nTime = block["time"]
        header.nBits = int(block["bits"], 16)
        header.nNonce = block["nonce"]

        share = CShareRecord()
        share.parent_share = 0
        share.prev_block_hash = header.hashPrevBlock
        share.candidate_header = header
        share.share_nBits = header.nBits
        share.payout_script = bytes.fromhex("0014" + "11" * 20)

        self.log.info("Relay share from node 0 through node 1 to an observing peer")
        sender.send_and_ping(msg_share([share]))

        self.wait_until(lambda: share.share_id in observer.received_share_ids)
        self.wait_until(lambda: any(received.share_id == share.share_id for received in observer.received_shares))
        assert_equal(observer.received_shares[-1].serialize(), share.serialize())


if __name__ == '__main__':
    SharepoolRelayTest(__file__).main()
