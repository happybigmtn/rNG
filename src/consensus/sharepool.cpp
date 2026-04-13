// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/sharepool.h>

#include <hash.h>
#include <script/script_error.h>
#include <serialize.h>
#include <streams.h>
#include <util/check.h>

#include <algorithm>
#include <array>
#include <ios>
#include <numeric>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace consensus::sharepool {
namespace {

constexpr std::string_view LEAF_TAG{"RNGSharepoolLeaf"};
constexpr std::string_view DESCRIPTOR_TAG{"RNGSharepoolDescriptor"};
constexpr std::string_view CLAIM_FLAG_TAG{"RNGSharepoolClaimFlag"};
constexpr std::string_view STATE_TAG{"RNGSharepoolState"};
constexpr std::string_view SOLO_LEAF_TAG{"RNGSharepoolSoloLeaf"};

template <typename Serializable>
uint256 TaggedDoubleSHA256(std::string_view tag, const Serializable& value)
{
    HashWriter writer;
    writer.write(MakeByteSpan(tag));
    writer << value;
    return writer.GetHash();
}

uint256 TaggedDoubleSHA256(std::string_view tag, const SettlementDescriptor& descriptor, const uint256& claim_status_root)
{
    HashWriter writer;
    writer.write(MakeByteSpan(tag));
    writer << descriptor;
    writer << claim_status_root;
    return writer.GetHash();
}

size_t NextPowerOfTwo(size_t value)
{
    if (value <= 1) return 1;
    size_t power{1};
    while (power < value) {
        power <<= 1;
    }
    return power;
}

std::vector<uint256> SettlementStatusLeafHashes(size_t leaf_count, const std::vector<bool>& claimed_flags)
{
    Assert(leaf_count > 0);
    Assert(claimed_flags.size() == leaf_count);

    std::vector<uint256> hashes;
    hashes.reserve(SettlementStatusTreeSize(leaf_count));
    for (size_t index = 0; index < leaf_count; ++index) {
        hashes.push_back(HashSettlementClaimFlag(index, claimed_flags[index]));
    }
    while (hashes.size() < SettlementStatusTreeSize(leaf_count)) {
        hashes.push_back(HashSettlementClaimFlag(hashes.size(), /*claimed=*/true));
    }
    return hashes;
}

std::vector<uint256> SettlementPayoutLeafHashes(const std::vector<SettlementLeaf>& ordered_leaves)
{
    std::vector<uint256> hashes;
    hashes.reserve(ordered_leaves.size());
    for (const SettlementLeaf& leaf : ordered_leaves) {
        hashes.push_back(HashSettlementLeaf(leaf));
    }
    return hashes;
}

bool SetSharepoolError(ScriptError* serror, ScriptError error)
{
    if (serror) *serror = error;
    return false;
}

template <typename T>
bool DeserializeSettlementElement(const std::vector<unsigned char>& element, T& value)
{
    try {
        DataStream stream{element};
        stream >> value;
        return stream.empty();
    } catch (const std::ios_base::failure&) {
        return false;
    }
}

bool DecodeSettlementLeafIndex(const std::vector<unsigned char>& element, uint32_t leaf_count, size_t& leaf_index)
{
    if (leaf_count == 0) return false;
    try {
        const CScriptNum index_num{element, /*fRequireMinimal=*/false, /*nMaxNumSize=*/5};
        const int64_t index_value{index_num.GetInt64()};
        if (index_value < 0) return false;
        if (static_cast<uint64_t>(index_value) >= leaf_count) return false;
        leaf_index = static_cast<size_t>(index_value);
        return true;
    } catch (const scriptnum_error&) {
        return false;
    }
}

bool DecodeSettlementHashBranch(const std::vector<unsigned char>& element, std::vector<uint256>& branch)
{
    if (element.size() % uint256::size() != 0) return false;
    branch.clear();
    branch.reserve(element.size() / uint256::size());
    for (size_t offset{0}; offset < element.size(); offset += uint256::size()) {
        branch.emplace_back(std::span<const unsigned char>{element.data() + offset, uint256::size()});
    }
    return true;
}

} // namespace

uint256 HashSettlementLeaf(const SettlementLeaf& leaf)
{
    return TaggedDoubleSHA256(LEAF_TAG, leaf);
}

uint256 HashSettlementDescriptor(const SettlementDescriptor& descriptor)
{
    return TaggedDoubleSHA256(DESCRIPTOR_TAG, descriptor);
}

