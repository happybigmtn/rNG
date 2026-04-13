// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/sharepool.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <validation.h>
#include <versionbits.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/test/unit_test.hpp>

namespace {

CScript CoinbaseScript()
{
    return CScript{} << OP_0 << std::vector<unsigned char>(20, 0x42);
}

class FakeVersionBitsChain
{
public:
    FakeVersionBitsChain(const CChainParams& params, int tip_height, bool signal_sharepool)
        : m_hashes(tip_height + 1),
          m_indexes(tip_height + 1)
    {
        const auto& consensus{params.GetConsensus()};
        const int32_t signal_version{
            VERSIONBITS_TOP_BITS | (int32_t{1} << consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].bit)};
        const int32_t block_version{signal_sharepool ? signal_version : VERSIONBITS_LAST_OLD_BLOCK_VERSION};

        uint256 prev_hash{params.GenesisBlock().GetHash()};
        for (int height{0}; height <= tip_height; ++height) {
            CBlockHeader header;
            header.nVersion = block_version;
            header.hashPrevBlock = prev_hash;
            header.hashMerkleRoot = uint256::ONE;
            header.nTime = params.GenesisBlock().nTime + height + 1;
            header.nBits = params.GenesisBlock().nBits;
            header.nNonce = static_cast<uint32_t>(height);

            m_hashes[height] = header.GetHash();
            m_indexes[height] = std::make_unique<CBlockIndex>(header);
            m_indexes[height]->nHeight = height;
            m_indexes[height]->pprev = height == 0 ? nullptr : m_indexes[height - 1].get();
            m_indexes[height]->phashBlock = &m_hashes[height];
            prev_hash = m_hashes[height];
        }
    }

    CBlockIndex* Tip() const { return m_indexes.back().get(); }

private:
    std::vector<uint256> m_hashes;
    std::vector<std::unique_ptr<CBlockIndex>> m_indexes;
};

struct SharepoolEnforcementSetup : public TestingSetup {
    SharepoolEnforcementSetup()
        : TestingSetup{
              ChainType::REGTEST,
              {.extra_args = {"-vbparams=sharepool:0:9999999999:0", "-testactivationheight=csv@999999"}}}
    {
    }

    CBlock BuildCandidateBlock(const FakeVersionBitsChain& chain,
                               const std::vector<CTxOut>& coinbase_outputs,
                               const std::vector<CMutableTransaction>& txns = {}) const
    {
        const int height{chain.Tip()->nHeight + 1};

        CMutableTransaction coinbase;
        coinbase.vin.resize(1);
        coinbase.vin[0].prevout.SetNull();
        coinbase.vin[0].nSequence = CTxIn::MAX_SEQUENCE_NONFINAL;
        coinbase.vin[0].scriptSig = CScript{} << height << OP_0;
        coinbase.nLockTime = static_cast<uint32_t>(height - 1);
        coinbase.vout = coinbase_outputs;

        CBlock block;
        block.nVersion = VERSIONBITS_LAST_OLD_BLOCK_VERSION;
        block.hashPrevBlock = chain.Tip()->GetBlockHash();
        block.nTime = Params().GenesisBlock().nTime + height + 1;
        block.nBits = Params().GenesisBlock().nBits;
        block.nNonce = 0;
        block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
        for (const CMutableTransaction& tx : txns) {
            block.vtx.push_back(MakeTransactionRef(tx));
        }
        block.hashMerkleRoot = BlockMerkleRoot(block);
        return block;
    }

    std::vector<CTxOut> LegacyCoinbaseOutputs(int height) const
    {
        return {CTxOut{GetBlockSubsidy(height, Params().GetConsensus()), CoinbaseScript()}};
    }

    std::vector<CTxOut> SharepoolCoinbaseOutputs(int height) const
    {
        const CAmount reward{GetBlockSubsidy(height, Params().GetConsensus())};
        return {
            CTxOut{/*nValue=*/0, CoinbaseScript()},
            CTxOut{reward, consensus::sharepool::BuildSettlementScriptPubKey(uint256::ONE)},
        };
    }

