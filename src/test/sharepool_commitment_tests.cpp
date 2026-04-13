// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/sharepool.h>

#include <hash.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <util/fs.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <fstream>
#include <stdexcept>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using consensus::sharepool::ComputeRemainingSettlementValue;
using consensus::sharepool::ComputeSettlementClaimStatusBranch;
using consensus::sharepool::ComputeSettlementClaimStatusRoot;
using consensus::sharepool::ComputeSettlementMerkleRootFromBranch;
using consensus::sharepool::ComputeSettlementPayoutBranch;
using consensus::sharepool::ComputeSettlementPayoutRoot;
using consensus::sharepool::ComputeSettlementStateHash;
using consensus::sharepool::HashSettlementDescriptor;
using consensus::sharepool::HashSettlementLeaf;
using consensus::sharepool::SettlementDescriptor;
using consensus::sharepool::SettlementLeaf;
using consensus::sharepool::SettlementStatusTreeSize;
using consensus::sharepool::SortSettlementLeaves;

fs::path SourceRoot()
{
    fs::path source{fs::absolute(fs::PathFromString(__FILE__))};
    return source.parent_path().parent_path().parent_path();
}

UniValue ReadVectors()
{
    const fs::path path{SourceRoot() / "contrib" / "sharepool" / "reports" / "pool-07b-settlement-vectors.json"};
    std::ifstream file{path};
    BOOST_REQUIRE_MESSAGE(file.is_open(), "failed to open settlement vectors: " + fs::PathToString(path));

    std::ostringstream contents;
    contents << file.rdbuf();

    UniValue root;
    BOOST_REQUIRE_MESSAGE(root.read(contents.str()), "failed to parse settlement vectors json");
    return root;
}

uint256 ParseRawHashHex(const std::string& hex)
{
    const std::vector<unsigned char> bytes{ParseHex(hex)};
    BOOST_REQUIRE_EQUAL(bytes.size(), uint256::size());
    return uint256{std::span<const unsigned char>{bytes.data(), bytes.size()}};
}

std::string RawHashHex(const uint256& value)
{
    return HexStr(std::span<const unsigned char>{value.begin(), value.size()});
}

uint256 SettlementShareIdToken(const std::string& token)
{
    if (token.size() == 64 && IsHex(token)) {
        return ParseRawHashHex(token);
    }

    HashWriter writer;
    const std::string_view tag{"RNGShareId"};
    writer.write(MakeByteSpan(tag));
    writer.write(MakeByteSpan(std::string_view{token}));
    return writer.GetHash();
}

SettlementLeaf ParseLeaf(const UniValue& value)
{
    const std::vector<unsigned char> payout_script_bytes{ParseHex(value["payout_script_hex"].get_str())};
    return SettlementLeaf{
        .payout_script = CScript{payout_script_bytes.begin(), payout_script_bytes.end()},
        .amount_roshi = value["amount_roshi"].getInt<int64_t>(),
        .first_share_id = SettlementShareIdToken(value["first_share_id"].get_str()),
        .last_share_id = SettlementShareIdToken(value["last_share_id"].get_str()),
    };
}

std::vector<SettlementLeaf> ParseLeaves(const UniValue& array)
{
    std::vector<SettlementLeaf> leaves;
    leaves.reserve(array.size());
    for (const UniValue& value : array.getValues()) {
        leaves.push_back(ParseLeaf(value));
    }
    return leaves;
}

std::vector<bool> ParseClaimedFlags(const UniValue& array)
{
    std::vector<bool> claimed;
    claimed.reserve(array.size());
    for (const UniValue& value : array.getValues()) {
        claimed.push_back(value.getInt<int>() != 0);
    }
    return claimed;
}

std::vector<uint256> ParseBranch(const UniValue& array)
{
    std::vector<uint256> branch;
    branch.reserve(array.size());
    for (const UniValue& value : array.getValues()) {
        branch.push_back(ParseRawHashHex(value.get_str()));
    }
    return branch;
}

