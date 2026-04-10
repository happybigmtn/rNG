// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/qsb_pool.h>

#include <primitives/block.h>

#include <algorithm>

#include <util/check.h>

namespace node {

QSBPoolInsertResult QSBPool::Add(const CTransactionRef& tx,
                                 QSBToyTxType type,
                                 CAmount fee,
                                 int64_t vsize,
                                 int64_t accepted_at)
{
    const Txid txid = tx->GetHash();
    LOCK(m_mutex);

    if (const auto it = m_entries.find(txid); it != m_entries.end()) {
        return {QSBPoolInsertStatus::ALREADY_PRESENT, it->second, std::nullopt};
    }

    std::vector<COutPoint> prevouts;
    prevouts.reserve(tx->vin.size());
    for (const CTxIn& txin : tx->vin) {
        if (const auto conflict = m_prevout_index.find(txin.prevout); conflict != m_prevout_index.end()) {
            return {QSBPoolInsertStatus::CONFLICTING_INPUT, std::nullopt, conflict->second};
        }
        prevouts.push_back(txin.prevout);
    }

    QSBPoolEntry entry{
        .tx = tx,
        .txid = txid,
        .fee = fee,
        .vsize = vsize,
        .accepted_at = accepted_at,
        .type = type,
        .prevouts = std::move(prevouts),
    };

    const auto [it, inserted] = m_entries.emplace(txid, entry);
    Assume(inserted);
    for (const COutPoint& prevout : it->second.prevouts) {
        m_prevout_index.emplace(prevout, txid);
    }

    return {QSBPoolInsertStatus::ADDED, it->second, std::nullopt};
}

std::optional<QSBPoolEntry> QSBPool::Get(const Txid& txid) const
{
    LOCK(m_mutex);
    if (const auto it = m_entries.find(txid); it != m_entries.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<QSBPoolEntry> QSBPool::List() const
{
    LOCK(m_mutex);
    std::vector<QSBPoolEntry> entries;
    entries.reserve(m_entries.size());
    for (const auto& [_, entry] : m_entries) {
        entries.push_back(entry);
    }
    std::sort(entries.begin(), entries.end(), [](const QSBPoolEntry& a, const QSBPoolEntry& b) {
        if (a.accepted_at != b.accepted_at) return a.accepted_at < b.accepted_at;
        return a.txid < b.txid;
    });
    return entries;
}

std::vector<QSBPoolEntry> QSBPool::GetMiningCandidates() const
{
    LOCK(m_mutex);
    std::vector<QSBPoolEntry> entries;
    entries.reserve(m_entries.size());
    for (const auto& [_, entry] : m_entries) {
        entries.push_back(entry);
    }
    std::sort(entries.begin(), entries.end(), [](const QSBPoolEntry& a, const QSBPoolEntry& b) {
        const auto lhs = static_cast<__int128>(a.fee) * b.vsize;
        const auto rhs = static_cast<__int128>(b.fee) * a.vsize;
        if (lhs != rhs) return lhs > rhs;
        if (a.accepted_at != b.accepted_at) return a.accepted_at < b.accepted_at;
        return a.txid < b.txid;
    });
    return entries;
}

bool QSBPool::Remove(const Txid& txid)
{
    LOCK(m_mutex);
    const auto it = m_entries.find(txid);
    if (it == m_entries.end()) return false;

    for (const COutPoint& prevout : it->second.prevouts) {
        m_prevout_index.erase(prevout);
    }
    m_entries.erase(it);
    return true;
}

void QSBPool::BlockConnected(ChainstateRole role, const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex)
{
    if (role == ChainstateRole::BACKGROUND) return;

    LOCK(m_mutex);
    for (size_t i = 1; i < block->vtx.size(); ++i) {
        const Txid txid = block->vtx[i]->GetHash();
        const auto it = m_entries.find(txid);
        if (it == m_entries.end()) continue;

        for (const COutPoint& prevout : it->second.prevouts) {
            m_prevout_index.erase(prevout);
        }
        m_entries.erase(it);
    }
}

} // namespace node
