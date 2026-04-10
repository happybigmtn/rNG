// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coins.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <script/qsb.h>
#include <script/script.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <test/util/setup_common.h>
#include <util/rbf.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <numeric>
#include <span>
#include <vector>

namespace {

std::array<unsigned char, qsb::TOY_SECRET_SIZE> MakeSecret(uint8_t first_byte)
{
    std::array<unsigned char, qsb::TOY_SECRET_SIZE> secret{};
    for (size_t i = 0; i < secret.size(); ++i) {
        secret[i] = first_byte + i;
    }
    return secret;
}

std::array<unsigned char, qsb::TOY_SECRET_SIZE> Sha256(std::span<const unsigned char> data)
{
    std::array<unsigned char, qsb::TOY_SECRET_SIZE> digest{};
    CSHA256().Write(data.data(), data.size()).Finalize(digest.data());
    return digest;
}

CScript BuildToySpendScriptSig(const std::array<unsigned char, qsb::TOY_SECRET_SIZE>& secret_preimage)
{
    CScript script_sig;
    script_sig << secret_preimage;
    return script_sig;
}

CScript BuildToyFundingScript(const std::array<unsigned char, qsb::TOY_SECRET_SIZE>& secret_preimage,
                              size_t payload_bytes = 780,
                              size_t chunk_size = 260)
{
    const auto secret_hash = Sha256(secret_preimage);
    std::vector<unsigned char> magic(qsb::TOY_MAGIC.begin(), qsb::TOY_MAGIC.end());
    std::vector<unsigned char> version{qsb::TOY_VERSION};
    std::array<unsigned char, qsb::TOY_METADATA_SIZE> metadata{};
    metadata.fill(0x42);

    std::vector<unsigned char> payload(payload_bytes);
    for (size_t i = 0; i < payload.size(); ++i) payload[i] = i & 0xff;

    CScript script;
    script << OP_SHA256 << secret_hash << OP_EQUALVERIFY;
    script << magic << OP_DROP;
    script << version << OP_DROP;
    script << metadata << OP_DROP;
    for (size_t offset = 0; offset < payload.size(); offset += chunk_size) {
        const size_t size = std::min(chunk_size, payload.size() - offset);
        script << std::span<const unsigned char>(payload.data() + offset, size) << OP_DROP;
    }
    script << OP_TRUE;
    return script;
}

CScript StandardOutputScript(const CKey& key)
{
    return CScript() << ToByteVector(key.GetPubKey()) << OP_CHECKSIG;
}

struct QSBTxSetup : public TestingSetup {
    CKey spend_key;

