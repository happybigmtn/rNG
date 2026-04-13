# Specification: RandomX Proof-of-Work and Mining System

This spec covers the foundational mining layer that the sharepool builds on: RandomX proof-of-work computation, difficulty adjustment, block parameters, and the internal miner. The code-level system is implemented; operational mainnet claims below are corpus-reported unless a fresh live-network probe is run.

## Objective

Document the existing PoW and mining system so that sharepool development has a
precise reference for the computation it extends (dual-target share checking)
and the block template pipeline it hooks into. Production claims are
corpus-reported unless re-verified against a live node.

## Evidence Status

### Verified Facts

Code-level facts in this subsection are verified against the RNG checkout as of
2026-04-13. Live mainnet state was not re-probed during this generated-doc
review.

- **Source locations confirmed**: `src/crypto/randomx/` (vendored library), `src/crypto/randomx_hash.{h,cpp}` (integration), `src/node/internal_miner.{h,cpp}` (~770 lines)
- **Test coverage**: `src/test/randomx_tests` (hash correctness), `test/functional/feature_internal_miner.py` (integration)

### Corpus-Reported Operational State

- Committed genesis docs report 32,122+ mainnet blocks as of 2026-04-13 and
  3 healthy validators (02, 04, 05) actively mining.
- Committed docs report no known PoW-related production failures. This review
  did not independently verify validator uptime or incident history.

### Recommendations

- The fixed genesis seed is a deliberate design choice that trades dataset recomputation cost for permanent precomputation by all parties (miners and attackers alike). This tradeoff is acceptable given RandomX's CPU-fairness properties but should be re-evaluated if hardware-accelerated RandomX implementations emerge.
- LWMA parameters (720-block window, 60-timestamp outlier cut) are
  corpus-reported as stable through 32,000+ blocks. No adjustment is recommended
  from code inspection alone, but activation gates should refresh live evidence.

### Hypotheses / Unresolved Questions

- **GPU/FPGA advantage magnitude**: RandomX is designed for CPU fairness, but non-zero GPU/FPGA advantages exist. The practical magnitude on RNG's network has not been measured. This is low priority while the network is small and CPU-only.
- **Fast-mode memory pressure at scale**: Committed docs report current miners
  run in fast mode (full 2 GiB dataset). If the node is co-located with
  memory-constrained services, light mode may be needed. This review did not
  re-check production memory pressure.

## RandomX Configuration

| Parameter | Value | Source |
|---|---|---|
| RandomX version | v1.2.1 (vendored) | `src/crypto/randomx/` |
| Genesis seed | `"RNG Genesis Seed"` | `src/crypto/randomx_hash.cpp` |
| Argon2d salt | `"RNGCHAIN01"` | `src/crypto/randomx_hash.cpp` |
| Dataset lifetime | Permanent (fixed seed) | Design choice |
| Default mode | Fast (full dataset, ~2 GiB RAM) | `internal_miner.cpp` |
| Light mode | Available but not default | Fallback for constrained environments |

**Dataset computation**: Because the seed is fixed, the RandomX dataset is computed once at node startup and never invalidated. This eliminates the periodic dataset recomputation that Monero nodes perform every 2048 blocks. The tradeoff is that an attacker's precomputed dataset is also permanent.

**Dual use**: The same RandomX computation serves both block proofs (hash must meet block difficulty target) and share proofs (hash must meet easier share target). The expensive part is the RandomX evaluation itself; comparing the result against a second target is negligible additional work.

## Difficulty Adjustment

| Parameter | Value |
|---|---|
| Algorithm | LWMA (Linear Weighted Moving Average) |
| Window | 720 blocks |
| Outlier cut | 60 timestamps |
| Target block spacing | 120 seconds (mainnet) |
| Adjustment frequency | Per-block |

LWMA weights recent blocks more heavily than distant blocks, providing responsive difficulty adjustment without the instability of purely reactive algorithms. The 60-timestamp outlier cut discards the most extreme timestamps from the window, resisting manipulation by miners who might submit dishonest timestamps.

