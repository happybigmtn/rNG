// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/sharechain.h>

#include <chain.h>
#include <hash.h>
#include <pow.h>
#include <streams.h>

#include <algorithm>
#include <cassert>
#include <string>
#include <utility>

namespace node {
namespace {

using ShareDBKey = std::pair<unsigned char, uint256>;
static constexpr unsigned char DB_SHARE{'s'};

arith_uint256 GetShareTargetLimit(const arith_uint256& block_target, const Consensus::Params& consensus)
{
    const arith_uint256 pow_limit{UintToArith256(consensus.powLimit)};
    const uint64_t ratio{static_cast<uint64_t>(std::max<int64_t>(1, consensus.nPowTargetSpacing))};
    const arith_uint256 ratio_uint{ratio};

    if (block_target > pow_limit / ratio_uint) {
        return pow_limit;
    }
    arith_uint256 limit{block_target * ratio_uint};
    if (limit > pow_limit) return pow_limit;
    return limit;
}

arith_uint256 GetShareWork(uint32_t nbits)
{
    CBlockIndex index;
    index.nBits = nbits;
    return GetBlockProof(index);
}

void SetRejectReason(std::string* reject_reason, std::string reason)
{
    if (reject_reason) *reject_reason = std::move(reason);
}

} // namespace

uint256 ShareRecord::GetHash() const
{
    return (HashWriter{} << *this).GetHash();
}

arith_uint256 GetShareProof(const ShareRecord& share)
{
    return GetShareWork(share.share_nBits);
}

bool ValidateShare(const ShareRecord& share, const Consensus::Params& consensus, std::string* reject_reason)
{
    if (share.version != SHARE_RECORD_VERSION) {
        SetRejectReason(reject_reason, "bad-share-version");
        return false;
    }

    if (share.candidate_header.hashPrevBlock != share.prev_block_hash) {
        SetRejectReason(reject_reason, "share-prevblock-mismatch");
        return false;
    }

    const auto block_target{DeriveTarget(share.candidate_header.nBits, consensus.powLimit)};
    if (!block_target) {
        SetRejectReason(reject_reason, "bad-block-target");
        return false;
    }

    const auto share_target{DeriveTarget(share.share_nBits, consensus.powLimit)};
    if (!share_target) {
        SetRejectReason(reject_reason, "bad-share-target");
        return false;
    }

    if (*share_target < *block_target) {
        SetRejectReason(reject_reason, "share-target-too-hard");
        return false;
    }

    if (*share_target > GetShareTargetLimit(*block_target, consensus)) {
        SetRejectReason(reject_reason, "share-target-too-easy");
        return false;
    }

    const uint256 seed_hash{Hash(std::string(kRandomXGenesisSeedPhrase))};
    if (!CheckProofOfWork(GetBlockPoWHash(share.candidate_header, seed_hash), share.share_nBits, consensus)) {
        SetRejectReason(reject_reason, "share-pow-invalid");
        return false;
    }

    return true;
}

SharechainStore::SharechainStore() = default;

SharechainStore::SharechainStore(DBParams db_params)
    : SharechainStore{std::make_unique<CDBWrapper>(db_params)}
{
}

SharechainStore::SharechainStore(std::unique_ptr<CDBWrapper> db)
    : m_db{std::move(db)}
{
    LOCK(m_mutex);
    LoadFromDisk();
}

SharechainStore::~SharechainStore() = default;

ShareStoreResult SharechainStore::AddShare(const ShareRecord& share, const Consensus::Params& consensus)
{
    LOCK(m_mutex);
    return AddShareInternal(share, &consensus, /*write_to_disk=*/true);
}

ShareStoreResult SharechainStore::AddShareInternal(const ShareRecord& share, const Consensus::Params* consensus, bool write_to_disk)
{
    ShareStoreResult result;
    result.share_id = share.GetHash();

    if (m_shares.contains(result.share_id) || m_orphans.contains(result.share_id)) {
        result.status = ShareStoreStatus::ALREADY_PRESENT;
        return result;
    }

    if (consensus && !ValidateShare(share, *consensus, &result.reject_reason)) {
        result.status = ShareStoreStatus::INVALID;
        return result;
    }

    if (!share.parent_share.IsNull() && !m_shares.contains(share.parent_share)) {
        AddOrphan(share, result.share_id, share.parent_share);
        result.status = ShareStoreStatus::ORPHAN;
        result.missing_parent = share.parent_share;
        return result;
    }

    if (!AcceptShare(share, result.accepted_ids, write_to_disk, result.reject_reason)) {
        result.status = ShareStoreStatus::INVALID;
        return result;
    }

    result.status = ShareStoreStatus::ACCEPTED;
    return result;
}

bool SharechainStore::AcceptShare(const ShareRecord& share, std::vector<uint256>& accepted_ids, bool write_to_disk, std::string& reject_reason)
{
    const uint256 share_id{share.GetHash()};
    if (m_shares.contains(share_id)) return true;

    if (write_to_disk && !WriteShareToDisk(share)) {
        reject_reason = "share-db-write-failed";
        return false;
    }

    arith_uint256 cumulative_work{GetShareWork(share.share_nBits)};
    int height{0};
    if (!share.parent_share.IsNull()) {
        const auto parent_it{m_shares.find(share.parent_share)};
        assert(parent_it != m_shares.end());
        cumulative_work += parent_it->second.cumulative_work;
        height = parent_it->second.height + 1;
    }

    ShareEntry entry{
        .record = share,
        .share_work = GetShareWork(share.share_nBits),
        .cumulative_work = cumulative_work,
        .height = height,
    };
    const auto [it, inserted]{m_shares.emplace(share_id, std::move(entry))};
    assert(inserted);
    accepted_ids.push_back(share_id);
    UpdateBestTip(share_id, it->second);
    ResolveOrphans(share_id, accepted_ids, write_to_disk, reject_reason);
    return reject_reason.empty();
}

void SharechainStore::AddOrphan(const ShareRecord& share, const uint256& share_id, const uint256& missing_parent)
{
    const auto [it, inserted]{m_orphans.emplace(share_id, OrphanEntry{
        .record = share,
        .sequence = m_next_orphan_sequence++,
    })};
    if (!inserted) return;

    m_orphan_parent_index[missing_parent].insert(share_id);
    m_orphan_order.push_back(share_id);

    while (m_orphans.size() > MAX_SHARE_ORPHANS) {
        EvictOldestOrphan();
    }
}

void SharechainStore::EvictOldestOrphan()
{
    while (!m_orphan_order.empty()) {
        const uint256 orphan_id{m_orphan_order.front()};
        m_orphan_order.pop_front();

        const auto orphan_it{m_orphans.find(orphan_id)};
        if (orphan_it == m_orphans.end()) continue;

        const uint256 parent_id{orphan_it->second.record.parent_share};
        if (auto parent_it{m_orphan_parent_index.find(parent_id)}; parent_it != m_orphan_parent_index.end()) {
            parent_it->second.erase(orphan_id);
            if (parent_it->second.empty()) m_orphan_parent_index.erase(parent_it);
        }
        m_orphans.erase(orphan_it);
        return;
    }
}

void SharechainStore::ResolveOrphans(const uint256& parent_id, std::vector<uint256>& accepted_ids, bool write_to_disk, std::string& reject_reason)
{
    std::deque<uint256> parents;
    parents.push_back(parent_id);

    while (!parents.empty() && reject_reason.empty()) {
        const uint256 current_parent{parents.front()};
        parents.pop_front();

        auto index_it{m_orphan_parent_index.find(current_parent)};
        if (index_it == m_orphan_parent_index.end()) continue;

        const std::set<uint256> orphan_ids{std::move(index_it->second)};
        m_orphan_parent_index.erase(index_it);

        for (const uint256& orphan_id : orphan_ids) {
            auto orphan_it{m_orphans.find(orphan_id)};
            if (orphan_it == m_orphans.end()) continue;

            ShareRecord orphan{std::move(orphan_it->second.record)};
            m_orphans.erase(orphan_it);

            if (!AcceptShare(orphan, accepted_ids, write_to_disk, reject_reason)) return;
            parents.push_back(orphan_id);
        }
    }
}

void SharechainStore::UpdateBestTip(const uint256& share_id, const ShareEntry& entry)
{
    if (m_best_tip.IsNull()) {
        m_best_tip = share_id;
        return;
    }

    const auto best_it{m_shares.find(m_best_tip)};
    if (best_it == m_shares.end() ||
        entry.cumulative_work > best_it->second.cumulative_work ||
        (entry.cumulative_work == best_it->second.cumulative_work && share_id < m_best_tip)) {
        m_best_tip = share_id;
    }
}

bool SharechainStore::WriteShareToDisk(const ShareRecord& share)
{
    if (!m_db) return true;
    return m_db->Write(ShareDBKey{DB_SHARE, share.GetHash()}, share);
}

void SharechainStore::LoadFromDisk()
{
    if (!m_db) return;

    std::unique_ptr<CDBIterator> it{m_db->NewIterator()};
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        ShareDBKey key;
        if (!it->GetKey(key) || key.first != DB_SHARE) continue;

        ShareRecord share;
        if (!it->GetValue(share)) continue;

        auto result{AddShareInternal(share, /*consensus=*/nullptr, /*write_to_disk=*/false)};
        (void)result;
    }
}

std::optional<ShareRecord> SharechainStore::GetShare(const uint256& share_id) const
{
    LOCK(m_mutex);
    const auto it{m_shares.find(share_id)};
    if (it == m_shares.end()) return std::nullopt;
    return it->second.record;
}

bool SharechainStore::Contains(const uint256& share_id) const
{
    LOCK(m_mutex);
    return m_shares.contains(share_id);
}

uint256 SharechainStore::BestTip() const
{
    LOCK(m_mutex);
    return m_best_tip;
}

std::optional<int> SharechainStore::Height(const uint256& share_id) const
{
    LOCK(m_mutex);
    const auto it{m_shares.find(share_id)};
    if (it == m_shares.end()) return std::nullopt;
    return it->second.height;
}

size_t SharechainStore::ShareCount() const
{
    LOCK(m_mutex);
    return m_shares.size();
}

size_t SharechainStore::OrphanCount() const
{
    LOCK(m_mutex);
    return m_orphans.size();
}

} // namespace node