    QSBTxSetup()
        : TestingSetup{ChainType::REGTEST}
    {
        constexpr std::array<unsigned char, 32> vch_key = {
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
        spend_key.Set(vch_key.begin(), vch_key.end(), true);
    }

    CMutableTransaction CreateValidTransaction(const std::vector<CTransactionRef>& input_transactions,
                                               const std::vector<COutPoint>& inputs,
                                               int input_height,
                                               const std::vector<CKey>& input_signing_keys,
                                               const std::vector<CTxOut>& outputs)
    {
        CMutableTransaction tx;
        tx.vin.reserve(inputs.size());
        tx.vout = outputs;
        for (const auto& outpoint : inputs) {
            tx.vin.emplace_back(outpoint, CScript{}, MAX_BIP125_RBF_SEQUENCE);
        }

        FillableSigningProvider keystore;
        for (const auto& key : input_signing_keys) keystore.AddKey(key);

        CCoinsView coins_view;
        CCoinsViewCache coins_cache(&coins_view);
        for (const auto& input_transaction : input_transactions) {
            AddCoins(coins_cache, *input_transaction, input_height);
        }

        std::map<COutPoint, Coin> input_coins;
        for (const auto& outpoint : inputs) {
            input_coins.emplace(outpoint, coins_cache.GetCoin(outpoint).value());
        }

        std::map<int, bilingual_str> input_errors;
        BOOST_REQUIRE(SignTransaction(tx, &keystore, input_coins, SIGHASH_ALL, input_errors));
        return tx;
    }

    CTransactionRef AddConfirmedSpendableCoin(CAmount amount)
    {
        CMutableTransaction parent;
        parent.vin.emplace_back(COutPoint(Txid::FromUint256(m_rng.rand256()), 0), CScript{}, 0xFFFFFFFF);
        parent.vout.emplace_back(amount, StandardOutputScript(spend_key));

        const auto tx = MakeTransactionRef(parent);
        LOCK(cs_main);
        AddCoins(Assert(m_node.chainman)->ActiveChainstate().CoinsTip(), *tx, /*nHeight=*/1);
        return tx;
    }
};

} // namespace

BOOST_AUTO_TEST_SUITE(qsb_tests)

BOOST_AUTO_TEST_CASE(qsb_toy_script_parser_matches_builder_shape)
{
    const auto secret = MakeSecret(1);
    const CScript funding_script = BuildToyFundingScript(secret);
    const CScript spend_script_sig = BuildToySpendScriptSig(secret);

    qsb::ToyFundingScriptInfo funding_info;
    BOOST_REQUIRE(qsb::MatchToyFundingScript(funding_script, &funding_info));
    BOOST_CHECK_EQUAL(funding_info.payload_bytes, 780U);
    BOOST_CHECK_EQUAL(funding_info.payload_chunk_count, 3U);
    BOOST_CHECK(funding_info.secret_hash == Sha256(secret));

    qsb::ToySpendScriptSigInfo spend_info;
    BOOST_REQUIRE(qsb::MatchToySpendScriptSig(spend_script_sig, &spend_info));
    BOOST_CHECK(spend_info.secret_preimage == secret);
}

BOOST_AUTO_TEST_CASE(qsb_toy_script_parser_rejects_small_payload)
{
    const auto secret = MakeSecret(9);
    const CScript undersized_script = BuildToyFundingScript(secret, /*payload_bytes=*/520, /*chunk_size=*/260);
    BOOST_CHECK(!qsb::MatchToyFundingScript(undersized_script));
}

BOOST_FIXTURE_TEST_CASE(qsb_operator_validation_accepts_funding_and_spend, QSBTxSetup)
{
    const CTransactionRef mature_coinbase = AddConfirmedSpendableCoin(50 * COIN);
    const auto secret = MakeSecret(21);
    const CScript funding_script = BuildToyFundingScript(secret);
    const CAmount funding_amount = mature_coinbase->vout[0].nValue - 1000;
    const auto funding_tx = MakeTransactionRef(CreateValidTransaction(
        /*input_transactions=*/{mature_coinbase},
        /*inputs=*/{COutPoint(mature_coinbase->GetHash(), 0)},
        /*input_height=*/1,
        /*input_signing_keys=*/{spend_key},
        /*outputs=*/{CTxOut{funding_amount, funding_script}}));

    {
        LOCK(cs_main);
        const MempoolAcceptResult public_result = m_node.chainman->ProcessTransaction(funding_tx);
        BOOST_REQUIRE_EQUAL(public_result.m_result_type, MempoolAcceptResult::ResultType::INVALID);
        BOOST_CHECK_EQUAL(public_result.m_state.GetRejectReason(), "scriptpubkey");

        const MempoolAcceptResult funding_result = m_node.chainman->ProcessTransaction(funding_tx, /*test_accept=*/false, /*allow_qsb_toy=*/true);
        BOOST_REQUIRE_MESSAGE(funding_result.m_result_type == MempoolAcceptResult::ResultType::VALID, funding_result.m_state.ToString());
    }

    CMutableTransaction spend_tx;
    spend_tx.vin.emplace_back(COutPoint(funding_tx->GetHash(), 0), BuildToySpendScriptSig(secret), 0xFFFFFFFF);
    spend_tx.vout.emplace_back(funding_amount - 1000, StandardOutputScript(spend_key));

    {
        LOCK(cs_main);
        const MempoolAcceptResult spend_result = m_node.chainman->ProcessTransaction(MakeTransactionRef(spend_tx), /*test_accept=*/false, /*allow_qsb_toy=*/true);
        BOOST_REQUIRE_EQUAL(spend_result.m_result_type, MempoolAcceptResult::ResultType::VALID);
    }
}

BOOST_FIXTURE_TEST_CASE(qsb_wrong_secret_stays_rejected, QSBTxSetup)
{
    const CTransactionRef mature_coinbase = AddConfirmedSpendableCoin(50 * COIN);
    const auto secret = MakeSecret(31);
    const CScript funding_script = BuildToyFundingScript(secret);
    const CAmount funding_amount = mature_coinbase->vout[0].nValue - 1000;
    const auto funding_tx = MakeTransactionRef(CreateValidTransaction(
        /*input_transactions=*/{mature_coinbase},
        /*inputs=*/{COutPoint(mature_coinbase->GetHash(), 0)},
        /*input_height=*/1,
        /*input_signing_keys=*/{spend_key},
        /*outputs=*/{CTxOut{funding_amount, funding_script}}));

    {
        LOCK(cs_main);
        const MempoolAcceptResult funding_result = m_node.chainman->ProcessTransaction(funding_tx, /*test_accept=*/false, /*allow_qsb_toy=*/true);
        BOOST_REQUIRE_MESSAGE(funding_result.m_result_type == MempoolAcceptResult::ResultType::VALID, funding_result.m_state.ToString());
    }

    CMutableTransaction spend_tx;
    spend_tx.vin.emplace_back(COutPoint(funding_tx->GetHash(), 0), BuildToySpendScriptSig(MakeSecret(32)), 0xFFFFFFFF);
    spend_tx.vout.emplace_back(funding_amount - 1000, StandardOutputScript(spend_key));

    {
        LOCK(cs_main);
        const MempoolAcceptResult spend_result = m_node.chainman->ProcessTransaction(MakeTransactionRef(spend_tx), /*test_accept=*/false, /*allow_qsb_toy=*/true);
        BOOST_REQUIRE_EQUAL(spend_result.m_result_type, MempoolAcceptResult::ResultType::INVALID);
        BOOST_CHECK_EQUAL(spend_result.m_state.GetRejectReason(), "qsb-secret-hash");
    }
}

BOOST_FIXTURE_TEST_CASE(qsb_does_not_allow_arbitrary_nonstandard_outputs, QSBTxSetup)
{
    const CTransactionRef mature_coinbase = AddConfirmedSpendableCoin(50 * COIN);
    const CAmount output_amount = mature_coinbase->vout[0].nValue - 1000;
    const auto nonstandard_tx = MakeTransactionRef(CreateValidTransaction(
        /*input_transactions=*/{mature_coinbase},
        /*inputs=*/{COutPoint(mature_coinbase->GetHash(), 0)},
        /*input_height=*/1,
        /*input_signing_keys=*/{spend_key},
        /*outputs=*/{CTxOut{output_amount, CScript{} << OP_TRUE}}));

    {
        LOCK(cs_main);
        const MempoolAcceptResult result = m_node.chainman->ProcessTransaction(nonstandard_tx, /*test_accept=*/false, /*allow_qsb_toy=*/true);
        BOOST_REQUIRE_EQUAL(result.m_result_type, MempoolAcceptResult::ResultType::INVALID);
        BOOST_CHECK_EQUAL(result.m_state.GetRejectReason(), "scriptpubkey");
    }
}

BOOST_AUTO_TEST_SUITE_END()
