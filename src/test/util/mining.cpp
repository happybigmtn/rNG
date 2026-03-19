// Copyright (c) 2019-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/mining.h>

#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <crypto/randomx_hash.h>
#include <hash.h>
#include <key_io.h>
#include <node/context.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <test/util/script.h>
#include <util/check.h>
#include <validation.h>
#include <validationinterface.h>
#include <versionbits.h>

#include <algorithm>
#include <map>
#include <memory>

using node::BlockAssembler;
using node::NodeContext;

COutPoint generatetoaddress(const NodeContext& node, const std::string& address)
{
    const auto dest = DecodeDestination(address);
    assert(IsValidDestination(dest));
    BlockAssembler::Options assembler_options;
    assembler_options.coinbase_output_script = GetScriptForDestination(dest);

    return MineBlock(node, assembler_options);
}

std::vector<std::shared_ptr<CBlock>> CreateBlockChain(size_t total_height, const CChainParams& params)
{
    std::vector<std::shared_ptr<CBlock>> ret{total_height};
    auto time{params.GenesisBlock().nTime};

    // Track previous block hashes for seed calculation
    // Map from height to block hash for seed lookups
    std::map<size_t, uint256> block_hashes;
    block_hashes[0] = params.GenesisBlock().GetHash(); // Genesis at height 0

    // NOTE: here `height` does not correspond to the block height but the block height - 1.
    for (size_t height{0}; height < total_height; ++height) {
        CBlock& block{*(ret.at(height) = std::make_shared<CBlock>())};

        CMutableTransaction coinbase_tx;
        coinbase_tx.nLockTime = static_cast<uint32_t>(height);
        coinbase_tx.vin.resize(1);
        coinbase_tx.vin[0].prevout.SetNull();
        coinbase_tx.vin[0].nSequence = CTxIn::MAX_SEQUENCE_NONFINAL; // Make sure timelock is enforced.
        coinbase_tx.vout.resize(1);
        coinbase_tx.vout[0].scriptPubKey = P2WSH_OP_TRUE;
        coinbase_tx.vout[0].nValue = GetBlockSubsidy(height + 1, params.GetConsensus());
        coinbase_tx.vin[0].scriptSig = CScript() << (height + 1) << OP_0;
        block.vtx = {MakeTransactionRef(std::move(coinbase_tx))};

        block.nVersion = VERSIONBITS_LAST_OLD_BLOCK_VERSION;
        block.hashPrevBlock = (height >= 1 ? *ret.at(height - 1) : params.GenesisBlock()).GetHash();
        block.hashMerkleRoot = BlockMerkleRoot(block);
        block.nTime = ++time;
        block.nBits = params.GenesisBlock().nBits;
        block.nNonce = 0;

        // Calculate seed hash for RandomX based on block height (height + 1 since we're building on top)
        uint64_t block_height = height + 1;
        uint64_t seed_height = GetRandomXSeedHeight(block_height);
        uint256 seed_hash;
        if (seed_height == 0) {
            // Use genesis seed
            seed_hash = Hash(std::string(kRandomXGenesisSeedPhrase));
        } else {
            // Use block hash at seed_height
            auto it = block_hashes.find(seed_height);
            if (it != block_hashes.end()) {
                seed_hash = it->second;
            } else {
                // Fallback to genesis seed (shouldn't happen in normal test scenarios)
                seed_hash = Hash(std::string(kRandomXGenesisSeedPhrase));
            }
        }

        // Mine with RandomX
        while (true) {
            uint256 pow_hash = GetBlockPoWHash(block, seed_hash);
            if (CheckProofOfWork(pow_hash, block.nBits, params.GetConsensus())) {
                break;
            }
            ++block.nNonce;
            assert(block.nNonce);
        }

        // Store block hash for future seed lookups
        block_hashes[height + 1] = block.GetHash();
    }
    return ret;
}

COutPoint MineBlock(const NodeContext& node, const node::BlockAssembler::Options& assembler_options)
{
    auto block = PrepareBlock(node, assembler_options);
    auto valid = MineBlock(node, block);
    assert(!valid.IsNull());
    return valid;
}

struct BlockValidationStateCatcher : public CValidationInterface {
    const uint256 m_hash;
    std::optional<BlockValidationState> m_state;

    BlockValidationStateCatcher(const uint256& hash)
        : m_hash{hash},
          m_state{} {}

protected:
    void BlockChecked(const std::shared_ptr<const CBlock>& block, const BlockValidationState& state) override
    {
        if (block->GetHash() != m_hash) return;
        m_state = state;
    }
};

COutPoint MineBlock(const NodeContext& node, std::shared_ptr<CBlock>& block)
{
    // Get the seed hash for RandomX mining based on current chain tip
    const CBlockIndex* pindexPrev = nullptr;
    {
        LOCK(cs_main);
        pindexPrev = Assert(node.chainman)->ActiveChain().Tip();
    }
    uint256 seed_hash = GetRandomXSeedHash(pindexPrev);

    // Mine with RandomX
    while (true) {
        uint256 pow_hash = GetBlockPoWHash(*block, seed_hash);
        if (CheckProofOfWork(pow_hash, block->nBits, Params().GetConsensus())) {
            break;
        }
        ++block->nNonce;
        assert(block->nNonce);
    }

    return ProcessBlock(node, block);
}

COutPoint ProcessBlock(const NodeContext& node, const std::shared_ptr<CBlock>& block)
{
    auto& chainman{*Assert(node.chainman)};
    const auto old_height = WITH_LOCK(chainman.GetMutex(), return chainman.ActiveHeight());
    bool new_block;
    BlockValidationStateCatcher bvsc{block->GetHash()};
    node.validation_signals->RegisterValidationInterface(&bvsc);
    const bool processed{chainman.ProcessNewBlock(block, true, true, &new_block)};
    const bool duplicate{!new_block && processed};
    assert(!duplicate);
    node.validation_signals->UnregisterValidationInterface(&bvsc);
    node.validation_signals->SyncWithValidationInterfaceQueue();
    const bool was_valid{bvsc.m_state && bvsc.m_state->IsValid()};
    assert(old_height + was_valid == WITH_LOCK(chainman.GetMutex(), return chainman.ActiveHeight()));

    if (was_valid) return {block->vtx[0]->GetHash(), 0};
    return {};
}

std::shared_ptr<CBlock> PrepareBlock(const NodeContext& node,
                                     const BlockAssembler::Options& assembler_options)
{
    auto block = std::make_shared<CBlock>(
        BlockAssembler{Assert(node.chainman)->ActiveChainstate(), Assert(node.mempool.get()), assembler_options}
            .CreateNewBlock()
            ->block);

    LOCK(cs_main);
    block->nTime = Assert(node.chainman)->ActiveChain().Tip()->GetMedianTimePast() + 1;
    block->hashMerkleRoot = BlockMerkleRoot(*block);

    return block;
}
std::shared_ptr<CBlock> PrepareBlock(const NodeContext& node, const CScript& coinbase_scriptPubKey)
{
    BlockAssembler::Options assembler_options;
    assembler_options.coinbase_output_script = coinbase_scriptPubKey;
    ApplyArgsManOptions(*node.args, assembler_options);
    return PrepareBlock(node, assembler_options);
}
