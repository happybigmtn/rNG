// Copyright (c) 2024-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <crypto/randomx_hash.h>
#include <hash.h>
#include <pow.h>
#include <primitives/block.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <vector>

BOOST_FIXTURE_TEST_SUITE(randomx_tests, BasicTestingSetup)

// =============================================================================
// Phase 1.2 Tests: RandomX Hash Function
// =============================================================================

/**
 * Test: Known hash vector (deterministic output)
 * Acceptance: Same input produces same output; hash is not all zeros.
 */
BOOST_AUTO_TEST_CASE(randomx_known_vector)
{
    // Create test input (80-byte "header")
    std::vector<uint8_t> header(80, 0);

    // Use genesis seed
    uint256 seed = Hash(std::string(kRandomXGenesisSeedPhrase));

    // Compute hash twice
    uint256 hash1 = RandomXHash(header, seed);
    uint256 hash2 = RandomXHash(header, seed);

    // Same input = same output (deterministic)
    BOOST_CHECK_EQUAL(hash1, hash2);

    // Hash is not all zeros (actually computed something)
    BOOST_CHECK(hash1 != uint256());

    // Hash is not the same as seed (different operation)
    BOOST_CHECK(hash1 != seed);
}

/**
 * Test: Different input produces different output
 * Acceptance: RandomX is a proper hash function with collision resistance.
 */
BOOST_AUTO_TEST_CASE(randomx_different_input)
{
    std::vector<uint8_t> header1(80, 0);
    std::vector<uint8_t> header2(80, 1); // Different content
    uint256 seed = Hash(std::string(kRandomXGenesisSeedPhrase));

    uint256 hash1 = RandomXHash(header1, seed);
    uint256 hash2 = RandomXHash(header2, seed);

    // Different input should produce different output
    BOOST_CHECK(hash1 != hash2);
}

/**
 * Test: Different seed produces different output
 * Acceptance: The seed hash properly influences the RandomX computation.
 */
BOOST_AUTO_TEST_CASE(randomx_different_seed)
{
    std::vector<uint8_t> header(80, 0);
    uint256 seed1 = uint256(); // All zeros
    uint256 seed2{"0000000000000000000000000000000000000000000000000000000000000001"};

    uint256 hash1 = RandomXHash(header, seed1);
    uint256 hash2 = RandomXHash(header, seed2);

    // Different seed should produce different output
    BOOST_CHECK(hash1 != hash2);
}

/**
 * Test: Light mode works (same as default Hash function)
 * Acceptance: RandomXHashLight produces valid hashes.
 */
BOOST_AUTO_TEST_CASE(randomx_light_mode)
{
    std::vector<uint8_t> header(80, 0);
    uint256 seed = Hash(std::string(kRandomXGenesisSeedPhrase));

    // Light mode should work (uses 256 MiB cache)
    uint256 hash = RandomXHashLight(header, seed);
    BOOST_CHECK(hash != uint256());

    // Light mode should give same result as regular RandomXHash
    // (since RandomXHash uses light mode internally for validation)
    uint256 hash2 = RandomXHash(header, seed);
    BOOST_CHECK_EQUAL(hash, hash2);
}

// =============================================================================
// Phase 1.3 Tests: Seed Height Calculation
// =============================================================================

/**
 * Test: Seed height remains fixed at genesis for all heights.
 * Acceptance: RNG's fixed-seed policy returns height 0 always.
 */
BOOST_AUTO_TEST_CASE(seed_height_calculation)
{
    // RNG intentionally uses a fixed genesis seed for chain-stability.
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(0), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(64), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(2047), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(2048), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(2111), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(2112), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(4000), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(4095), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(4096), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(4159), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(4160), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(6000), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(6207), 0ULL);
    BOOST_CHECK_EQUAL(GetRandomXSeedHeight(6208), 0ULL);
}

/**
 * Test: Epoch length and lag constants are correct.
 * Acceptance: Constants match specs/randomx.md.
 */
BOOST_AUTO_TEST_CASE(randomx_constants)
{
    // From specs/randomx.md:
    // - Epoch: 2048 blocks (~34 hours at 60s blocks)
    // - Lag: 64 blocks (~1 hour)
    BOOST_CHECK_EQUAL(RANDOMX_EPOCH_LENGTH, 2048ULL);
    BOOST_CHECK_EQUAL(RANDOMX_EPOCH_LAG, 64ULL);
}

// =============================================================================
// Phase 1.4 Tests: Block PoW Validation
// =============================================================================

/**
 * Test: Block header serialization for RandomX
 * Acceptance: Serialized header is 80 bytes (standard Bitcoin header size).
 */
