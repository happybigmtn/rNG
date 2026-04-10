// Copyright (c) 2024-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_INTERNAL_MINER_H
#define BITCOIN_NODE_INTERNAL_MINER_H

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <primitives/block.h>
#include <script/script.h>
#include <uint256.h>
#include <validationinterface.h>

class ChainstateManager;
class CConnman;
namespace interfaces { class Mining; }

namespace node {

/**
 * Internal multi-threaded miner for RNG.
 *
 * Architecture (v2 - per Codex roadmap):
 * - One COORDINATOR thread: creates block templates, monitors chain tip
 * - N WORKER threads: pure nonce grinding with no locks
 * - Event-driven: subscribes to ValidationSignals for instant new-block reaction
 * - Lock-free template sharing via atomic pointer swap
 * - Stride-based nonces: thread i tries nonces i, i+N, i+2N... (simpler than ranges)
 * - Backoff on bad conditions: exponential backoff when no peers/IBD/errors
 * - RandomX warmup: predictable startup with progress logging
 *
 * Safety guarantees (per Codex review):
 * - Mining is OFF by default (requires explicit -mine flag)
 * - Requires -mineaddress (no default, prevents accidental mining)
 * - Requires -minethreads (explicit thread count, logged loudly)
 * - Clean shutdown with proper thread join ordering
 * - Thread-safe statistics via atomics
 *
 * Usage:
 *   rngd -mine -mineaddress=rng1q... -minethreads=8
 */
class InternalMiner : public CValidationInterface {
public:
    /**
     * Construct internal miner.
     * Does NOT start mining - call Start() explicitly.
     */
    InternalMiner(ChainstateManager& chainman, interfaces::Mining& mining, CConnman* connman = nullptr);

    /**
     * Destructor ensures clean shutdown.
     * Calls Stop() if still running.
     */
    ~InternalMiner();

    // Disable copy/move
    InternalMiner(const InternalMiner&) = delete;
    InternalMiner& operator=(const InternalMiner&) = delete;
    InternalMiner(InternalMiner&&) = delete;
    InternalMiner& operator=(InternalMiner&&) = delete;

    /**
     * Start mining with specified configuration.
     *
     * @param num_threads   Number of worker threads (must be > 0)
     * @param coinbase_script  Script for coinbase output (validated address)
     * @param fast_mode     Use RandomX fast mode (2GB RAM) vs light (256MB)
     * @param low_priority  Run threads at nice 19 (low CPU priority)
     * @return true if started successfully
     */
    bool Start(int num_threads,
               const CScript& coinbase_script,
               bool fast_mode = true,
               bool low_priority = true);

    /**
     * Stop all mining threads.
     * Blocks until all threads have joined.
     * Safe to call multiple times.
     */
    void Stop();

    /**
     * Check if miner is currently running.
     */
    bool IsRunning() const { return m_running.load(std::memory_order_acquire); }

    /**
     * Get total hashes computed across all threads.
     */
    uint64_t GetHashCount() const { return m_hash_count.load(std::memory_order_relaxed); }

    /**
     * Get number of blocks successfully mined.
     */
    uint64_t GetBlocksFound() const { return m_blocks_found.load(std::memory_order_relaxed); }

    /**
     * Get number of stale blocks (found but rejected).
     */
    uint64_t GetStaleBlocks() const { return m_stale_blocks.load(std::memory_order_relaxed); }

    /**
     * Get current hashrate estimate (hashes per second).
     */
    double GetHashRate() const;

    /**
     * Get number of configured mining threads.
     */
    int GetThreadCount() const { return m_num_threads; }

    /**
     * Get number of template refreshes.
     */
    uint64_t GetTemplateCount() const { return m_template_count.load(std::memory_order_relaxed); }

    /**
     * Get start time (Unix timestamp).
     */
    int64_t GetStartTime() const { return m_start_time.load(std::memory_order_relaxed); }

