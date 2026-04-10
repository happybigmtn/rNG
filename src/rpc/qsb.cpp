// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <core_io.h>
#include <node/context.h>
#include <node/qsb_pool.h>
#include <node/qsb_validation.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <rpc/util.h>
#include <txmempool.h>
#include <util/check.h>
#include <util/time.h>
#include <validation.h>

#include <univalue.h>

namespace {

std::string QSBTxTypeString(node::QSBToyTxType type)
{
    switch (type) {
    case node::QSBToyTxType::FUNDING:
        return "funding";
    case node::QSBToyTxType::SPEND:
        return "spend";
    case node::QSBToyTxType::NONE:
        break;
    }
    return "unknown";
}

std::string QSBFailurePhase(const TxValidationState& state)
{
    switch (state.GetResult()) {
    case TxValidationResult::TX_MISSING_INPUTS:
    case TxValidationResult::TX_PREMATURE_SPEND:
        return "input-availability";
    case TxValidationResult::TX_CONSENSUS:
    case TxValidationResult::TX_WITNESS_MUTATED:
    case TxValidationResult::TX_WITNESS_STRIPPED:
        return "consensus-validation";
    default:
        return "policy-validation";
    }
}

std::string QSBClassificationPhase(const std::string& reason)
{
    if (reason == "bad-txns-inputs-missingorspent" || reason == "non-final" || reason == "non-BIP68-final") {
        return "input-availability";
    }
    return "template-matching";
}

UniValue QSBEntryToUniValue(const node::QSBPoolEntry& entry)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("txid", entry.txid.GetHex());
    obj.pushKV("type", QSBTxTypeString(entry.type));
    obj.pushKV("fee_sat", entry.fee);
    obj.pushKV("vsize", entry.vsize);
    obj.pushKV("accepted_at", entry.accepted_at);
    obj.pushKV("hex", EncodeHexTx(*entry.tx));

    UniValue prevouts(UniValue::VARR);
    for (const COutPoint& prevout : entry.prevouts) {
        UniValue prevout_obj(UniValue::VOBJ);
        prevout_obj.pushKV("txid", prevout.hash.GetHex());
        prevout_obj.pushKV("vout", prevout.n);
        prevouts.push_back(std::move(prevout_obj));
    }
    obj.pushKV("prevouts", std::move(prevouts));
    return obj;
}

node::QSBPool& EnsureQSBPool(node::NodeContext& node)
{
    return *CHECK_NONFATAL(node.qsb_pool);
}

