// Copyright (c) 2024-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_RANDOMX_HASH_H
#define BITCOIN_CRYPTO_RANDOMX_HASH_H

#include <uint256.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>

/**
 * RandomX proof-of-work hash computation for RNG.
 *
 * RandomX is a CPU-optimized, ASIC-resistant PoW algorithm that uses:
 * - 2080 MiB dataset for fast mode (mining)
 * - 256 MiB cache for light mode (validation)
 *
 * RNG mainnet currently uses RandomX ARGON_SALT ("RNGCHAIN01")
 * and a fixed genesis seed phrase for all block heights.
 */

// Forward declarations from randomx.h
struct randomx_cache;
struct randomx_vm;
struct randomx_dataset;

/**
 * RandomX context manager - handles VM, cache, and dataset lifecycle.
 * Thread-safe singleton pattern for efficient resource management.
 */
class RandomXContext {
public:
    ~RandomXContext();

    // Singleton access
    static RandomXContext& GetInstance();

    /**
     * Compute RandomX hash of input data using the current seed.
     * Uses light mode (256 MiB) for validation efficiency.
     *
     * @param input     Data to hash (typically 80-byte block header)
     * @param seed_hash The seed hash for this block's epoch
     * @return          256-bit RandomX hash
     */
    uint256 Hash(std::span<const unsigned char> input, const uint256& seed_hash);

    /**
     * Compute RandomX hash in fast mode with full dataset.
     * Requires ~2080 MiB RAM. Use for mining operations.
     *
     * @param input     Data to hash (typically 80-byte block header)
     * @param seed_hash The seed hash for this block's epoch
     * @return          256-bit RandomX hash
     */
    uint256 HashFast(std::span<const unsigned char> input, const uint256& seed_hash);

    /**
     * Check if RandomX is properly initialized.
     */
    bool IsInitialized() const;

    /**
     * Initialize for a new seed hash. Call when seed epoch changes.
     * Light mode is initialized automatically; fast mode requires explicit call.
     *
     * @param seed_hash New seed hash for the epoch
     * @param fast_mode If true, also initialize the full dataset (~2 GiB)
     */
    void UpdateSeedHash(const uint256& seed_hash, bool fast_mode = false);

    /**
     * Get current seed hash.
     */
    std::optional<uint256> GetCurrentSeedHash() const;

    /**
     * Get the shared dataset for mining VMs.
     * Returns nullptr if fast mode not initialized.
     * Thread-safe to call; returned pointer is valid while context exists.
     */
    randomx_dataset* GetDataset() const;

    /**
     * Get the shared cache.
     * Returns nullptr if not initialized.
     */
    randomx_cache* GetCache() const;

    // Disable copy
    RandomXContext(const RandomXContext&) = delete;
    RandomXContext& operator=(const RandomXContext&) = delete;

private:
    RandomXContext();

    void InitLight(const uint256& seed_hash);
    void InitFast(const uint256& seed_hash);
    void Cleanup();

    mutable std::mutex m_mutex;
    randomx_cache* m_cache{nullptr};
    randomx_vm* m_vm_light{nullptr};
    randomx_vm* m_vm_fast{nullptr};
    randomx_dataset* m_dataset{nullptr};
    std::optional<uint256> m_current_seed_hash;
    bool m_fast_mode_initialized{false};
};

/**
 * Per-thread RandomX mining VM.
 *
 * Each mining thread should create its own RandomXMiningVM instance.
 * The VM uses the shared dataset from RandomXContext but has its own
 * VM instance, allowing lock-free parallel hashing.
 *
 * Usage:
 *   RandomXMiningVM vm;
 *   vm.Initialize(seed_hash);  // Once per seed epoch
 *   while (mining) {
 *       uint256 hash = vm.Hash(header_data);  // Lock-free!
 *   }
 */
class RandomXMiningVM {
public:
    RandomXMiningVM();
    ~RandomXMiningVM();

    // Disable copy
    RandomXMiningVM(const RandomXMiningVM&) = delete;
    RandomXMiningVM& operator=(const RandomXMiningVM&) = delete;

    // Allow move
    RandomXMiningVM(RandomXMiningVM&& other) noexcept;
    RandomXMiningVM& operator=(RandomXMiningVM&& other) noexcept;

    /**
     * Initialize VM for a seed hash.
     * Uses the shared dataset from RandomXContext.
     * Must be called before Hash().
     *
     * @param seed_hash Seed hash for the current epoch
     * @return true if initialized successfully
     */
    bool Initialize(const uint256& seed_hash, bool fast_mode, bool* fast_mode_used = nullptr);

    /**
     * Compute RandomX hash. LOCK-FREE.
     * Must call Initialize() first.
     *
     * @param input Data to hash (typically 80-byte block header)
     * @return 256-bit RandomX hash
     */
    uint256 Hash(std::span<const unsigned char> input);

    /**
     * Check if the VM is ready for hashing.
     */
    bool IsReady() const { return m_vm != nullptr; }

    /**
     * Check if the current seed hash matches.
     */
    bool HasSeed(const uint256& seed_hash) const;

    /**
     * Check whether this VM is using fast mode.
     */
    bool IsFastMode() const { return m_fast_mode; }

private:
    randomx_vm* m_vm{nullptr};
    uint256 m_seed_hash;
    bool m_initialized{false};
    bool m_fast_mode{false};
};

/**
 * Compute RandomX PoW hash for a block header.
 * This is the main entry point for PoW validation.
 *
 * @param header_data Serialized block header (80 bytes)
 * @param seed_hash   The seed hash for this block's epoch
 * @return            256-bit RandomX hash
 */
uint256 RandomXHash(std::span<const unsigned char> header_data, const uint256& seed_hash);

/**
 * Compute RandomX hash using light mode (validation).
 * Uses 256 MiB cache - slower but memory efficient.
 */
uint256 RandomXHashLight(std::span<const unsigned char> data, const uint256& seed_hash);

/**
 * Calculate the seed height for a given block height.
 * RNG mainnet uses a fixed genesis seed, so this always returns 0.
 *
 * @param block_height Current block height
 * @return             Height of the block whose hash is used as seed
 */
uint64_t GetRandomXSeedHeight(uint64_t block_height);

/**
 * Historical epoch length constant retained for compatibility/tests.
 */
constexpr uint64_t RANDOMX_EPOCH_LENGTH = 2048;

/**
 * Historical epoch lag constant retained for compatibility/tests.
 */
constexpr uint64_t RANDOMX_EPOCH_LAG = 64;

#endif // BITCOIN_CRYPTO_RANDOMX_HASH_H
