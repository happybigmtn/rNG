// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_QSB_POOL_H
#define BITCOIN_NODE_QSB_POOL_H

#include <consensus/amount.h>
#include <node/qsb_validation.h>
#include <primitives/transaction.h>
#include <sync.h>
#include <validationinterface.h>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace node {

struct QSBPoolEntry {
    CTransactionRef tx;
    Txid txid;
    CAmount fee{0};
    int64_t vsize{0};
    int64_t accepted_at{0};
    QSBToyTxType type{QSBToyTxType::NONE};
    std::vector<COutPoint> prevouts;
};

enum class QSBPoolInsertStatus {
    ADDED,
    ALREADY_PRESENT,
    CONFLICTING_INPUT,
};

struct QSBPoolInsertResult {
    QSBPoolInsertStatus status;
    std::optional<QSBPoolEntry> entry;
    std::optional<Txid> conflicting_txid;
};

class QSBPool final
    : public CValidationInterface
{
public:
    QSBPool() = default;
    ~QSBPool() = default;
    QSBPool(const QSBPool&) = delete;
    QSBPool& operator=(const QSBPool&) = delete;

    QSBPoolInsertResult Add(const CTransactionRef& tx,
                            QSBToyTxType type,
                            CAmount fee,
                            int64_t vsize,
                            int64_t accepted_at) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    std::optional<QSBPoolEntry> Get(const Txid& txid) const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    std::vector<QSBPoolEntry> List() const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    std::vector<QSBPoolEntry> GetMiningCandidates() const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    bool Remove(const Txid& txid) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

private:
    void BlockConnected(ChainstateRole role, const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex) override EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    mutable Mutex m_mutex;
    std::map<Txid, QSBPoolEntry> m_entries GUARDED_BY(m_mutex);
    std::map<COutPoint, Txid> m_prevout_index GUARDED_BY(m_mutex);
};

} // namespace node

#endif // BITCOIN_NODE_QSB_POOL_H
