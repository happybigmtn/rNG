#!/usr/bin/env python3
# Copyright (c) 2026-present The RNG developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Exercise RNG's internal miner argument validation and startup path."""

from test_framework.address import ADDRESS_BCRT1_UNSPENDABLE
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class InternalMinerTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.uses_wallet = None

    def run_test(self):
        self.log.info("Restart node 0 with the internal miner enabled on regtest")
        self.restart_node(0, extra_args=[
            "-mine",
            f"-mineaddress={ADDRESS_BCRT1_UNSPENDABLE}",
            "-minethreads=1",
            "-minerandomx=light",
            "-minepriority=normal",
        ])
        self.connect_nodes(0, 1)

        self.log.info("Verify the internal miner reports running with the requested light mode")
        self.wait_until(lambda: self.nodes[0].getinternalmininginfo()["templates"] >= 1)
        mining_info = self.nodes[0].getinternalmininginfo()
        assert mining_info["running"]
        assert_equal(mining_info["threads"], 1)
        assert_equal(mining_info["fast_mode"], False)

        self.stop_node(1)

        self.log.info("Reject invalid RandomX mode values")
        self.nodes[1].assert_start_raises_init_error(
            extra_args=[
                "-mine",
                f"-mineaddress={ADDRESS_BCRT1_UNSPENDABLE}",
                "-minethreads=1",
                "-minerandomx=bogus",
            ],
            expected_msg="Error: minerandomx must be 'fast' or 'light'",
        )

        self.log.info("Reject invalid miner priority values")
        self.nodes[1].assert_start_raises_init_error(
            extra_args=[
                "-mine",
                f"-mineaddress={ADDRESS_BCRT1_UNSPENDABLE}",
                "-minethreads=1",
                "-minepriority=bogus",
            ],
            expected_msg="Error: minepriority must be 'low' or 'normal'",
        )


if __name__ == "__main__":
    InternalMinerTest(__file__).main()