uint256 HashSettlementClaimFlag(size_t index, bool claimed)
{
    HashWriter writer;
    writer.write(MakeByteSpan(CLAIM_FLAG_TAG));
    WriteCompactSize(writer, index);
    const uint8_t flag{claimed ? uint8_t{1} : uint8_t{0}};
    writer.write(std::as_bytes(std::span{&flag, size_t{1}}));
    return writer.GetHash();
}

uint256 ComputeSettlementStateHash(const SettlementDescriptor& descriptor, const uint256& claim_status_root)
{
    return TaggedDoubleSHA256(STATE_TAG, descriptor, claim_status_root);
}

bool SettlementLeafLess(const SettlementLeaf& left, const SettlementLeaf& right)
{
    const uint256 left_hash{Hash(left.payout_script)};
    const uint256 right_hash{Hash(right.payout_script)};
    if (left_hash != right_hash) return left_hash < right_hash;
    return left.payout_script < right.payout_script;
}

SettlementLeaf MakeSoloSettlementLeaf(const uint256& prev_block_hash, int32_t height, const CScript& payout_script, int64_t amount_roshi)
{
    HashWriter writer;
    writer.write(MakeByteSpan(SOLO_LEAF_TAG));
    writer << prev_block_hash;
    writer << height;
    writer << payout_script;
    writer << amount_roshi;
    const uint256 synthetic_share_id{writer.GetHash()};

    return SettlementLeaf{
        .payout_script = payout_script,
        .amount_roshi = amount_roshi,
        .first_share_id = synthetic_share_id,
        .last_share_id = synthetic_share_id,
    };
}

std::vector<SettlementLeaf> SortSettlementLeaves(std::vector<SettlementLeaf> leaves)
{
    std::sort(leaves.begin(), leaves.end(), SettlementLeafLess);
    return leaves;
}

SettlementDescriptor BuildSettlementDescriptor(const std::vector<SettlementLeaf>& ordered_leaves)
{
    Assert(!ordered_leaves.empty());
    return SettlementDescriptor{
        .version = SETTLEMENT_DESCRIPTOR_VERSION,
        .payout_root = ComputeSettlementPayoutRoot(ordered_leaves),
        .leaf_count = static_cast<uint32_t>(ordered_leaves.size()),
    };
}

uint256 ComputeSettlementPayoutRoot(const std::vector<SettlementLeaf>& ordered_leaves)
{
    Assert(!ordered_leaves.empty());
    return ComputeSettlementMerkleRoot(SettlementPayoutLeafHashes(ordered_leaves));
}

std::vector<uint256> ComputeSettlementPayoutBranch(const std::vector<SettlementLeaf>& ordered_leaves, size_t index)
{
    Assert(!ordered_leaves.empty());
    return ComputeSettlementMerkleBranch(SettlementPayoutLeafHashes(ordered_leaves), index);
}

size_t SettlementStatusTreeSize(size_t leaf_count)
{
    Assert(leaf_count > 0);
    return NextPowerOfTwo(leaf_count);
}

std::vector<bool> InitialSettlementClaimedFlags(size_t leaf_count)
{
    Assert(leaf_count > 0);
    return std::vector<bool>(leaf_count, false);
}

uint256 ComputeSettlementClaimStatusRoot(size_t leaf_count, const std::vector<bool>& claimed_flags)
{
    return ComputeSettlementMerkleRoot(SettlementStatusLeafHashes(leaf_count, claimed_flags));
}

uint256 ComputeInitialSettlementClaimStatusRoot(size_t leaf_count)
{
    return ComputeSettlementClaimStatusRoot(leaf_count, InitialSettlementClaimedFlags(leaf_count));
}

uint256 ComputeInitialSettlementStateHash(const std::vector<SettlementLeaf>& ordered_leaves)
{
    const SettlementDescriptor descriptor{BuildSettlementDescriptor(ordered_leaves)};
    return ComputeSettlementStateHash(descriptor, ComputeInitialSettlementClaimStatusRoot(descriptor.leaf_count));
}

std::vector<uint256> ComputeSettlementClaimStatusBranch(size_t leaf_count, const std::vector<bool>& claimed_flags, size_t index)
{
    return ComputeSettlementMerkleBranch(SettlementStatusLeafHashes(leaf_count, claimed_flags), index);
}

CScript BuildSettlementScriptPubKey(const uint256& state_hash)
{
    return CScript() << OP_2 << ToByteVector(state_hash);
}

