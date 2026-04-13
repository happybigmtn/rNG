// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_SHARECHAIN_H
#define BITCOIN_NODE_SHARECHAIN_H

#include <arith_uint256.h>
#include <consensus/params.h>
#include <dbwrapper.h>
#include <primitives/block.h>
#include <script/script.h>
#include <serialize.h>
#include <sync.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

class CDBWrapper;

namespace node {

static constexpr uint32_t SHARE_RECORD_VERSION{1};
static constexpr size_t MAX_SHARE_ORPHANS{64};
static constexpr size_t MAX_SHARE_INV_SZ{1000};
static constexpr size_t MAX_SHARE_BATCH_SZ{16};

struct ShareRecord {
    uint32_t version{SHARE_RECORD_VERSION};
    uint256 parent_share;
    uint256 prev_block_hash;
    CBlockHeader candidate_header;
    uint32_t share_nBits{0};
    CScript payout_script;

    SERIALIZE_METHODS(ShareRecord, obj)
    {
        READWRITE(obj.version,
                  obj.parent_share,
                  obj.prev_block_hash,
                  obj.candidate_header,
                  obj.share_nBits,
                  obj.payout_script);
    }

    uint256 GetHash() const;
};

bool ValidateShare(const ShareRecord& share, const Consensus::Params& consensus, std::string* reject_reason = nullptr);
arith_uint256 GetShareProof(const ShareRecord& share);

enum class ShareStoreStatus {
    ACCEPTED,
    ALREADY_PRESENT,
    ORPHAN,
    INVALID,
};

struct ShareStoreResult {
    ShareStoreStatus status{ShareStoreStatus::INVALID};
    uint256 share_id;
    std::vector<uint256> accepted_ids;
    std::optional<uint256> missing_parent;
    std::string reject_reason;
};

class SharechainStore
{
public:
    SharechainStore();
    explicit SharechainStore(DBParams db_params);
    explicit SharechainStore(std::unique_ptr<CDBWrapper> db);
    ~SharechainStore();

    SharechainStore(const SharechainStore&) = delete;
    SharechainStore& operator=(const SharechainStore&) = delete;

    ShareStoreResult AddShare(const ShareRecord& share, const Consensus::Params& consensus) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    std::optional<ShareRecord> GetShare(const uint256& share_id) const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    bool Contains(const uint256& share_id) const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    uint256 BestTip() const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    std::optional<int> Height(const uint256& share_id) const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    size_t ShareCount() const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    size_t OrphanCount() const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

private:
    struct ShareEntry {
        ShareRecord record;
        arith_uint256 share_work;
        arith_uint256 cumulative_work;
        int height{0};
    };

    struct OrphanEntry {
        ShareRecord record;
        uint64_t sequence{0};
    };

    ShareStoreResult AddShareInternal(const ShareRecord& share, const Consensus::Params* consensus, bool write_to_disk)
        EXCLUSIVE_LOCKS_REQUIRED(m_mutex);
    bool AcceptShare(const ShareRecord& share, std::vector<uint256>& accepted_ids, bool write_to_disk, std::string& reject_reason)
        EXCLUSIVE_LOCKS_REQUIRED(m_mutex);
    void AddOrphan(const ShareRecord& share, const uint256& share_id, const uint256& missing_parent)
        EXCLUSIVE_LOCKS_REQUIRED(m_mutex);
    void EvictOldestOrphan() EXCLUSIVE_LOCKS_REQUIRED(m_mutex);
    void ResolveOrphans(const uint256& parent_id, std::vector<uint256>& accepted_ids, bool write_to_disk, std::string& reject_reason)
        EXCLUSIVE_LOCKS_REQUIRED(m_mutex);
    void UpdateBestTip(const uint256& share_id, const ShareEntry& entry) EXCLUSIVE_LOCKS_REQUIRED(m_mutex);
    bool WriteShareToDisk(const ShareRecord& share) EXCLUSIVE_LOCKS_REQUIRED(m_mutex);
    void LoadFromDisk() EXCLUSIVE_LOCKS_REQUIRED(m_mutex);

    mutable Mutex m_mutex;
    std::unique_ptr<CDBWrapper> m_db GUARDED_BY(m_mutex);
    std::map<uint256, ShareEntry> m_shares GUARDED_BY(m_mutex);
    std::map<uint256, OrphanEntry> m_orphans GUARDED_BY(m_mutex);
    std::map<uint256, std::set<uint256>> m_orphan_parent_index GUARDED_BY(m_mutex);
    std::deque<uint256> m_orphan_order GUARDED_BY(m_mutex);
    uint64_t m_next_orphan_sequence GUARDED_BY(m_mutex){0};
    uint256 m_best_tip GUARDED_BY(m_mutex);
};

} // namespace node

#endif // BITCOIN_NODE_SHARECHAIN_H
