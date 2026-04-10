// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_QSB_H
#define BITCOIN_SCRIPT_QSB_H

#include <array>
#include <cstddef>
#include <cstdint>

class CScript;

namespace qsb {

static constexpr size_t TOY_SECRET_SIZE{32};
static constexpr size_t TOY_METADATA_SIZE{32};
static constexpr size_t TOY_MAGIC_SIZE{8};
static constexpr uint8_t TOY_VERSION{1};

extern const std::array<unsigned char, TOY_MAGIC_SIZE> TOY_MAGIC;

struct ToyFundingScriptInfo {
    std::array<unsigned char, TOY_SECRET_SIZE> secret_hash{};
    std::array<unsigned char, TOY_METADATA_SIZE> metadata_commitment{};
    size_t payload_bytes{0};
    size_t payload_chunk_count{0};
};

struct ToySpendScriptSigInfo {
    std::array<unsigned char, TOY_SECRET_SIZE> secret_preimage{};
};

bool MatchToyFundingScript(const CScript& script, ToyFundingScriptInfo* info = nullptr);
bool MatchToySpendScriptSig(const CScript& script_sig, ToySpendScriptSigInfo* info = nullptr);

} // namespace qsb

#endif // BITCOIN_SCRIPT_QSB_H