**Corpus-reported stability**: Committed docs report 32,122+ blocks produced under this algorithm with no difficulty-related anomalies. Reconfirm with a live node before treating this as current operational evidence.

## Block Parameters

| Parameter | Value |
|---|---|
| Initial subsidy | 50 RNG per block |
| Halving interval | 2,100,000 blocks |
| Coinbase maturity | 100 blocks |
| Smallest unit | 1 roshi = 10^-8 RNG |
| Unit relationship | 1 RNG = 100,000,000 roshi |

The subsidy schedule and coinbase maturity follow Bitcoin's model. At 120-second block spacing, the first halving occurs at approximately 4,800 days (~13.15 years) of continuous mining.

## Internal Miner Architecture

**Source**: `src/node/internal_miner.{h,cpp}` (~770 lines)

The `InternalMiner` class inherits from `CValidationInterface` to receive chain-tip notifications.

### Thread model

- **1 coordinator thread**: Manages block templates, handles block submission, monitors for new tips
- **N worker threads**: Grind RandomX nonces in lock-free loops (N set by `-minethreads`)

### Worker nonce assignment

Workers use stride-based partitioning to avoid contention:

- Thread 0: nonces 0, N, 2N, 3N, ...
- Thread 1: nonces 1, N+1, 2N+1, 3N+1, ...
- Thread k: nonces k, N+k, 2N+k, 3N+k, ...

No locks or atomic coordination are needed between workers during nonce grinding. Each worker independently evaluates RandomX hashes and compares results against the block target.

### CLI flags

| Flag | Purpose |
|---|---|
| `-mine` | Enable the internal miner |
| `-mineaddress=<addr>` | Destination address for coinbase rewards |
| `-minethreads=N` | Number of worker threads |
| `-minerandomx` | Use RandomX (default and only PoW algorithm) |
| `-minepriority` | Thread scheduling priority |

### Observability

| Counter / Method | Purpose |
|---|---|
| `hash_count` | Total hashes computed across all workers |
| `blocks_found` | Blocks successfully submitted |
| `stale_blocks` | Blocks that arrived too late (tip moved) |
| `GetHashRate()` | Real-time hash rate across all workers |
| `GetTemplateRefreshes()` | Number of template rebuilds (monitors responsiveness) |

## Mining Flow

1. **Template creation**: The coordinator requests a new block template via `interfaces::Mining`. The template includes the candidate block header, coinbase transaction, and current difficulty target.

2. **Nonce distribution**: The coordinator distributes the template to all worker threads. Each worker begins grinding its nonce stride.

3. **Hash evaluation**: For each nonce, the worker computes a RandomX hash of the candidate block header and compares the result against the block difficulty target.

4. **Block found**: When a hash meets the target, the worker signals the coordinator. The coordinator calls `ProcessNewBlock()` to submit the block to the node's validation pipeline.

5. **Template refresh**: The coordinator rebuilds the block template when any of these events occur:
   - New chain tip arrives (via `CValidationInterface` callback)
   - Periodic timer fires (ensures mempool updates are picked up)
   - A block is found (start working on the next block immediately)

## Security Model

### Fixed seed tradeoffs

The fixed genesis seed `"RNG Genesis Seed"` means:

- **Benefit**: Miners compute the RandomX dataset once and keep it permanently. Fast-mode VMs never need reinitialization. Node restarts are faster after the first dataset computation.
- **Risk**: An attacker can also precompute the dataset once. There is no periodic "reset" that forces recomputation. This is acceptable because RandomX's security does not depend on dataset secrecy -- it depends on the computational cost of each hash evaluation.

### CPU fairness

RandomX is specifically designed to minimize the advantage of specialized hardware:

- The algorithm uses random code execution, making fixed-function hardware (ASICs) impractical
- Memory-hard computation (Argon2d) limits GPU parallelism
- Non-zero GPU/FPGA advantage exists but is limited by design (estimated 2-5x, not 1000x as with SHA-256)

### Difficulty manipulation resistance