uint256 ComputeSettlementMerkleRoot(std::vector<uint256> leaves)
{
    Assert(!leaves.empty());
    while (leaves.size() > 1) {
        if (leaves.size() & 1U) {
            leaves.push_back(leaves.back());
        }
        std::vector<uint256> parents;
        parents.reserve(leaves.size() / 2);
        for (size_t pos{0}; pos < leaves.size(); pos += 2) {
            parents.push_back(Hash(leaves[pos], leaves[pos + 1]));
        }
        leaves = std::move(parents);
    }
    return leaves.front();
}

std::vector<uint256> ComputeSettlementMerkleBranch(std::vector<uint256> leaves, size_t index)
{
    Assert(!leaves.empty());
    Assert(index < leaves.size());

    std::vector<uint256> branch;
    while (leaves.size() > 1) {
        size_t sibling_index{index ^ size_t{1}};
        if (sibling_index >= leaves.size()) sibling_index = leaves.size() - 1;
        branch.push_back(leaves[sibling_index]);

        if (leaves.size() & 1U) {
            leaves.push_back(leaves.back());
        }
        std::vector<uint256> parents;
        parents.reserve(leaves.size() / 2);
        for (size_t pos{0}; pos < leaves.size(); pos += 2) {
            parents.push_back(Hash(leaves[pos], leaves[pos + 1]));
        }
        leaves = std::move(parents);
        index /= 2;
    }
    return branch;
}

uint256 ComputeSettlementMerkleRootFromBranch(const uint256& leaf_hash, size_t index, const std::vector<uint256>& branch)
{
    uint256 current{leaf_hash};
    for (const uint256& sibling : branch) {
        current = (index & size_t{1}) == 0 ? Hash(current, sibling) : Hash(sibling, current);
        index /= 2;
    }
    return current;
}

int64_t ComputeRemainingSettlementValue(const std::vector<SettlementLeaf>& ordered_leaves, const std::vector<bool>& claimed_flags)
{
    Assert(ordered_leaves.size() == claimed_flags.size());

    int64_t remaining{0};
    for (size_t index{0}; index < ordered_leaves.size(); ++index) {
        if (!claimed_flags[index]) {
            remaining += ordered_leaves[index].amount_roshi;
        }
    }
    return remaining;
}

bool VerifySharepoolSettlement(const std::vector<unsigned char>& program, const std::vector<std::vector<unsigned char>>& witness_stack, ScriptError* serror)
{
    if (witness_stack.size() != 5) {
        return SetSharepoolError(serror, SCRIPT_ERR_SHAREPOOL_WITNESS_SIZE);
    }
    if (program.size() != uint256::size()) {
        return SetSharepoolError(serror, SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED);
    }

    SettlementDescriptor descriptor;
    if (!DeserializeSettlementElement(witness_stack[0], descriptor)) {
        return SetSharepoolError(serror, SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED);
    }
    if (descriptor.version != SETTLEMENT_DESCRIPTOR_VERSION) {
        return SetSharepoolError(serror, SCRIPT_ERR_SHAREPOOL_DESCRIPTOR_VERSION);
    }

    size_t leaf_index{0};
    if (!DecodeSettlementLeafIndex(witness_stack[1], descriptor.leaf_count, leaf_index)) {
        return SetSharepoolError(serror, SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED);
    }

    SettlementLeaf leaf;
    if (!DeserializeSettlementElement(witness_stack[2], leaf)) {
        return SetSharepoolError(serror, SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED);
    }

    std::vector<uint256> payout_branch;
    std::vector<uint256> status_branch;
    if (!DecodeSettlementHashBranch(witness_stack[3], payout_branch) ||
        !DecodeSettlementHashBranch(witness_stack[4], status_branch)) {
        return SetSharepoolError(serror, SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED);
    }

    const uint256 payout_root{ComputeSettlementMerkleRootFromBranch(HashSettlementLeaf(leaf), leaf_index, payout_branch)};
    if (payout_root != descriptor.payout_root) {
        return SetSharepoolError(serror, SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED);
    }

    const uint256 old_status_root{
        ComputeSettlementMerkleRootFromBranch(HashSettlementClaimFlag(leaf_index, /*claimed=*/false), leaf_index, status_branch)};
    const uint256 expected_state_hash{ComputeSettlementStateHash(descriptor, old_status_root)};
    const uint256 program_state_hash{std::span<const unsigned char>{program.data(), program.size()}};
    if (expected_state_hash != program_state_hash) {
        return SetSharepoolError(serror, SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED);
    }

    if (serror) *serror = SCRIPT_ERR_OK;
    return true;
}

} // namespace consensus::sharepool
