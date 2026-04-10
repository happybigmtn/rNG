// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_QSB_VALIDATION_H
#define BITCOIN_NODE_QSB_VALIDATION_H

#include <optional>
#include <string>

class CCoinsViewCache;
class CFeeRate;
class CTransaction;

namespace node {

enum class QSBToyTxType {
    NONE,
    FUNDING,
    SPEND,
};

bool HasQSBToyFundingOutput(const CTransaction& tx);
bool IsQSBToyFundingTx(const CTransaction& tx,
                       const std::optional<unsigned>& max_datacarrier_bytes,
                       bool permit_bare_multisig,
                       const CFeeRate& dust_relay_fee,
                       std::string& reason);

bool HasQSBToySpendInput(const CTransaction& tx, const CCoinsViewCache& coins_view);
bool IsQSBToySpendTx(const CTransaction& tx, const CCoinsViewCache& coins_view, std::string& reason);
bool ClassifyQSBToyTransaction(const CTransaction& tx,
                               const CCoinsViewCache& coins_view,
                               const std::optional<unsigned>& max_datacarrier_bytes,
                               bool permit_bare_multisig,
                               const CFeeRate& dust_relay_fee,
                               QSBToyTxType& type,
                               std::string& reason);

} // namespace node

#endif // BITCOIN_NODE_QSB_VALIDATION_H