    BlockValidationState ConnectCandidateBlock(
        const FakeVersionBitsChain& chain,
        const CBlock& block,
        const std::vector<std::pair<COutPoint, Coin>>& coins = {})
    {
        CCoinsView backing_view;
        CCoinsViewCache view{&backing_view};
        view.SetBestBlock(chain.Tip()->GetBlockHash());
        for (const auto& [outpoint, coin] : coins) {
            view.AddCoin(outpoint, Coin{coin}, /*possible_overwrite=*/false);
        }

        uint256 block_hash{block.GetHash()};
        CBlockIndex index{block};
        index.pprev = chain.Tip();
        index.nHeight = chain.Tip()->nHeight + 1;
        index.phashBlock = &block_hash;

        BlockValidationState state;
        LOCK(::cs_main);
        m_node.chainman->m_versionbitscache.Clear();
        if (!m_node.chainman->ActiveChainstate().ConnectBlock(block, state, &index, view, /*fJustCheck=*/true)) {
            BOOST_REQUIRE_MESSAGE(!state.IsValid(), "ConnectBlock returned false without invalid state");
        }
        return state;
    }

    void CheckRejected(const FakeVersionBitsChain& chain, const CBlock& block, std::string_view reason)
    {
        const BlockValidationState state{ConnectCandidateBlock(chain, block)};
        BOOST_REQUIRE_MESSAGE(!state.IsValid(), "block unexpectedly accepted");
        BOOST_CHECK_EQUAL(state.GetRejectReason(), reason);
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(sharepool_enforcement_tests, SharepoolEnforcementSetup)

BOOST_AUTO_TEST_CASE(pool_07g_connectblock_enforcement_scenarios)
{
    FakeVersionBitsChain preactivation_chain{Params(), /*tip_height=*/100, /*signal_sharepool=*/false};
    const int preactivation_height{preactivation_chain.Tip()->nHeight + 1};
    const BlockValidationState preactivation_state{ConnectCandidateBlock(
        preactivation_chain,
        BuildCandidateBlock(preactivation_chain, LegacyCoinbaseOutputs(preactivation_height)))};
    BOOST_CHECK_MESSAGE(preactivation_state.IsValid(), preactivation_state.ToString());

    FakeVersionBitsChain active_chain{Params(), /*tip_height=*/432, /*signal_sharepool=*/true};
    const int active_height{active_chain.Tip()->nHeight + 1};
    const std::vector<CTxOut> valid_outputs{SharepoolCoinbaseOutputs(active_height)};

    const BlockValidationState valid_state{ConnectCandidateBlock(
        active_chain,
        BuildCandidateBlock(active_chain, valid_outputs))};
    BOOST_CHECK_MESSAGE(valid_state.IsValid(), valid_state.ToString());

    CheckRejected(
        active_chain,
        BuildCandidateBlock(active_chain, {valid_outputs.at(0)}),
        "bad-cb-settlement-count");

    std::vector<CTxOut> wrong_value_outputs{valid_outputs};
    BOOST_REQUIRE_GT(wrong_value_outputs.at(1).nValue, 0);
    wrong_value_outputs.at(1).nValue -= 1;
    CheckRejected(
        active_chain,
        BuildCandidateBlock(active_chain, wrong_value_outputs),
        "bad-cb-settlement-value");

    std::vector<CTxOut> duplicate_outputs{valid_outputs};
    CTxOut duplicate_settlement{valid_outputs.at(1)};
    duplicate_settlement.nValue = 0;
    duplicate_outputs.push_back(duplicate_settlement);
    CheckRejected(
        active_chain,
        BuildCandidateBlock(active_chain, duplicate_outputs),
        "bad-cb-settlement-count");

    constexpr CAmount settlement_value{10 * COIN};
    const COutPoint settlement_outpoint{Txid::FromUint256(uint256::ONE), 0};
    CMutableTransaction malformed_claim;
    malformed_claim.vin.emplace_back(settlement_outpoint);
    malformed_claim.vout.emplace_back(settlement_value, CScript{} << OP_TRUE);

    const CBlock malformed_claim_block{BuildCandidateBlock(active_chain, valid_outputs, {malformed_claim})};
    const BlockValidationState malformed_claim_state{ConnectCandidateBlock(
        active_chain,
        malformed_claim_block,
        {{settlement_outpoint,
          Coin{CTxOut{settlement_value, consensus::sharepool::BuildSettlementScriptPubKey(uint256::ONE)},
               active_height - 1,
               /*fCoinBaseIn=*/false}}})};
    BOOST_REQUIRE_MESSAGE(!malformed_claim_state.IsValid(), "malformed settlement witness spend unexpectedly accepted");
    BOOST_CHECK_NE(malformed_claim_state.GetRejectReason().find("Sharepool settlement witness stack has incorrect size"), std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
