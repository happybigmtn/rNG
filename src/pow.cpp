// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <crypto/randomx_hash.h>
#include <hash.h>
#include <primitives/block.h>
#include <serialize.h>
#include <span.h>
#include <streams.h>
#include <uint256.h>
#include <util/check.h>
#include <algorithm>
#include <vector>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    unsigned int nProofOfWorkLimit = bnPowLimit.GetCompact();

    // Monero-style difficulty adjustment: recalculate every block using a
    // window of recent block timestamps and cumulative difficulties.
    //
    // Algorithm (from Monero's next_difficulty):
    //   1. Collect up to DIFFICULTY_WINDOW timestamps
    //   2. Sort timestamps, cut DIFFICULTY_CUT outliers from each end
    //   3. difficulty = (total_work_in_window * target_seconds) / time_span
    //
    // This provides smooth, responsive difficulty that adjusts every block,
    // resistant to timestamp manipulation via the cut mechanism.

    const int64_t DIFFICULTY_WINDOW = params.nDifficultyWindow;  // 720 (like Monero)
    const int64_t DIFFICULTY_CUT = params.nDifficultyCut;        // 60 (like Monero)
    const int64_t T = params.nPowTargetSpacing;                  // 120s target

    // Collect timestamps and per-block difficulties from the window
    // Walk back, then reverse so index 0 = oldest
    std::vector<int64_t> timestamps;
    std::vector<arith_uint256> difficulties;

    {
        const CBlockIndex* pindex = pindexLast;
        int64_t count = 0;
        while (pindex && count < DIFFICULTY_WINDOW) {
            // Skip genesis block - its timestamp is artificial and would
            // create a huge time_span that prevents difficulty adjustment
            if (pindex->nHeight == 0) break;

            timestamps.push_back(pindex->GetBlockTime());
            arith_uint256 target;
            target.SetCompact(pindex->nBits);
            if (target == 0) target = 1;
            arith_uint256 block_difficulty = bnPowLimit / target;
            if (block_difficulty == 0) block_difficulty = 1;
            difficulties.push_back(block_difficulty);
            pindex = pindex->pprev;
            count++;
        }
    }

    size_t length = timestamps.size();
    if (length <= 1) {
        return nProofOfWorkLimit;
    }

    // Reverse so index 0 = oldest block in window
    std::reverse(timestamps.begin(), timestamps.end());
    std::reverse(difficulties.begin(), difficulties.end());

    // Build cumulative difficulties (ascending: index 0 = oldest, smallest cumul)
    std::vector<arith_uint256> cumulative_difficulties(length);
    cumulative_difficulties[0] = difficulties[0];
    for (size_t i = 1; i < length; i++) {
        cumulative_difficulties[i] = cumulative_difficulties[i-1] + difficulties[i];
    }

    // Sort timestamps (Monero sorts to handle out-of-order timestamps)
    std::vector<int64_t> sorted_timestamps(timestamps);
    std::sort(sorted_timestamps.begin(), sorted_timestamps.end());

    // Cut outliers from each end
    size_t cut_begin, cut_end;
    if (length <= static_cast<size_t>(DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT)) {
        cut_begin = 0;
        cut_end = length;
    } else {
        cut_begin = (length - (DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT) + 1) / 2;
        cut_end = cut_begin + (DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT);
    }

    if (cut_begin + 2 > cut_end || cut_end > length) {
        return nProofOfWorkLimit;
    }

    int64_t time_span = sorted_timestamps[cut_end - 1] - sorted_timestamps[cut_begin];
    if (time_span <= 0) {
        time_span = 1;
    }

    arith_uint256 total_work = cumulative_difficulties[cut_end - 1] - cumulative_difficulties[cut_begin];
    if (total_work == 0) {
        return nProofOfWorkLimit;
    }

    // difficulty = total_work * T / time_span
    // Use explicit uint32_t cast for T to ensure arith_uint256 multiplication works
    arith_uint256 bnT(static_cast<uint32_t>(T));
    arith_uint256 bnTimeSpan(static_cast<uint64_t>(time_span));
    arith_uint256 next_difficulty = (total_work * bnT + bnTimeSpan - 1) / bnTimeSpan;
    if (next_difficulty == 0) next_difficulty = 1;

    // Convert difficulty back to nBits target: target = powLimit / difficulty
    arith_uint256 bnNew = bnPowLimit / next_difficulty;
    if (bnNew > bnPowLimit) bnNew = bnPowLimit;
    if (bnNew == 0) bnNew = 1;

    return bnNew.GetCompact();
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;

    // Special difficulty rule for Testnet4
    if (params.enforce_BIP94) {
        // Here we use the first block of the difficulty period. This way
        // the real difficulty is always preserved in the first block as
        // it is not allowed to use the min-difficulty exception.
        int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
        const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
        bnNew.SetCompact(pindexFirst->nBits);
    } else {
        bnNew.SetCompact(pindexLast->nBits);
    }

    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

