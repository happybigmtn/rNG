// Copyright (c) 2024-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/internal_miner.h>

#include <array>
#include <chain.h>
#include <chainparams.h>
#include <consensus/merkle.h>
#include <crypto/randomx_hash.h>
#include <cstring>
#include <interfaces/mining.h>
#include <logging.h>
#include <net.h>
#include <pow.h>
#include <primitives/block.h>
#include <streams.h>
#include <util/batchpriority.h>
#include <util/signalinterrupt.h>
#include <util/time.h>
#include <validation.h>

#include <random>

namespace node {

InternalMiner::InternalMiner(ChainstateManager& chainman, interfaces::Mining& mining, CConnman* connman)
    : m_chainman(chainman), m_mining(mining), m_connman(connman)
{
    LogInfo("InternalMiner: Initialized (not started)\n");
}

InternalMiner::~InternalMiner()
{
    Stop();
}

bool InternalMiner::Start(int num_threads,
                          const CScript& coinbase_script,
                          bool fast_mode,
                          bool low_priority)
{
    // Validate parameters
    if (num_threads <= 0) {
        LogInfo("InternalMiner: ERROR - num_threads must be > 0\n");
        return false;
    }

    if (coinbase_script.empty()) {
        LogInfo("InternalMiner: ERROR - coinbase_script is empty\n");
        return false;
    }

    // Prevent double-start
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        LogInfo("InternalMiner: Already running\n");
        return false;
    }

    // Store configuration
    m_coinbase_script = coinbase_script;
    m_num_threads = num_threads;
    m_fast_mode = fast_mode;
    m_low_priority = low_priority;

    // Reset statistics
    m_hash_count.store(0, std::memory_order_relaxed);
    m_blocks_found.store(0, std::memory_order_relaxed);
    m_stale_blocks.store(0, std::memory_order_relaxed);
    m_template_count.store(0, std::memory_order_relaxed);
    m_start_time.store(GetTime(), std::memory_order_relaxed);
    m_job_id.store(0, std::memory_order_relaxed);
    m_backoff_level.store(0, std::memory_order_relaxed);
    m_using_fast_mode.store(fast_mode, std::memory_order_relaxed);

    // Log startup with full configuration (LOUD per Codex recommendation)
    LogInfo("╔══════════════════════════════════════════════════════════════╗\n");
    LogInfo("║          INTERNAL MINER v2 STARTING                         ║\n");
    LogInfo("╠══════════════════════════════════════════════════════════════╣\n");
    LogInfo("║  Worker Threads: %-44d ║\n", num_threads);
    LogInfo("║  Nonce Pattern:  Stride (i, i+N, i+2N, ...)                  ║\n");
    LogInfo("║  RandomX Mode:   %-44s ║\n", fast_mode ? "FAST (2GB RAM)" : "LIGHT (256MB RAM)");
    LogInfo("║  Priority:       %-44s ║\n", low_priority ? "LOW (nice 19)" : "NORMAL");
    LogInfo("║  Script Size:    %-44zu ║\n", coinbase_script.size());
    LogInfo("╠══════════════════════════════════════════════════════════════╣\n");
    LogInfo("║  Features:                                                   ║\n");
    LogInfo("║    ✓ Event-driven block notifications                       ║\n");
    LogInfo("║    ✓ Per-thread RandomX VMs (lock-free)                     ║\n");
    LogInfo("║    ✓ Exponential backoff on bad conditions                  ║\n");
    LogInfo("║    ✓ Automatic light-mode fallback                          ║\n");
    LogInfo("╚══════════════════════════════════════════════════════════════╝\n");

        // Note: RandomX dataset initialization happens when workers get their first template
    // with the correct seed hash. This avoids initializing with wrong seed.
    LogInfo("InternalMiner: RandomX will initialize on first template\n");
    m_using_fast_mode.store(fast_mode, std::memory_order_relaxed);

    // Register for block notifications (event-driven)
    if (m_chainman.m_options.signals) {
        m_chainman.m_options.signals->RegisterValidationInterface(this);
        LogInfo("InternalMiner: Registered for block notifications\n");
    }

    // Start coordinator thread first
    m_coordinator_thread = std::thread(&InternalMiner::CoordinatorThread, this);

    // Wait for first template
    {
        std::unique_lock<std::mutex> lock(m_context_mutex);
        bool got_template = m_context_cv.wait_for(lock, std::chrono::seconds(30), [this] {
            return m_current_context != nullptr || !m_running.load(std::memory_order_acquire);
        });
        if (!got_template || !m_current_context) {
            LogInfo("InternalMiner: Timeout waiting for first template\n");
            // Continue anyway, coordinator will keep trying
        }
    }

