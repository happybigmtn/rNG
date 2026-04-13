# Specification: Node Operations, Fleet Management, and Network Infrastructure

Date: 2026-04-13
Status: DRAFT
Source: Code review of chainparams.cpp, CMakeLists.txt, scripts/, Dockerfile, .github/workflows/, and committed genesis docs. This generated-doc review did not perform a fresh live fleet probe.

## Objective

Define the operational surface that keeps the RNG mainnet running: how binaries are built, how the network is discovered, how the validator fleet is managed, and where the gaps are. This spec captures verified facts, flags known risks, and proposes acceptance criteria for a production-grade deployment posture.

## Evidence Status

### Verified Facts

Items below are confirmed from source code as of 2026-04-13 unless explicitly
marked as corpus-reported.

- **Build system**: CMake 3.22+, C++20. Six shipping binaries plus test/bench.
- **Network magic**: 0xB07C010E, default port 8433, Bech32 HRP "rng".
- **Mainnet seed peers**: Four hardcoded IP literals in
  `src/kernel/chainparams.cpp`; no mainnet DNS seed names.
- **DNS seeds**: Absent on mainnet. Testnet/testnet4/signet have seed names,
  but mainnet peer discovery depends on hardcoded IPs plus addr gossip.
- **BIP324**: Encrypted transport available from genesis.
- **Scripts present**: `scripts/build-release.sh`,
  `scripts/check-reproducible-release.sh`, `scripts/doctor.sh`,
  `scripts/install-public-miner.sh`, `scripts/install-public-node.sh`,
  `scripts/load-bootstrap.sh`, `scripts/public-apply.sh`,
  `scripts/start-miner.sh`, `scripts/verify-release.sh`.
- **fPowAllowMinDifficultyBlocks**: true on mainnet. Effect with LWMA unresolved.

### Corpus-Reported Operational State

- **Seed peer hosting**: Committed genesis docs report all four hardcoded
  mainnet seed IPs are Contabo-hosted on a single ASN; this review did not
  perform ASN lookup.
- **Fleet health**: Committed genesis docs report 3 of 4 validators healthy at
  height ~32,122, with validator-01 crash-looping on a zero-byte settings file.
- **Bootstrap**: Committed genesis docs report a height-29944 bundle of ~60 MB
  and no versioning or refresh schedule.
- **Reproducible builds**: Same-machine linux-x86_64 verified. Cross-machine NOT verified.
- **QSB**: Committed genesis docs report operator-only QSB was proven on
  mainnet at heights 29946-29947.
- **RandomX**: Vendored v1.2.1, fixed genesis seed "RNG Genesis Seed", Argon salt "RNGCHAIN01".

### Recommendations

Items that follow from verified facts and represent actionable improvements.

1. **Add DNS seeds** or a second discovery mechanism to eliminate total dependence on hardcoded IPs.
2. **Diversify seed ASNs** -- all four seeds on a single provider is a single point of failure.
3. **Fix validator-01** -- zero-byte /root/.rng/settings.json causes crash-loop before RPC starts.
4. **Establish a bootstrap refresh schedule** with versioned bundles to prevent unbounded growth.
5. **Verify cross-machine reproducibility** for release builds.
6. **Resolve fPowAllowMinDifficultyBlocks=true** -- determine whether LWMA makes this safe or whether it should be disabled.

### Hypotheses / Unresolved Questions

Items that need investigation. Not confirmed, not contradicted.

- Whether the four Contabo nodes are in distinct datacenters or a single facility.
- Whether fPowAllowMinDifficultyBlocks=true permits minimum-difficulty blocks under the current LWMA algorithm, or whether LWMA's window makes the flag effectively inert.
- Whether BIP324 is enforced or merely available (i.e., do nodes accept v1 plaintext connections?).
- What the actual disk-growth rate is at current block heights and whether the 60 MB bootstrap will exceed practical download limits within 6 months.

## Build and Binaries

### Shipping Binaries

| Binary | Purpose |
|---|---|
| `rngd` | Full node daemon |
| `rng-cli` | Command-line RPC client |
| `rng-tx` | Raw transaction construction utility |
| `rng-util` | General-purpose utilities |
| `rng-wallet` | Offline wallet tool |
| `rng-qt` | Optional Qt GUI (requires Qt build deps) |

### Test Binaries

| Binary | Purpose |
|---|---|
| `test_bitcoin` | Unit/integration test runner |
| `bench_bitcoin` | Benchmark suite |

These retain Bitcoin-derived names intentionally. Renaming would increase divergence from upstream and complicate future merges.

### Build Commands

```bash
cmake -B build
cmake --build build -j$(nproc)
```

Requirements: C++20 compiler, CMake 3.22+.

