// Copyright (c) 2024-present The RNG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/randomx_hash.h>

#include <logging.h>
#include <util/check.h>

#include <cstring>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif
#include <randomx/src/randomx.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// ============================================================================
// RandomXContext implementation
// ============================================================================

RandomXContext::RandomXContext() = default;

RandomXContext::~RandomXContext() {
    Cleanup();
}

RandomXContext& RandomXContext::GetInstance() {
    static RandomXContext instance;
    return instance;
}

void RandomXContext::Cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_vm_light) {
        randomx_destroy_vm(m_vm_light);
        m_vm_light = nullptr;
    }
    if (m_vm_fast) {
        randomx_destroy_vm(m_vm_fast);
        m_vm_fast = nullptr;
    }
    if (m_dataset) {
        randomx_release_dataset(m_dataset);
        m_dataset = nullptr;
    }
    if (m_cache) {
        randomx_release_cache(m_cache);
        m_cache = nullptr;
    }
    m_current_seed_hash = std::nullopt;
    m_fast_mode_initialized = false;
}

void RandomXContext::InitLight(const uint256& seed_hash) {
    // Get recommended flags for this CPU
    randomx_flags flags = randomx_get_flags();

    // Create cache if not exists
    if (!m_cache) {
        m_cache = randomx_alloc_cache(flags | RANDOMX_FLAG_JIT);
        if (!m_cache) {
            // Fallback without JIT
            m_cache = randomx_alloc_cache(flags);
        }
        if (!m_cache) {
            throw std::runtime_error("RandomX: Failed to allocate cache");
        }
    }

    // Initialize cache with seed hash
    randomx_init_cache(m_cache, seed_hash.data(), 32);

    // Create or reinitialize light VM
    if (m_vm_light) {
        randomx_vm_set_cache(m_vm_light, m_cache);
    } else {
        m_vm_light = randomx_create_vm(flags | RANDOMX_FLAG_JIT, m_cache, nullptr);
        if (!m_vm_light) {
            // Fallback without JIT
            m_vm_light = randomx_create_vm(flags, m_cache, nullptr);
        }
        if (!m_vm_light) {
            throw std::runtime_error("RandomX: Failed to create light VM");
        }
    }

    m_current_seed_hash = seed_hash;
    LogDebug(BCLog::VALIDATION, "RandomX light mode initialized with seed %s\n",
             seed_hash.GetHex());
}

void RandomXContext::InitFast(const uint256& seed_hash) {
    // First ensure light mode is initialized (we need the cache)
    if (!m_cache || m_current_seed_hash != seed_hash) {
        InitLight(seed_hash);
    }

    randomx_flags flags = randomx_get_flags();

    // Allocate dataset if not exists
    if (!m_dataset) {
        m_dataset = randomx_alloc_dataset(flags);
        if (!m_dataset) {
            throw std::runtime_error("RandomX: Failed to allocate dataset (need ~2 GiB RAM)");
        }
    }

    // Initialize dataset from cache
    // This is computationally expensive (~1-2 minutes on modern CPU)
    unsigned long item_count = randomx_dataset_item_count();
    LogDebug(BCLog::VALIDATION, "RandomX initializing dataset with %lu items...\n", item_count);

    // Initialize all items (can be parallelized but keeping simple for now)
    randomx_init_dataset(m_dataset, m_cache, 0, item_count);

    // Create or reinitialize fast VM
    if (m_vm_fast) {
        randomx_vm_set_dataset(m_vm_fast, m_dataset);
    } else {
        m_vm_fast = randomx_create_vm(flags | RANDOMX_FLAG_JIT | RANDOMX_FLAG_FULL_MEM,
                                       nullptr, m_dataset);
        if (!m_vm_fast) {
            // Fallback without JIT
            m_vm_fast = randomx_create_vm(flags | RANDOMX_FLAG_FULL_MEM,
                                           nullptr, m_dataset);
        }
        if (!m_vm_fast) {
            throw std::runtime_error("RandomX: Failed to create fast VM");
        }
    }

    m_fast_mode_initialized = true;
    LogDebug(BCLog::VALIDATION, "RandomX fast mode initialized with seed %s\n",
             seed_hash.GetHex());
}

void RandomXContext::UpdateSeedHash(const uint256& seed_hash, bool fast_mode) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if already initialized with this seed
    if (m_current_seed_hash && *m_current_seed_hash == seed_hash) {
        if (!fast_mode || m_fast_mode_initialized) {
            return; // Already initialized
        }
    }

    if (fast_mode) {
        InitFast(seed_hash);
    } else {
        InitLight(seed_hash);
    }
}

bool RandomXContext::IsInitialized() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_vm_light != nullptr;
}

std::optional<uint256> RandomXContext::GetCurrentSeedHash() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_current_seed_hash;
}

randomx_dataset* RandomXContext::GetDataset() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dataset;  // May be nullptr if fast mode not initialized
}

randomx_cache* RandomXContext::GetCache() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cache;
}

uint256 RandomXContext::Hash(std::span<const unsigned char> input, const uint256& seed_hash) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Ensure we're initialized with the correct seed
    if (!m_current_seed_hash || *m_current_seed_hash != seed_hash) {
        InitLight(seed_hash);
    }

    Assert(m_vm_light);

    uint256 result;
    randomx_calculate_hash(m_vm_light, input.data(), input.size(), result.data());
    return result;
}