    // Launch worker threads
    m_worker_threads.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        m_worker_threads.emplace_back(&InternalMiner::WorkerThread, this, i);
    }

    LogInfo("InternalMiner: Started coordinator + %d worker threads\n", num_threads);
    return true;
}

void InternalMiner::Stop()
{
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        return;
    }

    LogInfo("InternalMiner: Stopping...\n");

    // Unregister from block notifications
    if (m_chainman.m_options.signals) {
        m_chainman.m_options.signals->UnregisterValidationInterface(this);
    }

    // Wake up all waiting threads
    m_new_block_cv.notify_all();
    m_context_cv.notify_all();

    // Stop workers first
    for (auto& thread : m_worker_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_worker_threads.clear();

    // Then coordinator
    if (m_coordinator_thread.joinable()) {
        m_coordinator_thread.join();
    }

    // Clear context
    {
        std::lock_guard<std::mutex> lock(m_context_mutex);
        m_current_context.reset();
    }

    // Final statistics
    int64_t elapsed = GetTime() - m_start_time.load(std::memory_order_relaxed);
    uint64_t hashes = m_hash_count.load(std::memory_order_relaxed);
    uint64_t blocks = m_blocks_found.load(std::memory_order_relaxed);
    uint64_t stale = m_stale_blocks.load(std::memory_order_relaxed);
    uint64_t templates = m_template_count.load(std::memory_order_relaxed);

    LogInfo("╔══════════════════════════════════════════════════════════════╗\n");
    LogInfo("║          INTERNAL MINER STOPPED                              ║\n");
    LogInfo("╠══════════════════════════════════════════════════════════════╣\n");
    LogInfo("║  Runtime:        %-42ld s ║\n", elapsed);
    LogInfo("║  Total Hashes:   %-44lu ║\n", hashes);
    LogInfo("║  Blocks Found:   %-44lu ║\n", blocks);
    LogInfo("║  Stale Blocks:   %-44lu ║\n", stale);
    LogInfo("║  Templates:      %-44lu ║\n", templates);
    if (elapsed > 0) {
        LogInfo("║  Avg Hashrate:   %-40.2f H/s ║\n", static_cast<double>(hashes) / elapsed);
    }
    LogInfo("╚══════════════════════════════════════════════════════════════╝\n");
}

double InternalMiner::GetHashRate() const
{
    int64_t elapsed = GetTime() - m_start_time.load(std::memory_order_relaxed);
    if (elapsed <= 0) return 0.0;
    return static_cast<double>(m_hash_count.load(std::memory_order_relaxed)) / elapsed;
}

// Event-driven: called when a new block is connected
void InternalMiner::UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload)
{
    if (!m_running.load(std::memory_order_acquire)) return;

    // Signal coordinator to refresh template
    {
        std::lock_guard<std::mutex> lock(m_signal_mutex);
        m_new_block_signal.store(true, std::memory_order_release);
    }
    m_new_block_cv.notify_one();

    // Reset backoff on successful block
    m_backoff_level.store(0, std::memory_order_relaxed);
}

bool InternalMiner::ShouldMine() const
{
    // NOTE(RNG): Do NOT gate mining on IBD.
    //
    // Bitcoin Core treats a tip older than DEFAULT_MAX_TIP_AGE as "IBD", which is
    // sensible for a mature network but can deadlock a young chain: if block
    // production pauses for >24h, nodes stay in IBD, the internal miner refuses
    // to create templates, and the chain can never recover.
    //
    // Peering / partition safety is handled below via MIN_PEERS_FOR_MINING.

    // Check peer count if we have connman
    if (m_connman) {
        int peer_count = m_connman->GetNodeCount(ConnectionDirection::Both);
        if (peer_count < MIN_PEERS_FOR_MINING) {
            return false;
        }
    }

    return true;
}

std::chrono::milliseconds InternalMiner::GetBackoffDuration() const
{
    int level = m_backoff_level.load(std::memory_order_relaxed);

    // Exponential backoff: 1s, 2s, 4s, 8s, 16s, 32s, 64s max
    int64_t base_ms = 1000 * (1 << std::min(level, MAX_BACKOFF_LEVEL));

    // Add jitter (0-25%)
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int64_t> dist(0, base_ms / 4);

    return std::chrono::milliseconds(base_ms + dist(gen));
}

