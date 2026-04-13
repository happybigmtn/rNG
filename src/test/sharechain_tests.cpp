// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/sharechain.h>

#include <arith_uint256.h>
#include <chainparams.h>
#include <dbwrapper.h>
#include <hash.h>
#include <pow.h>
#include <primitives/block.h>
#include <script/script.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/fs.h>

#include <boost/test/unit_test.hpp>

#include <memory>
#include <string>
#include <vector>

namespace {

CScript PayoutScript(unsigned char marker)
{
    std::vector<unsigned char> keyhash(20, marker);
    return CScript() << OP_0 << keyhash;
}

node::ShareRecord MakeShare(const Consensus::Params& consensus,
                            const uint256& parent_share,
                            const uint256& prev_block_hash,
                            unsigned char payout_marker)
{
    node::ShareRecord share;
    share.parent_share = parent_share;
    share.prev_block_hash = prev_block_hash;
    share.candidate_header.nVersion = 0x20000000;
    share.candidate_header.hashPrevBlock = prev_block_hash;
    share.candidate_header.hashMerkleRoot = Hash(std::vector<unsigned char>{payout_marker});
    share.candidate_header.nTime = 1'700'000'000 + payout_marker;
    share.candidate_header.nBits = UintToArith256(consensus.powLimit).GetCompact();
    share.candidate_header.nNonce = 0;
    share.share_nBits = share.candidate_header.nBits;
    share.payout_script = PayoutScript(payout_marker);

    const uint256 seed_hash{Hash(std::string(kRandomXGenesisSeedPhrase))};
    while (!CheckProofOfWork(GetBlockPoWHash(share.candidate_header, seed_hash), share.share_nBits, consensus)) {
        ++share.candidate_header.nNonce;
    }
    return share;
}

node::ShareRecord MakeInvalidPowShare(const Consensus::Params& consensus)
{
    node::ShareRecord share{MakeShare(consensus, uint256{}, uint256::ONE, 1)};
    const uint256 seed_hash{Hash(std::string(kRandomXGenesisSeedPhrase))};
    do {
        ++share.candidate_header.nNonce;
    } while (CheckProofOfWork(GetBlockPoWHash(share.candidate_header, seed_hash), share.share_nBits, consensus));
    return share;
}

std::unique_ptr<CDBWrapper> MakeMemoryDB()
{
    return std::make_unique<CDBWrapper>(DBParams{
        .path = "",
        .cache_bytes = 1 << 20,
        .memory_only = true,
        .wipe_data = true,
        .obfuscate = false,
    });
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(sharechain_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(share_record_serializes_deterministically)
{
    const auto params{CreateChainParams(*m_node.args, ChainType::REGTEST)};
    const node::ShareRecord share{MakeShare(params->GetConsensus(), uint256{}, uint256::ONE, 42)};

    DataStream first{};
    first << share;
    DataStream second{};
    second << share;
    BOOST_CHECK_EQUAL_COLLECTIONS(first.begin(), first.end(), second.begin(), second.end());

    node::ShareRecord decoded;
    first >> decoded;

    BOOST_CHECK_EQUAL(decoded.version, share.version);
    BOOST_CHECK_EQUAL(decoded.parent_share, share.parent_share);
    BOOST_CHECK_EQUAL(decoded.prev_block_hash, share.prev_block_hash);
    BOOST_CHECK_EQUAL(decoded.candidate_header.GetHash(), share.candidate_header.GetHash());
    BOOST_CHECK_EQUAL(decoded.share_nBits, share.share_nBits);
    BOOST_CHECK(decoded.payout_script == share.payout_script);
    BOOST_CHECK_EQUAL(decoded.GetHash(), share.GetHash());
}

BOOST_AUTO_TEST_CASE(sharechain_inserts_linked_chain_and_selects_tip)
{
    const auto params{CreateChainParams(*m_node.args, ChainType::REGTEST)};
    node::SharechainStore store{MakeMemoryDB()};
    const node::ShareRecord root{MakeShare(params->GetConsensus(), uint256{}, uint256::ONE, 1)};
    const node::ShareRecord child{MakeShare(params->GetConsensus(), root.GetHash(), uint256::ONE, 2)};

    const auto root_result{store.AddShare(root, params->GetConsensus())};
    BOOST_CHECK_EQUAL(root_result.status, node::ShareStoreStatus::ACCEPTED);
    BOOST_CHECK_EQUAL(root_result.accepted_ids.size(), 1U);
    BOOST_CHECK_EQUAL(store.BestTip(), root.GetHash());
    BOOST_CHECK_EQUAL(store.Height(root.GetHash()).value(), 0);

    const auto child_result{store.AddShare(child, params->GetConsensus())};
    BOOST_CHECK_EQUAL(child_result.status, node::ShareStoreStatus::ACCEPTED);
    BOOST_CHECK_EQUAL(store.BestTip(), child.GetHash());
    BOOST_CHECK_EQUAL(store.Height(child.GetHash()).value(), 1);
    BOOST_CHECK_EQUAL(store.ShareCount(), 2U);
}

BOOST_AUTO_TEST_CASE(sharechain_buffers_and_resolves_orphans)
{
    const auto params{CreateChainParams(*m_node.args, ChainType::REGTEST)};
    node::SharechainStore store{MakeMemoryDB()};
    const node::ShareRecord root{MakeShare(params->GetConsensus(), uint256{}, uint256::ONE, 1)};
    const node::ShareRecord child{MakeShare(params->GetConsensus(), root.GetHash(), uint256::ONE, 2)};

    const auto orphan_result{store.AddShare(child, params->GetConsensus())};
    BOOST_CHECK_EQUAL(orphan_result.status, node::ShareStoreStatus::ORPHAN);
    BOOST_REQUIRE(orphan_result.missing_parent.has_value());
    BOOST_CHECK_EQUAL(*orphan_result.missing_parent, root.GetHash());
    BOOST_CHECK_EQUAL(store.OrphanCount(), 1U);
    BOOST_CHECK(store.BestTip().IsNull());

    const auto root_result{store.AddShare(root, params->GetConsensus())};
    BOOST_CHECK_EQUAL(root_result.status, node::ShareStoreStatus::ACCEPTED);
    BOOST_REQUIRE_EQUAL(root_result.accepted_ids.size(), 2U);
    BOOST_CHECK_EQUAL(root_result.accepted_ids[0], root.GetHash());
    BOOST_CHECK_EQUAL(root_result.accepted_ids[1], child.GetHash());
    BOOST_CHECK_EQUAL(store.OrphanCount(), 0U);
    BOOST_CHECK_EQUAL(store.BestTip(), child.GetHash());
}

BOOST_AUTO_TEST_CASE(sharechain_orphan_buffer_is_bounded)
{
    const auto params{CreateChainParams(*m_node.args, ChainType::REGTEST)};
    node::SharechainStore store{MakeMemoryDB()};

    for (unsigned char i = 0; i < node::MAX_SHARE_ORPHANS + 1; ++i) {
        uint256 missing_parent{ArithToUint256(arith_uint256{i + 1U})};
        const node::ShareRecord orphan{MakeShare(params->GetConsensus(), missing_parent, uint256::ONE, static_cast<unsigned char>(i + 1))};
        const auto result{store.AddShare(orphan, params->GetConsensus())};
        BOOST_CHECK_EQUAL(result.status, node::ShareStoreStatus::ORPHAN);
    }

    BOOST_CHECK_EQUAL(store.OrphanCount(), node::MAX_SHARE_ORPHANS);
}

BOOST_AUTO_TEST_CASE(sharechain_rejects_invalid_pow)
{
    const auto params{CreateChainParams(*m_node.args, ChainType::REGTEST)};
    node::SharechainStore store{MakeMemoryDB()};
    const node::ShareRecord invalid_share{MakeInvalidPowShare(params->GetConsensus())};

    const auto result{store.AddShare(invalid_share, params->GetConsensus())};
    BOOST_CHECK_EQUAL(result.status, node::ShareStoreStatus::INVALID);
    BOOST_CHECK_EQUAL(store.ShareCount(), 0U);
    BOOST_CHECK(!result.reject_reason.empty());
}

BOOST_AUTO_TEST_CASE(sharechain_persists_records_to_leveldb)
{
    const auto params{CreateChainParams(*m_node.args, ChainType::REGTEST)};
    const fs::path db_path{m_args.GetDataDirNet() / "sharechain-test"};
    uint256 expected_tip;

    {
        node::SharechainStore store{DBParams{
            .path = db_path,
            .cache_bytes = 1 << 20,
            .memory_only = false,
            .wipe_data = true,
            .obfuscate = false,
        }};
        const node::ShareRecord root{MakeShare(params->GetConsensus(), uint256{}, uint256::ONE, 1)};
        const node::ShareRecord child{MakeShare(params->GetConsensus(), root.GetHash(), uint256::ONE, 2)};
        BOOST_CHECK_EQUAL(store.AddShare(root, params->GetConsensus()).status, node::ShareStoreStatus::ACCEPTED);
        BOOST_CHECK_EQUAL(store.AddShare(child, params->GetConsensus()).status, node::ShareStoreStatus::ACCEPTED);
        expected_tip = child.GetHash();
        BOOST_CHECK_EQUAL(store.BestTip(), expected_tip);
    }

    node::SharechainStore reloaded{DBParams{
        .path = db_path,
        .cache_bytes = 1 << 20,
        .memory_only = false,
        .wipe_data = false,
        .obfuscate = false,
    }};
    BOOST_CHECK_EQUAL(reloaded.ShareCount(), 2U);
    BOOST_CHECK_EQUAL(reloaded.BestTip(), expected_tip);
    BOOST_CHECK_EQUAL(reloaded.OrphanCount(), 0U);
}

BOOST_AUTO_TEST_SUITE_END()