uint256 RandomXContext::HashFast(std::span<const unsigned char> input, const uint256& seed_hash) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Ensure fast mode is initialized with the correct seed
    if (!m_fast_mode_initialized || !m_current_seed_hash || *m_current_seed_hash != seed_hash) {
        InitFast(seed_hash);
    }

    Assert(m_vm_fast);

    uint256 result;
    randomx_calculate_hash(m_vm_fast, input.data(), input.size(), result.data());
    return result;
}

// ============================================================================
// RandomXMiningVM implementation - Per-thread mining VMs
// ============================================================================

RandomXMiningVM::RandomXMiningVM() = default;

RandomXMiningVM::~RandomXMiningVM() {
    if (m_vm) {
        randomx_destroy_vm(m_vm);
        m_vm = nullptr;
    }
}

RandomXMiningVM::RandomXMiningVM(RandomXMiningVM&& other) noexcept
    : m_vm(other.m_vm), m_seed_hash(other.m_seed_hash), m_initialized(other.m_initialized), m_fast_mode(other.m_fast_mode) {
    other.m_vm = nullptr;
    other.m_initialized = false;
    other.m_fast_mode = false;
}

RandomXMiningVM& RandomXMiningVM::operator=(RandomXMiningVM&& other) noexcept {
    if (this != &other) {
        if (m_vm) {
            randomx_destroy_vm(m_vm);
        }
        m_vm = other.m_vm;
        m_seed_hash = other.m_seed_hash;
        m_initialized = other.m_initialized;
        m_fast_mode = other.m_fast_mode;
        other.m_vm = nullptr;
        other.m_initialized = false;
        other.m_fast_mode = false;
    }
    return *this;
}

bool RandomXMiningVM::Initialize(const uint256& seed_hash, bool fast_mode, bool* fast_mode_used) {
    auto try_initialize = [&](bool use_fast_mode) -> bool {
        try {
            // Ensure the global context is initialized with this seed.
            RandomXContext::GetInstance().UpdateSeedHash(seed_hash, /*fast_mode=*/use_fast_mode);
        } catch (const std::exception& e) {
            LogWarning("RandomXMiningVM: %s mode initialization failed: %s\n",
                       use_fast_mode ? "fast" : "light", e.what());
            return false;
        }

        if (m_vm && (m_seed_hash != seed_hash || m_fast_mode != use_fast_mode)) {
            randomx_destroy_vm(m_vm);
            m_vm = nullptr;
        }

        if (!m_vm) {
            randomx_flags flags = randomx_get_flags();

            if (use_fast_mode) {
                randomx_dataset* dataset = RandomXContext::GetInstance().GetDataset();
                if (!dataset) {
                    LogWarning("RandomXMiningVM: Dataset not available (fast mode)\n");
                    return false;
                }

                m_vm = randomx_create_vm(flags | RANDOMX_FLAG_JIT | RANDOMX_FLAG_FULL_MEM,
                                         nullptr, dataset);
                if (!m_vm) {
                    m_vm = randomx_create_vm(flags | RANDOMX_FLAG_FULL_MEM, nullptr, dataset);
                }
            } else {
                randomx_cache* cache = RandomXContext::GetInstance().GetCache();
                if (!cache) {
                    LogWarning("RandomXMiningVM: Cache not available (light mode)\n");
                    return false;
                }

                m_vm = randomx_create_vm(flags | RANDOMX_FLAG_JIT, cache, nullptr);
                if (!m_vm) {
                    m_vm = randomx_create_vm(flags, cache, nullptr);
                }
            }

            if (!m_vm) {
                LogWarning("RandomXMiningVM: Failed to create %s VM\n", use_fast_mode ? "fast" : "light");
                return false;
            }
        }

        m_seed_hash = seed_hash;
        m_initialized = true;
        m_fast_mode = use_fast_mode;
        if (fast_mode_used) {
            *fast_mode_used = use_fast_mode;
        }
        return true;
    };

    if (try_initialize(fast_mode)) {
        return true;
    }

    if (fast_mode) {
        LogWarning("RandomXMiningVM: Falling back to light mode after fast mode failure\n");
        return try_initialize(/*use_fast_mode=*/false);
    }

    return false;
}

uint256 RandomXMiningVM::Hash(std::span<const unsigned char> input) {
    Assert(m_vm && m_initialized);

    uint256 result;
    randomx_calculate_hash(m_vm, input.data(), input.size(), result.data());
    return result;
}

bool RandomXMiningVM::HasSeed(const uint256& seed_hash) const {
    return m_initialized && m_seed_hash == seed_hash;
}

// ============================================================================
// Public API functions
// ============================================================================

uint256 RandomXHash(std::span<const unsigned char> header_data, const uint256& seed_hash) {
    return RandomXContext::GetInstance().Hash(header_data, seed_hash);
}

uint256 RandomXHashLight(std::span<const unsigned char> data, const uint256& seed_hash) {
    return RandomXContext::GetInstance().Hash(data, seed_hash);
}

uint64_t GetRandomXSeedHeight(uint64_t block_height) {
    // RNG uses a fixed genesis seed for all blocks.
    // This eliminates permanent fork divergence that occurs when nodes
    // on different forks have different block hashes at epoch boundaries.
    // Any node can verify any block regardless of chain history.
    //
    // Trade-off: theoretically less ASIC-resistant than rotating seeds,
    // but irrelevant for RNG's network size. Stability > theory.
    (void)block_height;
    return 0;
}