std::shared_ptr<InternalMiner::MiningContext> InternalMiner::CreateTemplate()
{
    // Get chain state
    const CBlockIndex* tip_index;
    {
        LOCK(cs_main);
        tip_index = m_chainman.ActiveChain().Tip();
        if (!tip_index) {
            return nullptr;
        }
    }

    // Create block template
    auto block_template = m_mining.createNewBlock({
        .coinbase_output_script = m_coinbase_script
    });

    if (!block_template) {
        return nullptr;
    }

    // Build context
    auto ctx = std::make_shared<MiningContext>();
    ctx->block = block_template->getBlock();
    ctx->block.hashMerkleRoot = BlockMerkleRoot(ctx->block);
    ctx->nBits = ctx->block.nBits;
    ctx->job_id = m_job_id.fetch_add(1, std::memory_order_relaxed) + 1;
    ctx->height = tip_index->nHeight + 1;

    // Get RandomX seed hash
    {
        LOCK(cs_main);
        ctx->seed_hash = GetRandomXSeedHash(tip_index);
    }

    m_template_count.fetch_add(1, std::memory_order_relaxed);

    return ctx;
}

void InternalMiner::CoordinatorThread()
{
    LogInfo("InternalMiner: Coordinator thread started\n");

    uint256 last_tip;
    int64_t last_template_time = 0;

    while (m_running.load(std::memory_order_acquire) &&
           !static_cast<bool>(m_chainman.m_interrupt)) {

        // Check mining conditions
        if (!ShouldMine()) {
            auto backoff = GetBackoffDuration();
            m_backoff_level.fetch_add(1, std::memory_order_relaxed);

            LogInfo("InternalMiner: Bad conditions, backing off %lldms\n",
                      static_cast<long long>(backoff.count()));

            std::unique_lock<std::mutex> lock(m_signal_mutex);
            m_new_block_cv.wait_for(lock, backoff, [this] {
                return m_new_block_signal.load(std::memory_order_acquire) ||
                       !m_running.load(std::memory_order_acquire);
            });
            m_new_block_signal.store(false, std::memory_order_release);
            continue;
        }

        // Reset backoff on good conditions
        m_backoff_level.store(0, std::memory_order_relaxed);

        // Get current tip
        uint256 current_tip;
        {
            LOCK(cs_main);
            const CBlockIndex* tip = m_chainman.ActiveChain().Tip();
            if (tip) current_tip = tip->GetBlockHash();
        }

        // Check if we need a new template
        bool need_template = (current_tip != last_tip) ||
                            (GetTime() - last_template_time >= TEMPLATE_REFRESH_INTERVAL_SECS) ||
                            (m_job_id.load(std::memory_order_relaxed) == 0);

        if (need_template) {
            auto ctx = CreateTemplate();

            if (!ctx) {
                auto backoff = GetBackoffDuration();
                m_backoff_level.fetch_add(1, std::memory_order_relaxed);
                LogInfo("InternalMiner: Template creation failed, backing off\n");
                std::this_thread::sleep_for(backoff);
                continue;
            }

            // Publish new template
            {
                std::lock_guard<std::mutex> lock(m_context_mutex);
                m_current_context = ctx;
            }
            m_context_cv.notify_all();

            last_tip = current_tip;
            last_template_time = GetTime();

            if (ctx->job_id == 1) {
                LogInfo("InternalMiner: First template ready (height %d)\n", ctx->height);
            } else {
                LogInfo("InternalMiner: New template #%lu (height %d)\n", ctx->job_id, ctx->height);
            }
        }

        // Wait for new block signal or timeout
        {
            std::unique_lock<std::mutex> lock(m_signal_mutex);
            m_new_block_cv.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return m_new_block_signal.load(std::memory_order_acquire) ||
                       !m_running.load(std::memory_order_acquire);
            });
            m_new_block_signal.store(false, std::memory_order_release);
        }
    }

    LogInfo("InternalMiner: Coordinator thread stopped\n");
}