### RandomX Integration

- Vendored source: `src/crypto/randomx/` (RandomX v1.2.1)
- Interface: `src/crypto/randomx_hash.h`, `src/crypto/randomx_hash.cpp`
- Genesis seed: `"RNG Genesis Seed"` (fixed, hardcoded)
- Argon salt: `"RNGCHAIN01"`

## Network Identity

| Parameter | Value |
|---|---|
| Mainnet magic bytes | `0xB07C010E` |
| Default P2P port | `8433` |
| Bech32 HRP | `"rng"` |
| Encrypted transport | BIP324 available from genesis |

### Peer Discovery

**Hardcoded seed IPs**: Four mainnet IP literals. Committed genesis docs report
they are all Contabo-hosted on a single ASN; re-run ASN lookup before treating
that as fresh operational evidence.

**DNS seeds**: Not implemented. Old documentation references DNS seeds but chainparams.cpp contains no DNS seed entries. Peer discovery depends entirely on the four hardcoded IPs plus addr gossip from connected peers.

**Risk**: If all four seed IPs become unreachable simultaneously (provider outage, IP reassignment, account suspension), new nodes cannot bootstrap peer discovery. Existing nodes that already have a populated addr database would continue to connect via gossip, but cold-start nodes would be stranded.

## Validator Fleet

### Corpus-Reported Current State (2026-04-13, height ~32,122)

| Node | Status | Notes |
|---|---|---|
| validator-01 | DOWN | Zero-byte `/root/.rng/settings.json`. `rngd.service` crash-loops before RPC initializes. |
| validator-02 | HEALTHY | Synced at height 32,122 |
| validator-04 | HEALTHY | Synced at height 32,122 |
| validator-05 | HEALTHY | Synced at height 32,122 |

Three of four validators healthy. This is a thin margin -- losing one more reduces the fleet to 50% of original capacity.

### validator-01 Recovery

Root cause: zero-byte `/root/.rng/settings.json`. The daemon attempts to parse this file on startup, fails, and exits before the RPC interface is available. Fix: either delete the file (rngd regenerates defaults) or write valid JSON (`{}`). Then restart `rngd.service`.

## Operator Tooling

### scripts/ Directory

| Script | Purpose |
|---|---|
| `start-miner.sh` | Start mining with specified address and thread count |
| `doctor.sh` | Health check: chain sync status, peer count, mining state |
| `install-public-miner.sh` | Public miner installation helper |
| `install-public-node.sh` | Public node installation helper |
| `load-bootstrap.sh` | Load the corpus-reported height-29944 bootstrap bundle (~60 MB) |
| `public-apply.sh` | Apply public-node configuration |
| `build-release.sh` | Produce reproducible release build |
| `check-reproducible-release.sh` | Verify build reproducibility |
| `verify-release.sh` | Verify release artifacts |

### QSB Operator RPCs

Enabled with `-enableqsboperator` flag. Not exposed to public relay.

| RPC | Purpose |
|---|---|
| `submitqsbtransaction` | Submit a QSB transaction |
| `listqsbtransactions` | List pending QSB transactions |
| `removeqsbtransaction` | Remove a QSB transaction from the pool |

Source files: `src/script/qsb.{h,cpp}`, `src/node/qsb_pool.{h,cpp}`, `src/node/qsb_validation.{h,cpp}`.

Committed genesis docs report this was proven on mainnet at heights 29946-29947.

### Docker

Dockerfile present. Requires `RNG_RPC_PASSWORD` environment variable -- fails closed if unset.

## Bootstrap and Distribution

### Current Bootstrap

- Snapshot height: 29,944
- Size: ~60 MB
- Loaded via: `scripts/load-bootstrap.sh`

### Gaps

- **No versioning**: No mechanism to distinguish bootstrap versions or validate integrity beyond the script itself.
- **No CDN**: Distribution method is not specified in the codebase.
- **No refresh schedule**: The bootstrap will grow unbounded as the chain grows. Without periodic refresh, new nodes must sync increasing amounts of chain data after loading the bundle.
- **No pruned-node bootstrap**: No guidance or tooling for pruned-node operation.

## CI and Release Pipeline

### CI

- Workflow: `.github/workflows/ci.yml`
- Platforms: Linux, macOS, Windows
- Runs on: push and pull request

### Release

- Separate release workflow for cross-platform builds.
- `scripts/build-release.sh` produces the build.
- `scripts/check-reproducible-release.sh` verifies reproducibility.

### Reproducibility Status

| Scope | Status |
|---|---|
| Same machine, linux-x86_64 | Verified |
| Cross-machine, linux-x86_64 | NOT verified |
| macOS | NOT verified |
| Windows | NOT verified |

