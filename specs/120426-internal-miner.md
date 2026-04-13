# Specification: Internal Miner Architecture

## Objective

Define the built-in multithreaded RandomX miner integrated into `rngd`. Covers the coordinator/worker threading model, template management, nonce distribution, configuration flags, statistics reporting, and operational behavior.

## Evidence Status

### Verified Facts (grounded in source code)

- **Source files**: `src/node/internal_miner.h` and `src/node/internal_miner.cpp` (~530 lines)
- **Architecture**: One coordinator thread + N worker threads
  - Coordinator: creates block templates, monitors chain tip via `ValidationSignals`, swaps templates atomically
  - Workers: pure nonce grinding with no locks on the hot path
- **Nonce distribution**: Stride-based — thread `i` of `N` tries nonces `i, i+N, i+2N, ...` (no range partitioning)
- **Template sharing**: Atomic pointer swap; workers read the current template without locking
- **Template refresh**: Every 30 seconds, or immediately on new chain tip (event-driven via `ValidationSignals`)
- **Backoff**: Exponential backoff on repeated failures, max 6 levels (64 seconds max delay)
- **Minimum peers**: Mining requires at least 1 connected peer
- **Configuration flags**:
  - `-mine`: Enable internal miner (OFF by default)
  - `-mineaddress=<addr>`: Required payout address (no default — must be explicit)
  - `-minethreads=<n>`: Number of worker threads (logged prominently at startup)
  - `-minerandomx=fast|light`: RandomX mode selection
  - `-minepriority=low|normal`: CPU priority (`low` maps to `nice 19`)
- **Safety guarantees**:
  - Mining is OFF by default — explicit opt-in required
  - No default payout address — prevents accidental mining to uncontrolled address
  - Thread count is logged loudly at startup
  - Clean shutdown with proper thread join ordering
- **Statistics** (thread-safe via atomics):
  - `GetHashRate()`: Current hashrate in H/s
  - `GetBlocksFound()`: Total blocks mined this session
  - `GetHashCount()`: Total hashes computed
  - `IsRunning()`: Whether mining is active
  - `GetTemplateCount()`: Number of template refreshes
- **RPC endpoint**: `getinternalmininginfo` exposes miner statistics (added RPC, not present in upstream Bitcoin Core)
- **Public API** (from `internal_miner.h`):
  - `Start(num_threads, coinbase_script, fast_mode, low_priority)`
  - `Stop()` — graceful shutdown
  - Individual getters for stats listed above

### Recommendations (intended system)

- Plan 008 proposes extending the internal miner to produce share records alongside block attempts (dual-target: share target and block target per hash)
- Plan 008 proposes tracking `m_shares_found` counter in addition to `m_blocks_found`
- Share production would construct a `ShareRecord`, submit to sharechain store, and relay to peers — none of this code exists yet

### Hypotheses / Unresolved Questions

- The exact template refresh interval (30 seconds) may need tuning for pooled mining (shares need fresher templates than blocks)
- Whether the stride-based nonce strategy has any statistical bias compared to range partitioning (likely negligible for RandomX)

## Acceptance Criteria

- `rngd -mine -mineaddress=<addr> -minethreads=N` starts N worker threads and one coordinator thread
- Without `-mine`, no mining threads are started
- Without `-mineaddress`, mining refuses to start (error logged)
- Workers operate without mutex contention on the hot path (nonce grinding is lock-free)
- New chain tip triggers immediate template refresh (not waiting for 30-second interval)
- `getinternalmininginfo` returns current hashrate, blocks found, hash count, running status, and template count
- Mining stops cleanly on `rng-cli stop` — all threads join in order, no orphan threads
- With `-minepriority=low`, worker threads run at nice 19
- With `-minerandomx=fast`, the 2 GiB RandomX dataset is allocated per the fast-mode specification
- With `-minerandomx=light`, the 256 MiB cache is used instead
- Mining does not proceed with 0 connected peers

## Verification

```bash
# Start mining with explicit configuration
rngd -daemon -mine -mineaddress=rng1q... -minethreads=4 -minerandomx=fast -minepriority=low

# Verify mining is active
rng-cli getinternalmininginfo
# Expected: { "running": true, "threads": 4, "hashrate": <non-zero>, ... }

# Verify hashrate is reasonable (~500-700 H/s per core for fast mode)
rng-cli getinternalmininginfo | jq '.hashrate'

# Verify template refresh on new block
# (observe template_count incrementing when a new block arrives)
rng-cli getinternalmininginfo | jq '.template_count'

# Verify clean shutdown
rng-cli stop
# No zombie threads should remain
ps aux | grep rngd

# Verify mining refuses without -mineaddress
rngd -daemon -mine -minethreads=4
# Should log error about missing mineaddress
```

## Open Questions

1. Should the miner expose per-thread hashrate statistics, or is aggregate sufficient?
2. Is the 30-second template refresh interval documented or hardcoded? If hardcoded, should it be configurable?
3. What is the miner's behavior when the node is in initial block download (IBD)? Does it wait for sync to complete before mining?
4. Should `getinternalmininginfo` be renamed to match RNG branding (e.g., `getmininginfo` extension) or kept separate?