BOOST_AUTO_TEST_CASE(block_header_serialization)
{
    CBlockHeader header;
    header.nVersion = 0x20000000;
    header.hashPrevBlock = uint256();
    header.hashMerkleRoot = uint256();
    header.nTime = 1234567890;
    header.nBits = 0x207fffff;
    header.nNonce = 0;

    DataStream ss{};
    ss << header;

    // Block header should be exactly 80 bytes
    BOOST_CHECK_EQUAL(ss.size(), 80U);
}

/**
 * Test: GetBlockPoWHash produces valid hash
 * Acceptance: PoW hash computation works on block headers.
 */
BOOST_AUTO_TEST_CASE(get_block_pow_hash)
{
    CBlockHeader header;
    header.nVersion = 0x20000000;
    header.hashPrevBlock = uint256();
    header.hashMerkleRoot = uint256();
    header.nTime = 1234567890;
    header.nBits = 0x207fffff;
    header.nNonce = 0;

    uint256 seed = Hash(std::string(kRandomXGenesisSeedPhrase));
    uint256 pow_hash = GetBlockPoWHash(header, seed);

    // Hash should not be zero
    BOOST_CHECK(pow_hash != uint256());

    // Same header should produce same hash
    uint256 pow_hash2 = GetBlockPoWHash(header, seed);
    BOOST_CHECK_EQUAL(pow_hash, pow_hash2);

    // Different nonce should produce different hash
    header.nNonce = 1;
    uint256 pow_hash3 = GetBlockPoWHash(header, seed);
    BOOST_CHECK(pow_hash != pow_hash3);
}

/**
 * Test: Genesis seed hash computation
 * Acceptance: Genesis seed is SHA256(kRandomXGenesisSeedPhrase).
 */
BOOST_AUTO_TEST_CASE(genesis_seed_hash)
{
    // GetRandomXSeedHash with null pindex should return genesis seed
    uint256 expected = Hash(std::string(kRandomXGenesisSeedPhrase));
    uint256 actual = GetRandomXSeedHash(nullptr);

    BOOST_CHECK_EQUAL(actual, expected);
}

/**
 * Test: Known live mainnet header validates under the shipped PoW constants.
 * Acceptance: The first post-reset mainnet block must not regress to high-hash.
 */
BOOST_AUTO_TEST_CASE(live_mainnet_block1_pow)
{
    const auto main_params = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto seed = Hash(std::string(kRandomXGenesisSeedPhrase));

    CBlockHeader header;
    DataStream stream{ParseHex("00000020e4ac366d49bc6b6b10e9aa1818f6d8e5619b7e06807938078cc85df882a4a683318e090ca2ec77a92235340ccdd778612b1b061a9a2084c367de15d09959fb53a3d8a069ffff7f2003000000")};
    stream >> header;

    BOOST_CHECK_EQUAL(header.GetHash().GetHex(), "ea5820b9302d87b6fcece3a8fa5ff7dbd5df2c1c6e377ea7e471e8912139b46b");
    BOOST_CHECK(CheckProofOfWork(GetBlockPoWHash(header, seed), header.nBits, main_params->GetConsensus()));
}

// =============================================================================
// Integration Tests
// =============================================================================

/**
 * Test: RandomX context singleton works correctly.
 * Acceptance: Repeated calls work, context is properly managed.
 */
BOOST_AUTO_TEST_CASE(randomx_context_singleton)
{
    RandomXContext& ctx = RandomXContext::GetInstance();

    // Should start uninitialized or get initialized on first use
    std::vector<uint8_t> data(80, 42);
    uint256 seed = Hash(std::string("Test Seed"));

    // This will initialize the context if needed
    uint256 hash1 = ctx.Hash(data, seed);
    BOOST_CHECK(hash1 != uint256());

    // Context should now be initialized
    BOOST_CHECK(ctx.IsInitialized());

    // Same seed should be cached
    auto cached_seed = ctx.GetCurrentSeedHash();
    BOOST_CHECK(cached_seed.has_value());
    BOOST_CHECK_EQUAL(*cached_seed, seed);

    // Repeated hash should give same result
    uint256 hash2 = ctx.Hash(data, seed);
    BOOST_CHECK_EQUAL(hash1, hash2);
}

/**
 * Test: Seed hash update works correctly.
 * Acceptance: Changing seed produces different hashes.
 */
BOOST_AUTO_TEST_CASE(randomx_context_seed_update)
{
    RandomXContext& ctx = RandomXContext::GetInstance();

    std::vector<uint8_t> data(80, 0);
    uint256 seed1 = Hash(std::string("Seed One"));
    uint256 seed2 = Hash(std::string("Seed Two"));

    uint256 hash1 = ctx.Hash(data, seed1);
    uint256 hash2 = ctx.Hash(data, seed2);

    // Different seeds should produce different hashes
    BOOST_CHECK(hash1 != hash2);

    // Going back to first seed should give same hash as before
    uint256 hash3 = ctx.Hash(data, seed1);
    BOOST_CHECK_EQUAL(hash1, hash3);
}

BOOST_AUTO_TEST_SUITE_END()