const UniValue& FindScenario(const UniValue& root, std::string_view scenario)
{
    for (const UniValue& entry : root["vectors"].getValues()) {
        if (entry["scenario"].get_str() == scenario) return entry;
    }
    throw std::runtime_error("missing settlement vector scenario: " + std::string{scenario});
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(sharepool_commitment_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(sharepool_initial_state_matches_committed_vectors)
{
    const UniValue vectors{ReadVectors()};
    const UniValue& initial{FindScenario(vectors, "initial_state")};

    const std::vector<SettlementLeaf> leaves{ParseLeaves(initial["leaves"])};
    const auto ordered{SortSettlementLeaves(leaves)};
    const std::vector<bool> claimed_flags{ParseClaimedFlags(initial["claimed_flags"])};

    const SettlementDescriptor descriptor{
        .version = initial["descriptor"]["version"].getInt<uint64_t>(),
        .payout_root = ComputeSettlementPayoutRoot(ordered),
        .leaf_count = static_cast<uint32_t>(ordered.size()),
    };

    BOOST_CHECK_EQUAL(SettlementStatusTreeSize(ordered.size()), vectors["metadata"]["status_tree_size"].getInt<size_t>());
    BOOST_CHECK_EQUAL(RawHashHex(descriptor.payout_root), initial["descriptor"]["payout_root"].get_str());
    BOOST_CHECK_EQUAL(RawHashHex(HashSettlementDescriptor(descriptor)), initial["descriptor"]["descriptor_hash"].get_str());

    const uint256 claim_status_root{ComputeSettlementClaimStatusRoot(ordered.size(), claimed_flags)};
    BOOST_CHECK_EQUAL(RawHashHex(claim_status_root), initial["claim_status_root"].get_str());
    BOOST_CHECK_EQUAL(RawHashHex(ComputeSettlementStateHash(descriptor, claim_status_root)), initial["state_hash"].get_str());
    BOOST_CHECK_EQUAL(ComputeRemainingSettlementValue(ordered, claimed_flags), initial["remaining_value_roshi"].getInt<int64_t>());
}

BOOST_AUTO_TEST_CASE(sharepool_claim_branches_reconstruct_committed_roots)
{
    const UniValue vectors{ReadVectors()};
    const UniValue& initial{FindScenario(vectors, "initial_state")};
    const UniValue& valid_claim{FindScenario(vectors, "one_valid_claim_transition")};

    const std::vector<SettlementLeaf> ordered{SortSettlementLeaves(ParseLeaves(initial["leaves"]))};
    const std::vector<bool> initial_flags{ParseClaimedFlags(initial["claimed_flags"])};
    const size_t index{valid_claim["leaf_index"].getInt<size_t>()};

    BOOST_REQUIRE_LT(index, ordered.size());
    const uint256 payout_root{ComputeSettlementPayoutRoot(ordered)};
    const uint256 payout_root_from_branch{
        ComputeSettlementMerkleRootFromBranch(
            HashSettlementLeaf(ordered[index]),
            index,
            ParseBranch(valid_claim["payout_branch"]))
    };
    BOOST_CHECK_EQUAL(RawHashHex(payout_root_from_branch), RawHashHex(payout_root));

    const uint256 old_status_root_from_branch{
        ComputeSettlementMerkleRootFromBranch(
            consensus::sharepool::HashSettlementClaimFlag(index, initial_flags[index]),
            index,
            ParseBranch(valid_claim["status_branch"]))
    };
    BOOST_CHECK_EQUAL(RawHashHex(old_status_root_from_branch), valid_claim["old_claim_status_root"].get_str());
    BOOST_CHECK_EQUAL(RawHashHex(ComputeSettlementClaimStatusRoot(ordered.size(), initial_flags)), valid_claim["old_claim_status_root"].get_str());

    std::vector<bool> new_flags{initial_flags};
    new_flags[index] = true;
    BOOST_CHECK_EQUAL(RawHashHex(ComputeSettlementClaimStatusRoot(ordered.size(), new_flags)), valid_claim["new_claim_status_root"].get_str());

    const SettlementDescriptor descriptor{
        .version = initial["descriptor"]["version"].getInt<uint64_t>(),
        .payout_root = payout_root,
        .leaf_count = static_cast<uint32_t>(ordered.size()),
    };
    BOOST_CHECK_EQUAL(
        RawHashHex(ComputeSettlementStateHash(descriptor, ComputeSettlementClaimStatusRoot(ordered.size(), new_flags))),
        valid_claim["new_state_hash"].get_str());

    const auto computed_branch{ComputeSettlementPayoutBranch(ordered, index)};
    const auto computed_status_branch{ComputeSettlementClaimStatusBranch(ordered.size(), initial_flags, index)};
    const auto expected_payout_branch{ParseBranch(valid_claim["payout_branch"])};
    const auto expected_status_branch{ParseBranch(valid_claim["status_branch"])};
    BOOST_REQUIRE_EQUAL(computed_branch.size(), expected_payout_branch.size());
    BOOST_REQUIRE_EQUAL(computed_status_branch.size(), expected_status_branch.size());
    for (size_t pos{0}; pos < computed_branch.size(); ++pos) {
        BOOST_CHECK_EQUAL(RawHashHex(computed_branch[pos]), RawHashHex(expected_payout_branch[pos]));
    }
    for (size_t pos{0}; pos < computed_status_branch.size(); ++pos) {
        BOOST_CHECK_EQUAL(RawHashHex(computed_status_branch[pos]), RawHashHex(expected_status_branch[pos]));
    }
}

BOOST_AUTO_TEST_CASE(sharepool_final_claim_and_fee_funding_vectors_match)
{
    const UniValue vectors{ReadVectors()};
    const UniValue& initial{FindScenario(vectors, "initial_state")};
    const UniValue& valid_claim{FindScenario(vectors, "one_valid_claim_transition")};
    const UniValue& fee_case{FindScenario(vectors, "non_settlement_fee_funding")};
    const UniValue& final_case{FindScenario(vectors, "final_claim_transition")};

    const std::vector<SettlementLeaf> ordered{SortSettlementLeaves(ParseLeaves(initial["leaves"]))};
    const std::vector<bool> initial_flags{ParseClaimedFlags(initial["claimed_flags"])};

    const size_t fee_index{fee_case["leaf_index"].getInt<size_t>()};
    std::vector<bool> fee_flags{initial_flags};
    fee_flags[fee_index] = true;
    const int64_t total_reward{vectors["metadata"]["total_reward_roshi"].getInt<int64_t>()};
    const int64_t settlement_outputs_after_fee_case{
        fee_case["template"]["payout_output_value"].getInt<int64_t>() +
        fee_case["template"]["successor_output_value"].getInt<int64_t>()
    };
    BOOST_CHECK_EQUAL(settlement_outputs_after_fee_case, total_reward);
    BOOST_CHECK_EQUAL(ComputeRemainingSettlementValue(ordered, fee_flags), fee_case["template"]["successor_output_value"].getInt<int64_t>());

    std::vector<bool> almost_final_flags{ParseClaimedFlags(valid_claim["new_claimed_flags"])};
    almost_final_flags[0] = true;
    const size_t final_index{final_case["leaf_index"].getInt<size_t>()};
    BOOST_REQUIRE_LT(final_index, ordered.size());

    const SettlementDescriptor descriptor{
        .version = initial["descriptor"]["version"].getInt<uint64_t>(),
        .payout_root = ComputeSettlementPayoutRoot(ordered),
        .leaf_count = static_cast<uint32_t>(ordered.size()),
    };
    const uint256 almost_final_status_root{ComputeSettlementClaimStatusRoot(ordered.size(), almost_final_flags)};
    BOOST_CHECK_EQUAL(RawHashHex(almost_final_status_root), final_case["old_claim_status_root"].get_str());
    BOOST_CHECK_EQUAL(RawHashHex(ComputeSettlementStateHash(descriptor, almost_final_status_root)), final_case["old_state_hash"].get_str());

    BOOST_CHECK(!almost_final_flags[final_index]);
    BOOST_CHECK_EQUAL(ComputeRemainingSettlementValue(ordered, almost_final_flags), final_case["template"]["payout_output_value"].getInt<int64_t>());

    almost_final_flags[final_index] = true;
    BOOST_CHECK_EQUAL(ComputeRemainingSettlementValue(ordered, almost_final_flags), 0);
}

BOOST_AUTO_TEST_SUITE_END()