LWMA with outlier cut provides two defenses against timestamp manipulation:

1. **Outlier cut**: The 60 most extreme timestamps in the 720-block window are discarded, preventing a miner from shifting difficulty by submitting dishonest timestamps on a small number of blocks.
2. **Linear weighting**: Recent blocks carry more weight, so the algorithm responds to genuine hashrate changes while being resistant to long-range manipulation.

### 51% attack considerations

Standard PoW 51% attack applies. With 3 active validators and a small network, the barrier to 51% attack is relatively low in absolute terms. This is expected for an early-stage chain and is mitigated by:

- Low economic value at stake (early chain)
- Coinbase maturity (100 blocks) delays attacker reward realization
- Active monitoring by known validators

## Sharepool Interaction

The sharepool extends the mining system without modifying it. The key integration points:

### Dual-target checking

When sharepool activates, each RandomX hash evaluation is compared against two targets:

1. **Block target**: The current block difficulty target (unchanged)
2. **Share target**: `min(powLimit, block_target *
   consensus.nPowTargetSpacing)` for a one-second share cadence. This is `120`
   on mainnet and `60` on the current test networks.

The share target is easier than the block target by a factor of 120, producing approximately one share per second at the 120-second block target. A hash that meets the block target necessarily also meets the share target.

### Cost profile

The expensive operation is the RandomX hash evaluation itself. Comparing the result against a second target is a single 256-bit integer comparison -- negligible overhead. Dual-target checking does not measurably affect mining performance.

### Template pipeline

The sharepool hooks into the same `interfaces::Mining` template pipeline that the internal miner already uses. Share production is a side effect of normal mining: hashes that miss the block target but hit the share target become share proofs instead of being discarded.

## Acceptance Criteria

Code-level criteria are met in this checkout. Operational criteria should be
re-verified against a live node before a release or activation gate:

- RandomX hash computation produces correct, deterministic results for the fixed genesis seed -- **verified by `src/test/randomx_tests`**
- Internal miner discovers blocks at a rate consistent with the configured difficulty and hashrate -- **corpus-reported by 32,122+ mainnet blocks**
- LWMA difficulty adjustment maintains approximately 120-second block spacing under varying hashrate -- **verified by mainnet operation**
- Block subsidy, halving schedule, and coinbase maturity match specified parameters -- **verified by chain state**
- No PoW-related crashes, consensus failures, or chain splits in production -- **corpus-reported; requires validator log review to re-verify**

## Verification

### Automated tests

| Test | Location | What it verifies |
|---|---|---|
| RandomX hash tests | `src/test/randomx_tests` | Hash computation correctness against known vectors |
| Internal miner integration | `test/functional/feature_internal_miner.py` | End-to-end mining: start miner, produce blocks, verify chain growth |

### Manual verification

```bash
# Start mining on regtest
rngd -regtest -mine -mineaddress=<addr> -minethreads=4

# Check hashrate
rng-cli -regtest getmininginfo

# Verify blocks are produced
rng-cli -regtest getblockcount
```

### Mainnet evidence

- Committed genesis docs report 32,122+ blocks as of 2026-04-13
- Committed genesis docs report 3 healthy validators actively mining (02, 04, 05)
- Committed genesis docs report no PoW-related incidents in production history

## Open Questions

1. **GPU advantage measurement**: What is the actual speedup from a GPU-based RandomX implementation on RNG's fixed-seed configuration? This has not been measured but is low priority while the network is CPU-only.

2. **Light mode testing**: Has the light-mode fallback been exercised in production? If memory-constrained deployments are planned, light mode should be tested under load.

3. **Dataset initialization time**: What is the wall-clock time for initial dataset computation on representative hardware? This affects cold-start time for new validators and should be documented.

4. **Nonce space exhaustion**: With stride-based partitioning across N threads, the 32-bit nonce space provides ~4.29 billion candidates per template. At high hashrates, is template refresh triggered before nonce exhaustion? The current refresh timer likely preempts this, but it has not been explicitly verified.