void InternalMiner::WorkerThread(int thread_id)
{
    LogInfo("InternalMiner: Worker %d started (stride pattern)\n", thread_id);

    if (m_low_priority) {
        ScheduleBatchPriority();
    }

    // Create per-thread RandomX VM
    RandomXMiningVM mining_vm;

    // Local state
    uint64_t local_hashes = 0;
    uint64_t last_job_id = 0;
    std::shared_ptr<MiningContext> ctx;
    CBlock working_block;
    std::array<unsigned char, 80> header_buf{};  // Pre-serialized header (Codex optimization)
    uint32_t nonce_counter = 0;  // Stride nonce with natural overflow

    while (m_running.load(std::memory_order_acquire) &&
           !static_cast<bool>(m_chainman.m_interrupt)) {

        // Check for new template
        uint64_t current_job = m_job_id.load(std::memory_order_acquire);
        if (current_job != last_job_id || !ctx) {
            // Get new context
            {
                std::unique_lock<std::mutex> lock(m_context_mutex);
                if (!m_current_context) {
                    m_context_cv.wait(lock, [this] {
                        return m_current_context != nullptr ||
                               !m_running.load(std::memory_order_acquire);
                    });
                    if (!m_running.load(std::memory_order_acquire)) break;
                }
                ctx = m_current_context;
            }

            if (!ctx) continue;

            // Initialize/update per-thread VM if seed changed
            if (!mining_vm.HasSeed(ctx->seed_hash)) {
                bool fast_mode_used = m_fast_mode;
                if (!mining_vm.Initialize(ctx->seed_hash, m_fast_mode, &fast_mode_used)) {
                    LogInfo("InternalMiner: Worker %d VM init failed, retrying...\n", thread_id);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                m_using_fast_mode.store(fast_mode_used, std::memory_order_relaxed);
                if (m_fast_mode && !fast_mode_used) {
                    LogWarning("InternalMiner: Worker %d is mining in RandomX light mode after fast mode fallback\n", thread_id);
                }
            }

            // Copy template and PRE-SERIALIZE header once (Codex optimization)
            // This avoids per-hash DataStream allocation which kills performance
            working_block = ctx->block;
            DataStream ss{};
            ss << static_cast<const CBlockHeader&>(working_block);
            assert(ss.size() == 80);  // Standard header size
            std::memcpy(header_buf.data(), ss.data(), 80);

            // Reset nonce counter for this template
            nonce_counter = static_cast<uint32_t>(thread_id);
            last_job_id = ctx->job_id;
        }

        // STRIDE-BASED NONCE GRINDING (Codex optimized)
        // Thread i tries: i, i+N, i+2N, i+3N, ... using natural uint32_t overflow
        // Header is pre-serialized; only mutate nonce bytes (offset 76-79)
        for (uint64_t iter = 0; iter < STALENESS_CHECK_INTERVAL; ++iter) {
            // Write nonce directly to header buffer (little-endian, offset 76)
            std::memcpy(header_buf.data() + 76, &nonce_counter, 4);

            // Compute hash using per-thread VM (LOCK-FREE, no allocations)
            uint256 pow_hash = mining_vm.Hash(header_buf);

            ++local_hashes;

            // Check if valid
            if (CheckProofOfWork(pow_hash, ctx->nBits, Params().GetConsensus())) {
                // Update block nonce for submission
                working_block.nNonce = nonce_counter;

                LogInfo("╔══════════════════════════════════════════════════════════════╗\n");
                LogInfo("║  🎉 BLOCK FOUND BY WORKER %d                                 ║\n", thread_id);
                LogInfo("╠══════════════════════════════════════════════════════════════╣\n");
                LogInfo("║  Height: %-53d ║\n", ctx->height);
                LogInfo("║  Nonce:  %-53u ║\n", nonce_counter);
                LogInfo("║  Hash:   %s... ║\n", pow_hash.ToString().substr(0, 16).c_str());
                LogInfo("╚══════════════════════════════════════════════════════════════╝\n");

                if (SubmitBlock(working_block)) {
                    m_blocks_found.fetch_add(1, std::memory_order_relaxed);
                } else {
                    m_stale_blocks.fetch_add(1, std::memory_order_relaxed);
                }

                // Flush hash count after block submission
                if (local_hashes > 0) {
                    m_hash_count.fetch_add(local_hashes, std::memory_order_relaxed);
                    local_hashes = 0;
                }

                // Force template refresh
                last_job_id = 0;
                break;
            }

            // Stride: add num_threads (natural uint32_t overflow handles wrap)
            nonce_counter += static_cast<uint32_t>(m_num_threads);

            // Check for new job every few iterations
            if (iter % 100 == 99) {
                if (m_job_id.load(std::memory_order_relaxed) != last_job_id) {
                    break;  // New template available
                }
            }
        }

        // Batch update hash count
        if (local_hashes >= HASH_BATCH_SIZE) {
            m_hash_count.fetch_add(local_hashes, std::memory_order_relaxed);
            local_hashes = 0;
        }
    }

    // Final hash count
    if (local_hashes > 0) {
        m_hash_count.fetch_add(local_hashes, std::memory_order_relaxed);
    }

    LogInfo("InternalMiner: Worker %d stopped\n", thread_id);
}

bool InternalMiner::SubmitBlock(const CBlock& block)
{
    // Note: ProcessNewBlock manages its own locking internally
    // Wrapping in cs_main causes contention and potential lock inversions

    bool new_block = false;
    auto block_ptr = std::make_shared<const CBlock>(block);
    bool accepted = m_chainman.ProcessNewBlock(block_ptr, /*force_processing=*/true,
                                                /*min_pow_checked=*/true, &new_block);

    if (accepted && new_block) {
        LogInfo("InternalMiner: Block accepted by network!\n");
        return true;
    } else if (accepted) {
        LogInfo("InternalMiner: Block was duplicate\n");
        return false;
    } else {
        LogInfo("InternalMiner: Block rejected (stale or invalid)\n");
        return false;
    }
}

} // namespace node
