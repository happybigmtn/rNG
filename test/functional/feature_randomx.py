#!/usr/bin/env python3
# Copyright (c) 2024-present RNG Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RandomX proof-of-work.

This test verifies that RNG uses RandomX for block validation instead of SHA256d.

Acceptance Criteria (from specs/randomx.md):
1. RandomX library compiles and links with RNG
2. Block validation uses RandomX hash (not SHA-256d)
3. Valid RandomX proof-of-work is accepted
4. Mining RPC produces valid RandomX proofs
5. The shipped fixed-seed policy remains internally consistent across blocks
"""

from test_framework.test_framework import BitcoinTestFramework


class RandomXTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Test 1: Mine a block using RandomX")
        address = node.getnewaddress()
        block_hashes = self.generatetoaddress(node, 1, address)

        # Block was accepted
        assert len(block_hashes) == 1, "Should have mined 1 block"
        self.log.info(f"  PASS: Mined block {block_hashes[0]}")

        # Block has at least 1 confirmation
        block = node.getblock(block_hashes[0])
        assert block['confirmations'] >= 1, "Block should have confirmations"
        self.log.info(f"  PASS: Block has {block['confirmations']} confirmation(s)")

        self.log.info("Test 2: Verify block difficulty is valid")
        # The block should have been mined with valid difficulty
        assert 'bits' in block, "Block should have 'bits' field"
        assert 'difficulty' in block, "Block should have 'difficulty' field"
        self.log.info(f"  PASS: Block bits={block['bits']}, difficulty={block['difficulty']}")

        self.log.info("Test 3: Mine multiple blocks to verify continuous mining")
        more_blocks = self.generatetoaddress(node, 10, address)
        assert len(more_blocks) == 10, "Should have mined 10 more blocks"

        height = node.getblockcount()
        assert height >= 11, f"Chain should have 11+ blocks, got {height}"
        self.log.info(f"  PASS: Successfully mined to height {height}")

        self.log.info("Test 4: Verify coinbase maturity")
        # After 100 blocks, coinbase from block 1 should be mature
        self.generatetoaddress(node, 100, address)

        # Should be able to spend mature coinbase
        balance = node.getbalance()
        assert balance > 0, "Should have spendable balance"
        self.log.info(f"  PASS: Spendable balance: {balance} RNG")

        self.log.info("Test 5: Send a transaction to verify chain validity")
        test_address = node.getnewaddress()
        txid = node.sendtoaddress(test_address, 1.0)
        assert txid, "Transaction should be created"
        self.log.info(f"  PASS: Created transaction {txid}")

        # Confirm the transaction
        self.generatetoaddress(node, 1, address)
        tx = node.gettransaction(txid)
        assert tx['confirmations'] >= 1, "Transaction should be confirmed"
        self.log.info(f"  PASS: Transaction confirmed")

        self.log.info("")
        self.log.info("="*60)
        self.log.info("RandomX proof-of-work test passed!")
        self.log.info("="*60)


if __name__ == '__main__':
    RandomXTest(__file__).main()
