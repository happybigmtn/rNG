// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
#include <consensus/sharepool.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <span.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <test/util/transaction_utils.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using consensus::sharepool::BuildSettlementDescriptor;
using consensus::sharepool::BuildSettlementScriptPubKey;
using consensus::sharepool::ComputeInitialSettlementClaimStatusRoot;
using consensus::sharepool::ComputeSettlementClaimStatusBranch;
using consensus::sharepool::ComputeSettlementClaimStatusRoot;
using consensus::sharepool::ComputeSettlementPayoutBranch;
using consensus::sharepool::ComputeSettlementStateHash;
using consensus::sharepool::MakeSoloSettlementLeaf;
using consensus::sharepool::SettlementDescriptor;
using consensus::sharepool::SettlementLeaf;
using consensus::sharepool::SortSettlementLeaves;

constexpr unsigned int BASE_WITNESS_FLAGS{SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS};
constexpr unsigned int SHAREPOOL_WITNESS_FLAGS{BASE_WITNESS_FLAGS | SCRIPT_VERIFY_SHAREPOOL};

template <typename T>
std::vector<unsigned char> SerializeToBytes(const T& value)
{
    DataStream stream{};
    stream << value;
    const auto bytes{MakeUCharSpan(stream)};
    return {bytes.begin(), bytes.end()};
}

std::vector<unsigned char> SerializeBranch(const std::vector<uint256>& branch)
{
    std::vector<unsigned char> serialized;
    serialized.reserve(branch.size() * uint256::size());
    for (const uint256& hash : branch) {
        serialized.insert(serialized.end(), hash.begin(), hash.end());
    }
    return serialized;
}

CScript PayoutScript(unsigned char marker)
{
    return CScript() << OP_0 << std::vector<unsigned char>(20, marker);
}

SettlementLeaf SharepoolLeaf(unsigned char marker, int64_t amount_roshi)
{
    const uint256 first_share_id{Hash(std::vector<unsigned char>{marker, 0})};
    const uint256 last_share_id{Hash(std::vector<unsigned char>{marker, 1})};
    return SettlementLeaf{
        .payout_script = PayoutScript(marker),
        .amount_roshi = amount_roshi,
        .first_share_id = first_share_id,
        .last_share_id = last_share_id,
    };
}

struct ClaimFixture {
    std::vector<SettlementLeaf> leaves;
    SettlementDescriptor descriptor;
    std::vector<bool> claimed_flags;
    size_t leaf_index{0};
    CScript script_pub_key;
    CScriptWitness witness;
};

ClaimFixture BuildClaimFixture(std::vector<SettlementLeaf> leaves, size_t leaf_index, std::vector<bool> claimed_flags = {})
{
    leaves = SortSettlementLeaves(std::move(leaves));
    if (claimed_flags.empty()) {
        claimed_flags.assign(leaves.size(), false);
    }

    const SettlementDescriptor descriptor{BuildSettlementDescriptor(leaves)};
    const uint256 claim_status_root{ComputeSettlementClaimStatusRoot(leaves.size(), claimed_flags)};
    const uint256 state_hash{ComputeSettlementStateHash(descriptor, claim_status_root)};

    CScriptWitness witness;
    witness.stack.push_back(SerializeToBytes(descriptor));
    witness.stack.push_back(CScriptNum(static_cast<int64_t>(leaf_index)).getvch());
    witness.stack.push_back(SerializeToBytes(leaves.at(leaf_index)));
    witness.stack.push_back(SerializeBranch(ComputeSettlementPayoutBranch(leaves, leaf_index)));
    witness.stack.push_back(SerializeBranch(ComputeSettlementClaimStatusBranch(leaves.size(), claimed_flags, leaf_index)));

    return ClaimFixture{
        .leaves = std::move(leaves),
        .descriptor = descriptor,
        .claimed_flags = std::move(claimed_flags),
        .leaf_index = leaf_index,
        .script_pub_key = BuildSettlementScriptPubKey(state_hash),
        .witness = std::move(witness),
    };
}

bool VerifyWitnessSpend(const CScript& script_pub_key, const CScriptWitness& witness, unsigned int flags, ScriptError& err)
{
    const CTransaction credit{BuildCreditingTransaction(script_pub_key, 1)};
    const CMutableTransaction spend{BuildSpendingTransaction(CScript{}, witness, credit)};
    return VerifyScript(
        CScript{},
        script_pub_key,
        &witness,
        flags,
        MutableTransactionSignatureChecker(&spend, 0, credit.vout[0].nValue, MissingDataBehavior::ASSERT_FAIL),
        &err);
}