static RPCHelpMan submitqsbtransaction()
{
    return RPCHelpMan{
        "submitqsbtransaction",
        "Validate and queue a supported QSB candidate locally without submitting it to the public mempool.\n",
        {
            {"hexstring", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The raw transaction hex."},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::BOOL, "accepted", "Whether the candidate is present in the local QSB queue."},
                {RPCResult::Type::BOOL, "already_queued", "Whether the transaction was already queued."},
                {RPCResult::Type::STR_HEX, "txid", "The queued transaction id."},
                {RPCResult::Type::STR, "type", "The supported QSB template family."},
                {RPCResult::Type::NUM, "fee_sat", "The base fee in satoshis."},
                {RPCResult::Type::NUM, "vsize", "The transaction virtual size used for validation."},
                {RPCResult::Type::NUM_TIME, "accepted_at", "The unix timestamp when the queue accepted the candidate."},
            }
        },
        RPCExamples{
            HelpExampleCli("submitqsbtransaction", "\"rawtxhex\"")
            + HelpExampleRpc("submitqsbtransaction", "\"rawtxhex\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    node::NodeContext& node = EnsureAnyNodeContext(request.context);
    node::QSBPool& qsb_pool = EnsureQSBPool(node);

    CMutableTransaction mtx;
    if (!DecodeHexTx(mtx, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed. Make sure the tx has at least one input.");
    }
    const CTransactionRef tx = MakeTransactionRef(std::move(mtx));

    if (const auto existing = qsb_pool.Get(tx->GetHash())) {
        UniValue result(UniValue::VOBJ);
        result.pushKV("accepted", true);
        result.pushKV("already_queued", true);
        result.pushKV("txid", existing->txid.GetHex());
        result.pushKV("type", QSBTxTypeString(existing->type));
        result.pushKV("fee_sat", existing->fee);
        result.pushKV("vsize", existing->vsize);
        result.pushKV("accepted_at", existing->accepted_at);
        return result;
    }

    node::QSBToyTxType qsb_type{node::QSBToyTxType::NONE};
    std::optional<MempoolAcceptResult> validation_result;
    {
        LOCK(cs_main);
        ChainstateManager& chainman = *CHECK_NONFATAL(node.chainman);
        CTxMemPool& mempool = *CHECK_NONFATAL(node.mempool);
        std::string reason;
        if (!node::ClassifyQSBToyTransaction(*tx,
                                             chainman.ActiveChainstate().CoinsTip(),
                                             mempool.m_opts.max_datacarrier_bytes,
                                             mempool.m_opts.permit_bare_multisig,
                                             mempool.m_opts.dust_relay_feerate,
                                             qsb_type,
                                             reason)) {
            throw JSONRPCError(RPC_VERIFY_ERROR, QSBClassificationPhase(reason) + ": " + reason);
        }
        validation_result.emplace(chainman.ProcessTransaction(tx, /*test_accept=*/true, /*allow_qsb_toy=*/true));
    }

    if (validation_result->m_result_type != MempoolAcceptResult::ResultType::VALID) {
        if (validation_result->m_result_type == MempoolAcceptResult::ResultType::MEMPOOL_ENTRY) {
            throw JSONRPCError(RPC_VERIFY_ERROR, "policy-validation: txn-already-in-mempool");
        }
        if (validation_result->m_result_type == MempoolAcceptResult::ResultType::DIFFERENT_WITNESS) {
            throw JSONRPCError(RPC_VERIFY_ERROR, "policy-validation: txn-same-nonwitness-data-in-mempool");
        }
        throw JSONRPCError(RPC_VERIFY_ERROR, QSBFailurePhase(validation_result->m_state) + ": " + validation_result->m_state.ToString());
    }

    const int64_t accepted_at = GetTime();
    const auto insert_result = qsb_pool.Add(tx,
                                            qsb_type,
                                            *CHECK_NONFATAL(validation_result->m_base_fees),
                                            *CHECK_NONFATAL(validation_result->m_vsize),
                                            accepted_at);
    if (insert_result.status == node::QSBPoolInsertStatus::CONFLICTING_INPUT) {
        throw JSONRPCError(RPC_VERIFY_ERROR,
                           "input-conflict: queued transaction " + insert_result.conflicting_txid->GetHex() + " already spends one of the requested prevouts");
    }

    const node::QSBPoolEntry& entry = *CHECK_NONFATAL(insert_result.entry);
    UniValue result(UniValue::VOBJ);
    result.pushKV("accepted", true);
    result.pushKV("already_queued", insert_result.status == node::QSBPoolInsertStatus::ALREADY_PRESENT);
    result.pushKV("txid", entry.txid.GetHex());
    result.pushKV("type", QSBTxTypeString(entry.type));
    result.pushKV("fee_sat", entry.fee);
    result.pushKV("vsize", entry.vsize);
    result.pushKV("accepted_at", entry.accepted_at);
    return result;
},
    };
}

static RPCHelpMan listqsbtransactions()
{
    return RPCHelpMan{
        "listqsbtransactions",
        "List the locally queued QSB candidates.\n",
        {},
        RPCResult{
            RPCResult::Type::ARR, "", "",
            {
                {RPCResult::Type::OBJ, "", "",
                {
                    {RPCResult::Type::STR_HEX, "txid", "The queued transaction id."},
                    {RPCResult::Type::STR, "type", "The supported QSB template family."},
                    {RPCResult::Type::NUM, "fee_sat", "The base fee in satoshis."},
                    {RPCResult::Type::NUM, "vsize", "The transaction virtual size used for validation."},
                    {RPCResult::Type::NUM_TIME, "accepted_at", "The unix timestamp when the queue accepted the candidate."},
                    {RPCResult::Type::STR_HEX, "hex", "The raw transaction hex."},
                    {RPCResult::Type::ARR, "prevouts", "The prevouts consumed by the transaction.",
                    {
                        {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::STR_HEX, "txid", "The previous transaction id."},
                            {RPCResult::Type::NUM, "vout", "The output index."},
                        }},
                    }},
                }},
            }
        },
        RPCExamples{
            HelpExampleCli("listqsbtransactions", "")
            + HelpExampleRpc("listqsbtransactions", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    node::NodeContext& node = EnsureAnyNodeContext(request.context);
    UniValue result(UniValue::VARR);
    for (const node::QSBPoolEntry& entry : EnsureQSBPool(node).List()) {
        result.push_back(QSBEntryToUniValue(entry));
    }
    return result;
},
    };
}

static RPCHelpMan removeqsbtransaction()
{
    return RPCHelpMan{
        "removeqsbtransaction",
        "Remove a locally queued QSB candidate by txid.\n",
        {
            {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The queued transaction id."},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR_HEX, "txid", "The requested transaction id."},
                {RPCResult::Type::BOOL, "removed", "Whether a queued candidate was removed."},
            }
        },
        RPCExamples{
            HelpExampleCli("removeqsbtransaction", "\"txid\"")
            + HelpExampleRpc("removeqsbtransaction", "\"txid\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    node::NodeContext& node = EnsureAnyNodeContext(request.context);
    const auto txid = Txid::FromUint256(ParseHashV(request.params[0], "txid"));
    const bool removed = EnsureQSBPool(node).Remove(txid);

    UniValue result(UniValue::VOBJ);
    result.pushKV("txid", txid.GetHex());
    result.pushKV("removed", removed);
    return result;
},
    };
}

} // namespace

void RegisterQSBRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"qsb", &submitqsbtransaction},
        {"qsb", &listqsbtransactions},
        {"qsb", &removeqsbtransaction},
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
