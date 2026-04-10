// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/qsb_validation.h>

#include <coins.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <script/qsb.h>
#include <script/script.h>
#include <script/solver.h>

#include <algorithm>
#include <array>
#include <vector>

namespace node {
namespace {

bool CheckStandardEnvelope(const CTransaction& tx, std::string& reason)
{
    if (tx.version > TX_MAX_STANDARD_VERSION || tx.version < TX_MIN_STANDARD_VERSION) {
        reason = "version";
        return false;
    }

    if (GetTransactionWeight(tx) > MAX_STANDARD_TX_WEIGHT) {
        reason = "tx-size";
        return false;
    }

    for (const CTxIn& txin : tx.vin) {
        if (txin.scriptSig.size() > MAX_STANDARD_SCRIPTSIG_SIZE) {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptSig.IsPushOnly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
    }

    return true;
}

std::array<unsigned char, qsb::TOY_SECRET_SIZE> Sha256(const std::array<unsigned char, qsb::TOY_SECRET_SIZE>& input)
{
    std::array<unsigned char, qsb::TOY_SECRET_SIZE> digest;
    CSHA256().Write(input.data(), input.size()).Finalize(digest.data());
    return digest;
}

} // namespace

bool HasQSBToyFundingOutput(const CTransaction& tx)
{
    return std::any_of(tx.vout.begin(), tx.vout.end(), [](const CTxOut& txout) {
        return qsb::MatchToyFundingScript(txout.scriptPubKey);
    });
}

bool IsQSBToyFundingTx(const CTransaction& tx,
                       const std::optional<unsigned>& max_datacarrier_bytes,
                       bool permit_bare_multisig,
                       const CFeeRate& dust_relay_fee,
                       std::string& reason)
{
    if (!CheckStandardEnvelope(tx, reason)) return false;

    unsigned int datacarrier_bytes_left = max_datacarrier_bytes.value_or(0);
    bool found_qsb_output{false};

    TxoutType which_type;
    for (const CTxOut& txout : tx.vout) {
        if (qsb::MatchToyFundingScript(txout.scriptPubKey)) {
            if (found_qsb_output) {
                reason = "qsb-multiple-outputs";
                return false;
            }
            found_qsb_output = true;
            continue;
        }

        if (!IsStandard(txout.scriptPubKey, which_type)) {
            reason = "scriptpubkey";
            return false;
        }

        if (which_type == TxoutType::NULL_DATA) {
            const unsigned int size = txout.scriptPubKey.size();
            if (size > datacarrier_bytes_left) {
                reason = "datacarrier";
                return false;
            }
            datacarrier_bytes_left -= size;
        } else if (which_type == TxoutType::MULTISIG && !permit_bare_multisig) {
            reason = "bare-multisig";
            return false;
        }
    }

    if (!found_qsb_output) {
        reason = "qsb-missing-output";
        return false;
    }

    if (GetDust(tx, dust_relay_fee).size() > MAX_DUST_OUTPUTS_PER_TX) {
        reason = "dust";
        return false;
    }

    return true;
}

bool HasQSBToySpendInput(const CTransaction& tx, const CCoinsViewCache& coins_view)
{
    return std::any_of(tx.vin.begin(), tx.vin.end(), [&coins_view](const CTxIn& txin) {
        const Coin& coin = coins_view.AccessCoin(txin.prevout);
        return !coin.IsSpent() && qsb::MatchToyFundingScript(coin.out.scriptPubKey);
    });
}

bool LooksLikeQSBToySpendTx(const CTransaction& tx)
{
    if (tx.IsCoinBase()) return false;
    if (tx.vin.size() != 1 || tx.vout.size() != 1) return false;

    const CTxIn& txin = tx.vin.front();
    if (!txin.scriptWitness.IsNull()) return false;
    if (txin.nSequence != CTxIn::SEQUENCE_FINAL) return false;
    return qsb::MatchToySpendScriptSig(txin.scriptSig);
}

bool IsQSBToySpendTx(const CTransaction& tx, const CCoinsViewCache& coins_view, std::string& reason)
{
    if (tx.IsCoinBase()) {
        reason = "coinbase";
        return false;
    }

    if (tx.vin.size() != 1) {
        reason = "qsb-input-count";
        return false;
    }

    if (tx.vout.size() != 1) {
        reason = "qsb-output-count";
        return false;
    }

    const CTxIn& txin = tx.vin.front();
    if (!txin.scriptWitness.IsNull()) {
        reason = "qsb-witness";
        return false;
    }
    if (txin.nSequence != CTxIn::SEQUENCE_FINAL) {
        reason = "qsb-sequence";
        return false;
    }

    const Coin& prev_coin = coins_view.AccessCoin(txin.prevout);
    if (prev_coin.IsSpent()) {
        reason = "bad-txns-inputs-missingorspent";
        return false;
    }

    qsb::ToyFundingScriptInfo funding_info;
    if (!qsb::MatchToyFundingScript(prev_coin.out.scriptPubKey, &funding_info)) {
        reason = "qsb-prevout";
        return false;
    }

    qsb::ToySpendScriptSigInfo spend_info;
    if (!qsb::MatchToySpendScriptSig(txin.scriptSig, &spend_info)) {
        reason = "qsb-scriptsig";
        return false;
    }

    if (Sha256(spend_info.secret_preimage) != funding_info.secret_hash) {
        reason = "qsb-secret-hash";
        return false;
    }

    return true;
}

bool ClassifyQSBToyTransaction(const CTransaction& tx,
                               const CCoinsViewCache& coins_view,
                               const std::optional<unsigned>& max_datacarrier_bytes,
                               bool permit_bare_multisig,
                               const CFeeRate& dust_relay_fee,
                               QSBToyTxType& type,
                               std::string& reason)
{
    type = QSBToyTxType::NONE;

    if (HasQSBToyFundingOutput(tx)) {
        if (!IsQSBToyFundingTx(tx, max_datacarrier_bytes, permit_bare_multisig, dust_relay_fee, reason)) {
            return false;
        }
        type = QSBToyTxType::FUNDING;
        return true;
    }

    if (LooksLikeQSBToySpendTx(tx)) {
        if (!IsQSBToySpendTx(tx, coins_view, reason)) {
            return false;
        }
        type = QSBToyTxType::SPEND;
        return true;
    }

    reason = "qsb-template";
    return false;
}

} // namespace node
