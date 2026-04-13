// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_SHAREPOOL_H
#define BITCOIN_CONSENSUS_SHAREPOOL_H

#include <script/script.h>
#include <serialize.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <vector>

class HashWriter;

namespace consensus::sharepool {

inline constexpr uint64_t SETTLEMENT_DESCRIPTOR_VERSION{1};

struct SettlementLeaf {
    CScript payout_script;
    int64_t amount_roshi{0};
    uint256 first_share_id;
    uint256 last_share_id;

    SERIALIZE_METHODS(SettlementLeaf, obj)
    {
        READWRITE(obj.payout_script, obj.amount_roshi, obj.first_share_id, obj.last_share_id);
    }
};

struct SettlementDescriptor {
    uint64_t version{SETTLEMENT_DESCRIPTOR_VERSION};
    uint256 payout_root;
    uint32_t leaf_count{0};

    SERIALIZE_METHODS(SettlementDescriptor, obj)
    {
        READWRITE(COMPACTSIZE(obj.version), obj.payout_root, COMPACTSIZE(obj.leaf_count));
    }
};

uint256 HashSettlementLeaf(const SettlementLeaf& leaf);
uint256 HashSettlementDescriptor(const SettlementDescriptor& descriptor);
uint256 HashSettlementClaimFlag(size_t index, bool claimed);
uint256 ComputeSettlementStateHash(const SettlementDescriptor& descriptor, const uint256& claim_status_root);

bool SettlementLeafLess(const SettlementLeaf& left, const SettlementLeaf& right);
SettlementLeaf MakeSoloSettlementLeaf(const uint256& prev_block_hash, int32_t height, const CScript& payout_script, int64_t amount_roshi);
std::vector<SettlementLeaf> SortSettlementLeaves(std::vector<SettlementLeaf> leaves);

SettlementDescriptor BuildSettlementDescriptor(const std::vector<SettlementLeaf>& ordered_leaves);
uint256 ComputeSettlementPayoutRoot(const std::vector<SettlementLeaf>& ordered_leaves);
std::vector<uint256> ComputeSettlementPayoutBranch(const std::vector<SettlementLeaf>& ordered_leaves, size_t index);

size_t SettlementStatusTreeSize(size_t leaf_count);
std::vector<bool> InitialSettlementClaimedFlags(size_t leaf_count);
uint256 ComputeSettlementClaimStatusRoot(size_t leaf_count, const std::vector<bool>& claimed_flags);
uint256 ComputeInitialSettlementClaimStatusRoot(size_t leaf_count);
uint256 ComputeInitialSettlementStateHash(const std::vector<SettlementLeaf>& ordered_leaves);
std::vector<uint256> ComputeSettlementClaimStatusBranch(size_t leaf_count, const std::vector<bool>& claimed_flags, size_t index);

CScript BuildSettlementScriptPubKey(const uint256& state_hash);
uint256 ComputeSettlementMerkleRoot(std::vector<uint256> leaves);
std::vector<uint256> ComputeSettlementMerkleBranch(std::vector<uint256> leaves, size_t index);
uint256 ComputeSettlementMerkleRootFromBranch(const uint256& leaf_hash, size_t index, const std::vector<uint256>& branch);

int64_t ComputeRemainingSettlementValue(const std::vector<SettlementLeaf>& ordered_leaves, const std::vector<bool>& claimed_flags);

} // namespace consensus::sharepool

#endif // BITCOIN_CONSENSUS_SHAREPOOL_H