// With Monero-style per-block difficulty adjustment, every block can have a
// different difficulty. We allow all transitions (the algorithm self-regulates).
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits, uint32_t new_nbits)
{
    // Monero-style: difficulty changes every block, no fixed interval check needed
    return true;
}

// Bypasses the actual proof of work check during fuzz testing with a simplified validation checking whether
// the most significant bit of the last byte of the hash is set.
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    if (EnableFuzzDeterminism()) return (hash.data()[31] & 0x80) == 0;
    return CheckProofOfWorkImpl(hash, nBits, params);
}

std::optional<arith_uint256> DeriveTarget(unsigned int nBits, const uint256 pow_limit)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(pow_limit))
        return {};

    return bnTarget;
}

bool CheckProofOfWorkImpl(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    auto bnTarget{DeriveTarget(nBits, params.powLimit)};
    if (!bnTarget) return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}

// ============================================================================
// RandomX Proof-of-Work Functions
// ============================================================================

uint256 GetRandomXSeedHash(const CBlockIndex* pindex)
{
    if (!pindex) {
        // Consensus constant: changing this phrase would fork the live chain.
        return Hash(std::string(kRandomXGenesisSeedPhrase));
    }

    // Get seed height for this block
    uint64_t seed_height = GetRandomXSeedHeight(static_cast<uint64_t>(pindex->nHeight));

    if (seed_height == 0) {
        // Use genesis seed
        return Hash(std::string(kRandomXGenesisSeedPhrase));
    }

    // Navigate to the seed block
    const CBlockIndex* seed_block = pindex;
    while (seed_block && seed_block->nHeight > static_cast<int>(seed_height)) {
        seed_block = seed_block->pprev;
    }

    if (!seed_block || seed_block->nHeight != static_cast<int>(seed_height)) {
        // Shouldn't happen, but fallback to genesis seed
        return Hash(std::string(kRandomXGenesisSeedPhrase));
    }

    // Return the block hash at seed height
    return seed_block->GetBlockHash();
}

uint256 GetBlockPoWHash(const CBlockHeader& header, const uint256& seed_hash)
{
    // Serialize the block header to bytes
    DataStream ss{};
    ss << header;

    // Compute RandomX hash
    return RandomXHash(MakeUCharSpan(ss), seed_hash);
}

bool CheckBlockProofOfWork(const CBlockHeader& header, const CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    // Get the seed hash for this block's epoch
    // If pindexPrev is null, we're checking genesis (use genesis seed)
    uint256 seed_hash;
    if (pindexPrev) {
        // Get seed hash based on the block height we're validating (pindexPrev->nHeight + 1)
        uint64_t block_height = static_cast<uint64_t>(pindexPrev->nHeight + 1);
        uint64_t seed_height = GetRandomXSeedHeight(block_height);

        if (seed_height == 0) {
            seed_hash = Hash(std::string(kRandomXGenesisSeedPhrase));
        } else {
            const CBlockIndex* seed_block = pindexPrev;
            while (seed_block && seed_block->nHeight > static_cast<int>(seed_height)) {
                seed_block = seed_block->pprev;
            }
            if (seed_block && seed_block->nHeight == static_cast<int>(seed_height)) {
                seed_hash = seed_block->GetBlockHash();
            } else {
                seed_hash = Hash(std::string(kRandomXGenesisSeedPhrase));
            }
        }
    } else {
        // Genesis block
        seed_hash = Hash(std::string(kRandomXGenesisSeedPhrase));
    }

    // Compute RandomX PoW hash
    uint256 pow_hash = GetBlockPoWHash(header, seed_hash);

    // Check against difficulty target
    return CheckProofOfWorkImpl(pow_hash, header.nBits, params);
}
