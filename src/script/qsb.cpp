// Copyright (c) 2026-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/qsb.h>

#include <script/script.h>

#include <algorithm>
#include <vector>

namespace qsb {
namespace {

bool ExpectOpcode(const CScript& script, CScript::const_iterator& pc, opcodetype expected)
{
    opcodetype opcode;
    return script.GetOp(pc, opcode) && opcode == expected;
}

bool ReadPush(const CScript& script,
              CScript::const_iterator& pc,
              std::vector<unsigned char>& data,
              bool require_minimal)
{
    opcodetype opcode;
    return script.GetOp(pc, opcode, data) && opcode <= OP_PUSHDATA4 && (!require_minimal || CheckMinimalPush(data, opcode));
}

template <size_t N>
bool ReadFixedPush(const CScript& script,
                   CScript::const_iterator& pc,
                   std::array<unsigned char, N>& output,
                   bool require_minimal)
{
    std::vector<unsigned char> data;
    if (!ReadPush(script, pc, data, require_minimal) || data.size() != N) return false;
    std::copy(data.begin(), data.end(), output.begin());
    return true;
}

bool ReadExpectedPush(const CScript& script,
                      CScript::const_iterator& pc,
                      const std::array<unsigned char, TOY_MAGIC_SIZE>& expected,
                      bool require_minimal)
{
    std::vector<unsigned char> data;
    return ReadPush(script, pc, data, require_minimal) && data.size() == expected.size() &&
           std::equal(data.begin(), data.end(), expected.begin(), expected.end());
}

bool ReadVersionPush(const CScript& script, CScript::const_iterator& pc)
{
    std::vector<unsigned char> data;
    return ReadPush(script, pc, data, /*require_minimal=*/false) && data.size() == 1 && data[0] == TOY_VERSION;
}

} // namespace

const std::array<unsigned char, TOY_MAGIC_SIZE> TOY_MAGIC{{'R', 'N', 'G', 'Q', 'S', 'B', 'V', '1'}};

bool MatchToyFundingScript(const CScript& script, ToyFundingScriptInfo* info)
{
    if (script.size() <= MAX_SCRIPT_ELEMENT_SIZE || script.size() > MAX_SCRIPT_SIZE || !script.HasValidOps()) {
        return false;
    }

    CScript::const_iterator pc = script.begin();
    ToyFundingScriptInfo parsed;
    if (!ExpectOpcode(script, pc, OP_SHA256)) return false;
    if (!ReadFixedPush(script, pc, parsed.secret_hash, /*require_minimal=*/false)) return false;
    if (!ExpectOpcode(script, pc, OP_EQUALVERIFY)) return false;

    if (!ReadExpectedPush(script, pc, TOY_MAGIC, /*require_minimal=*/false)) return false;
    if (!ExpectOpcode(script, pc, OP_DROP)) return false;
    if (!ReadVersionPush(script, pc)) return false;
    if (!ExpectOpcode(script, pc, OP_DROP)) return false;
    if (!ReadFixedPush(script, pc, parsed.metadata_commitment, /*require_minimal=*/false)) return false;
    if (!ExpectOpcode(script, pc, OP_DROP)) return false;

    while (pc != script.end()) {
        CScript::const_iterator peek = pc;
        opcodetype opcode;
        if (!script.GetOp(peek, opcode)) return false;
        if (opcode == OP_TRUE) {
            if (peek != script.end()) return false;
            pc = peek;
            break;
        }

        std::vector<unsigned char> chunk;
        if (!ReadPush(script, pc, chunk, /*require_minimal=*/false) || chunk.empty() || chunk.size() > MAX_SCRIPT_ELEMENT_SIZE) {
            return false;
        }
        parsed.payload_bytes += chunk.size();
        ++parsed.payload_chunk_count;
        if (!ExpectOpcode(script, pc, OP_DROP)) return false;
    }

    if (pc != script.end()) return false;
    if (parsed.payload_chunk_count < 2 || parsed.payload_bytes <= MAX_SCRIPT_ELEMENT_SIZE) return false;
    if (parsed.payload_chunk_count + 6 > static_cast<size_t>(MAX_OPS_PER_SCRIPT)) return false;

    if (info != nullptr) *info = parsed;
    return true;
}

bool MatchToySpendScriptSig(const CScript& script_sig, ToySpendScriptSigInfo* info)
{
    CScript::const_iterator pc = script_sig.begin();
    ToySpendScriptSigInfo parsed;
    if (!ReadFixedPush(script_sig, pc, parsed.secret_preimage, /*require_minimal=*/true)) return false;
    if (pc != script_sig.end()) return false;

    if (info != nullptr) *info = parsed;
    return true;
}

} // namespace qsb