    /**
     * Check if using fast mode (full dataset) or light mode.
     */
    bool IsFastMode() const { return m_using_fast_mode.load(std::memory_order_relaxed); }

protected:
    // CValidationInterface - event-driven block notifications
    void UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload) override;

private:
    /**
     * Shared mining context - passed to workers via atomic pointer.
     * Immutable once published (workers read-only).
     */
    struct MiningContext {
        CBlock block;              // Block template (workers modify nNonce only)
        uint256 seed_hash;         // RandomX seed hash
        unsigned int nBits;        // Difficulty bits for CheckProofOfWork
        uint64_t job_id;           // Monotonic ID to detect staleness
        int height;                // Block height being mined

        MiningContext() : nBits(0), job_id(0), height(0) {}
    };

    /**
     * Coordinator thread: creates templates and monitors chain.
     * Reacts to m_new_block_signal for event-driven updates.
     */
    void CoordinatorThread();

    /**
     * Worker thread: pure nonce grinding with stride pattern.
     * Thread i tries nonces: i, i+num_threads, i+2*num_threads, ...
     * @param thread_id  Unique thread identifier (0 to num_threads-1)
     */
    void WorkerThread(int thread_id);

    /**
     * Submit a found block to the network.
     * Thread-safe, called by workers when they find a valid block.
     */
    bool SubmitBlock(const CBlock& block);

    /**
     * Create a new block template.
     * Called by coordinator when tip changes or template is stale.
     * @return New mining context, or nullptr on failure
     */
    std::shared_ptr<MiningContext> CreateTemplate();

    /**
     * Check if conditions are good for mining.
     * @return true if we should mine, false if we should back off
     */
    bool ShouldMine() const;

    /**
     * Get backoff duration based on current conditions.
     * Uses exponential backoff with jitter.
     */
    std::chrono::milliseconds GetBackoffDuration() const;

    // References to node components (must outlive miner)
    ChainstateManager& m_chainman;
    interfaces::Mining& m_mining;
    CConnman* m_connman;  // May be nullptr

    // Mining configuration (set at Start(), immutable during mining)
    CScript m_coinbase_script;
    int m_num_threads{0};
    bool m_fast_mode{true};
    bool m_low_priority{true};

    // Thread management
    std::atomic<bool> m_running{false};
    std::thread m_coordinator_thread;
    std::vector<std::thread> m_worker_threads;

    // Event-driven signaling (from ValidationInterface)
    std::mutex m_signal_mutex;
    std::condition_variable m_new_block_cv;
    std::atomic<bool> m_new_block_signal{false};

    // Shared mining context (lock-free via atomic pointer)
    std::shared_ptr<MiningContext> m_current_context;
    std::mutex m_context_mutex;
    std::condition_variable m_context_cv;
    std::atomic<uint64_t> m_job_id{0};

    // Statistics (thread-safe)
    std::atomic<uint64_t> m_hash_count{0};
    std::atomic<uint64_t> m_blocks_found{0};
    std::atomic<uint64_t> m_stale_blocks{0};
    std::atomic<uint64_t> m_template_count{0};
    std::atomic<int64_t> m_start_time{0};
    std::atomic<bool> m_using_fast_mode{true};

    // Backoff state
    mutable std::atomic<int> m_backoff_level{0};

    // Constants
    static constexpr int64_t TEMPLATE_REFRESH_INTERVAL_SECS = 30;
    static constexpr uint64_t HASH_BATCH_SIZE = 10000;
    static constexpr uint64_t STALENESS_CHECK_INTERVAL = 1000;
    static constexpr int MAX_BACKOFF_LEVEL = 6;  // Max 64 seconds
    static constexpr int MIN_PEERS_FOR_MINING = 1;  // Allow bootstrapping with small peer set
};

} // namespace node

#endif // BITCOIN_NODE_INTERNAL_MINER_H