void CheckSharepoolFailure(const CScript& script_pub_key, const CScriptWitness& witness, ScriptError expected)
{
    ScriptError err{SCRIPT_ERR_OK};
    BOOST_CHECK(!VerifyWitnessSpend(script_pub_key, witness, SHAREPOOL_WITNESS_FLAGS, err));
    BOOST_CHECK_EQUAL(err, expected);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(sharepool_verification_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(valid_solo_leaf_claim_verifies)
{
    const auto leaves{std::vector<SettlementLeaf>{
        MakeSoloSettlementLeaf(uint256::ONE, 101, PayoutScript(1), 50 * COIN),
    }};
    const ClaimFixture fixture{BuildClaimFixture(leaves, 0)};

    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    BOOST_CHECK(VerifyWitnessSpend(fixture.script_pub_key, fixture.witness, SHAREPOOL_WITNESS_FLAGS, err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(valid_multi_leaf_claim_with_nonzero_index_verifies)
{
    const auto leaves{std::vector<SettlementLeaf>{
        SharepoolLeaf(1, 10 * COIN),
        SharepoolLeaf(2, 15 * COIN),
        SharepoolLeaf(3, 25 * COIN),
    }};
    const ClaimFixture fixture{BuildClaimFixture(leaves, 1)};

    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    BOOST_CHECK(VerifyWitnessSpend(fixture.script_pub_key, fixture.witness, SHAREPOOL_WITNESS_FLAGS, err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(wrong_witness_stack_size_fails)
{
    ClaimFixture fixture{BuildClaimFixture({SharepoolLeaf(1, 50 * COIN)}, 0)};
    fixture.witness.stack.pop_back();

    CheckSharepoolFailure(fixture.script_pub_key, fixture.witness, SCRIPT_ERR_SHAREPOOL_WITNESS_SIZE);
}

BOOST_AUTO_TEST_CASE(wrong_descriptor_version_fails)
{
    ClaimFixture fixture{BuildClaimFixture({SharepoolLeaf(1, 50 * COIN)}, 0)};
    fixture.descriptor.version = 2;
    fixture.witness.stack[0] = SerializeToBytes(fixture.descriptor);

    CheckSharepoolFailure(fixture.script_pub_key, fixture.witness, SCRIPT_ERR_SHAREPOOL_DESCRIPTOR_VERSION);
}

BOOST_AUTO_TEST_CASE(out_of_bounds_leaf_index_fails)
{
    ClaimFixture fixture{BuildClaimFixture({SharepoolLeaf(1, 50 * COIN)}, 0)};
    fixture.witness.stack[1] = CScriptNum(1).getvch();

    CheckSharepoolFailure(fixture.script_pub_key, fixture.witness, SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED);
}

BOOST_AUTO_TEST_CASE(tampered_leaf_data_fails)
{
    ClaimFixture fixture{BuildClaimFixture({SharepoolLeaf(1, 50 * COIN)}, 0)};
    SettlementLeaf tampered{fixture.leaves.at(0)};
    ++tampered.amount_roshi;
    fixture.witness.stack[2] = SerializeToBytes(tampered);

    CheckSharepoolFailure(fixture.script_pub_key, fixture.witness, SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED);
}

BOOST_AUTO_TEST_CASE(tampered_payout_branch_fails)
{
    ClaimFixture fixture{BuildClaimFixture({SharepoolLeaf(1, 20 * COIN), SharepoolLeaf(2, 30 * COIN)}, 1)};
    BOOST_REQUIRE(!fixture.witness.stack[3].empty());
    fixture.witness.stack[3][0] ^= 0x01;

    CheckSharepoolFailure(fixture.script_pub_key, fixture.witness, SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED);
}

BOOST_AUTO_TEST_CASE(double_claim_state_hash_fails)
{
    ClaimFixture fixture{BuildClaimFixture({SharepoolLeaf(1, 20 * COIN), SharepoolLeaf(2, 30 * COIN)}, 1)};
    std::vector<bool> claimed(fixture.leaves.size(), false);
    claimed[fixture.leaf_index] = true;
    const uint256 already_claimed_root{ComputeSettlementClaimStatusRoot(fixture.leaves.size(), claimed)};
    fixture.script_pub_key = BuildSettlementScriptPubKey(ComputeSettlementStateHash(fixture.descriptor, already_claimed_root));

    CheckSharepoolFailure(fixture.script_pub_key, fixture.witness, SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED);
}

BOOST_AUTO_TEST_CASE(pre_activation_witness_v2_keeps_softfork_compatibility)
{
    CScriptWitness malformed;
    malformed.stack.push_back({0x01});
    const CScript script_pub_key{BuildSettlementScriptPubKey(uint256::ONE)};

    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    BOOST_CHECK(VerifyWitnessSpend(script_pub_key, malformed, BASE_WITNESS_FLAGS, err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(malformed_witness_v2_program_fails_after_activation)
{
    CScriptWitness malformed;
    malformed.stack.push_back({0x01});
    const CScript script_pub_key{BuildSettlementScriptPubKey(uint256::ONE)};

    CheckSharepoolFailure(script_pub_key, malformed, SCRIPT_ERR_SHAREPOOL_WITNESS_SIZE);
}

BOOST_AUTO_TEST_CASE(other_future_witness_versions_keep_softfork_compatibility)
{
    CScriptWitness malformed;
    malformed.stack.push_back({0x01});
    const CScript script_pub_key{CScript() << OP_3 << ToByteVector(uint256::ONE)};

    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    BOOST_CHECK(VerifyWitnessSpend(script_pub_key, malformed, SHAREPOOL_WITNESS_FLAGS, err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(truncated_branch_fails)
{
    ClaimFixture fixture{BuildClaimFixture({SharepoolLeaf(1, 20 * COIN), SharepoolLeaf(2, 30 * COIN)}, 1)};
    fixture.witness.stack[3].push_back(0x01);

    CheckSharepoolFailure(fixture.script_pub_key, fixture.witness, SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED);
}

BOOST_AUTO_TEST_SUITE_END()