Cross-machine and cross-platform reproducibility remain unproven. Users cannot independently verify that a release binary matches the source without building on an identical machine.

## Security and Resilience

### Single-ASN Risk

Committed genesis docs report all four seed peers and all four validators are
hosted on Contabo infrastructure. If still current, a single provider action
(maintenance, outage, account suspension, IP block reassignment) could take down
all seeds and all validators simultaneously. Re-run ASN/fleet verification before
using this as current operational evidence.

**Mitigation**: Add seed peers on at least two additional autonomous systems. Prioritize geographic and provider diversity.

### fPowAllowMinDifficultyBlocks on Mainnet

This flag is set to `true` in mainnet chainparams. On Bitcoin, this flag allows blocks at minimum difficulty after a timeout, and is only enabled on testnet/signet. On RNG mainnet with LWMA difficulty adjustment, the interaction is unresolved.

**Risk**: If the flag permits minimum-difficulty blocks under LWMA, an attacker with modest hashrate could produce long chains of trivial blocks, potentially enabling difficulty manipulation or chain reorganization.

**Required action**: Audit the LWMA integration to determine whether this flag has any effect, and if so, disable it.

### Limited Fleet Redundancy

Committed genesis docs report 3 of 4 validators healthy, and code inspection
shows no mainnet DNS seeds. If the fleet state is still current, a second
validator failure reduces the healthy fleet to 50%.

### No Root install.sh

No system-level install script. Operators must manually build, configure, and deploy. This increases setup variance and error probability across nodes.

## Acceptance Criteria

The following criteria define a production-grade operational posture. Items marked [MET] are satisfied today; items marked [NOT MET] require work.

- [REPORTED] Mainnet is live and producing blocks.
- [REPORTED] At least 3 validators are synced and healthy.
- [MET] Operator health-check script (`doctor.sh`) exists and covers sync, peers, and mining.
- [MET] Docker deployment fails closed on missing credentials.
- [MET] CI covers Linux, macOS, and Windows.
- [REPORTED] QSB operator feature is proven on mainnet.
- [NOT MET] DNS seeds or a second peer-discovery mechanism exist.
- [NOT MET] Seed peers span at least two autonomous systems.
- [NOT MET] All validators are healthy (validator-01 is down).
- [NOT MET] Bootstrap has versioning, integrity checks, and a refresh schedule.
- [NOT MET] Cross-machine reproducible builds are verified.
- [NOT MET] fPowAllowMinDifficultyBlocks interaction with LWMA is audited and resolved.
- [NOT MET] An install or deployment script exists for cold-start node setup.

## Verification

To verify claims in this spec against the running network and codebase:

```bash
# Build from source
cmake -B build && cmake --build build -j$(nproc)

# Check network identity in chainparams
grep -n "pchMessageStart\|nDefaultPort\|bech32_hrp" src/kernel/chainparams.cpp

# Check for DNS seeds
grep -n "vSeeds" src/kernel/chainparams.cpp

# Check seed IPs
grep -n "vSeeds.emplace_back" src/kernel/chainparams.cpp

# Check fPowAllowMinDifficultyBlocks
grep -n "fPowAllowMinDifficultyBlocks" src/kernel/chainparams.cpp

# Run health check on a validator
ssh validator-02 'bash /path/to/scripts/doctor.sh'

# Verify bootstrap exists and check size
ls -lh bootstrap/  # or wherever the bundle is hosted

# Check QSB source files exist
ls src/script/qsb.{h,cpp} src/node/qsb_pool.{h,cpp} src/node/qsb_validation.{h,cpp}

# Check RandomX integration
ls src/crypto/randomx/ src/crypto/randomx_hash.{h,cpp}
```

## Open Questions

1. **fPowAllowMinDifficultyBlocks + LWMA**: Does the flag have any effect under the LWMA difficulty adjustment algorithm? If so, what are the security implications and should it be set to false?
2. **Contabo datacenter diversity**: Are the four Contabo VPS instances in different datacenters, or could a single facility event take all of them offline?
3. **BIP324 enforcement**: Is encrypted transport required, or do nodes also accept v1 plaintext connections?
4. **Bootstrap growth rate**: At current block production rate and average block size, when will the bootstrap bundle exceed practical limits (e.g., 500 MB, 1 GB)?
5. **validator-01 root cause**: Is the zero-byte settings.json a symptom of a deeper issue (disk failure, OOM kill during write), or a one-time corruption?
6. **Cross-machine reproducibility**: What are the blockers? Compiler version, system library differences, or non-deterministic build steps?
7. **DNS seed operation**: Who would run DNS seed infrastructure, and what software (e.g., bitcoin-seeder fork)?
