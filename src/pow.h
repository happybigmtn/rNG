// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include <consensus/params.h>

#include <cstdint>
#include <string_view>

class CBlockHeader;
class CBlockIndex;
class uint256;
class arith_uint256;

/**
 * Convert nBits value to target.
 *
 * @param[in] nBits     compact representation of the target
 * @param[in] pow_limit PoW limit (consensus parameter)
 *
 * @return              the proof-of-work target or nullopt if the nBits value
 *                      is invalid (due to overflow or exceeding pow_limit)
 */
std::optional<arith_uint256> DeriveTarget(unsigned int nBits, uint256 pow_limit);

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params&);
bool CheckProofOfWorkImpl(uint256 hash, unsigned int nBits, const Consensus::Params&);

/** Fixed RandomX genesis seed phrase used by RNG mainnet. */
inline constexpr std::string_view kRandomXGenesisSeedPhrase{"RNG Genesis Seed"};

/**
 * Get the RandomX seed hash for a given block index.
 * The seed hash is derived from the block hash at the seed height.
 * For genesis block (height 0), uses Hash(kRandomXGenesisSeedPhrase).
 *
 * @param pindex Block index to get seed hash for
 * @return       The seed hash for RandomX
 */
uint256 GetRandomXSeedHash(const CBlockIndex* pindex);

/**
 * Compute the RandomX PoW hash for a block header.
 *
 * @param header      The block header to hash
 * @param seed_hash   The seed hash for this block's epoch
 * @return            The RandomX hash
 */
uint256 GetBlockPoWHash(const CBlockHeader& header, const uint256& seed_hash);

/**
 * Check whether a block satisfies the RandomX proof-of-work requirement.
 * This is the main PoW validation function for RNG.
 *
 * @param header  The block header
 * @param pindexPrev  The previous block index (for determining seed hash)
 * @param params  Consensus parameters
 * @return        true if PoW is valid
 */
bool CheckBlockProofOfWork(const CBlockHeader& header, const CBlockIndex* pindexPrev, const Consensus::Params& params);

/**
 * Return false if the proof-of-work requirement specified by new_nbits at a
 * given height is not possible, given the proof-of-work on the prior block as
 * specified by old_nbits.
 *
 * This function only checks that the new value is within a factor of 4 of the
 * old value for blocks at the difficulty adjustment interval, and otherwise
 * requires the values to be the same.
 *
 * Always returns true on networks where min difficulty blocks are allowed,
 * such as regtest/testnet.
 */
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits, uint32_t new_nbits);

#endif // BITCOIN_POW_H
