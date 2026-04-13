# IMPLEMENTATION_PLAN

Generated: 2026-04-12
Branch at time of planning: `feat/bitcoin-30-qsb` (HEAD `f39bc788f4`)
Main: `8e33f25b30` (ahead of current branch; includes Bitcoin Core v30.2 port with QSB)

---

## Priority Work

### Tier 3: Sharepool Core Implementation

- [ ] `POOL-04` Add `DEPLOYMENT_SHAREPOOL` BIP9 activation boundary

  Spec: `specs/120426-sharepool-protocol.md`
  Why now: Corpus Plan 004. The activation boundary must exist before any consensus-changing code so that all new rules are gated behind `DeploymentActiveAt()`. Ships as `NEVER_ACTIVE` on mainnet — no risk to live network.
  Codebase evidence: `src/consensus/params.h:35-36` — BIP9 enum has `DEPLOYMENT_TESTDUMMY` and `DEPLOYMENT_TAPROOT`. New entry goes after `DEPLOYMENT_TAPROOT`. `MAX_VERSION_BITS_DEPLOYMENTS` needs incrementing. `src/kernel/chainparams.cpp` needs deployment parameters for all network types.
  Owns: `src/consensus/params.h` (new enum entry), `src/kernel/chainparams.cpp` (deployment params per network), `src/deploymentinfo.cpp` (name string).
  Integration touchpoints: `src/versionbits.cpp` (automatic via BIP9 framework), `-vbparams` regtest override.
  Scope boundary: Activation wiring only. No consensus rule changes. No share code. Mainnet: `NEVER_ACTIVE`. Regtest: activatable via `-vbparams=sharepool:0:9999999999:0`.
  Acceptance criteria: `DEPLOYMENT_SHAREPOOL` compiles. `DeploymentActiveAt(tip, DEPLOYMENT_SHAREPOOL)` returns false on mainnet. Regtest with `-vbparams=sharepool:0:9999999999:0` activates after signaling window.
  Verification: `cmake --build build -j$(nproc) && build/bin/test_bitcoin --run_test=versionbits_tests`
  Required tests: Existing `versionbits_tests` pass with new deployment. Add one test confirming `DEPLOYMENT_SHAREPOOL` is `NEVER_ACTIVE` by default.
  Dependencies: `CHKPT-02`.
  Estimated scope: S
  Completion signal: Build succeeds; versionbits tests pass; regtest activation works via `-vbparams`.

- [ ] `POOL-05` Implement sharechain data model, storage, and P2P relay

  Spec: `specs/120426-sharepool-protocol.md`
  Why now: Corpus Plan 005. The sharechain is the core data structure — shares must be stored, linked, and relayed before reward windows or payout commitments can be computed. This is the largest single implementation unit.
  Codebase evidence: No share-related code exists in `src/`. `src/node/internal_miner.h` (the miner that will produce shares) is 240 lines. `src/protocol.h` defines existing P2P message types — new `shareinv`/`getshare`/`share` messages follow the same pattern.
  Owns: New files: `src/node/sharechain.{h,cpp}` (ShareRecord struct, sharechain store, tip selection, orphan buffer), `src/net_processing.cpp` additions (3 new message handlers), `src/protocol.h` additions (3 new message type constants).
  Integration touchpoints: `src/node/internal_miner.cpp` (future share production in POOL-07), `src/validation.cpp` (future commitment validation in POOL-06), LevelDB storage layer.
  Scope boundary: Data model + storage + relay. No payout computation. No commitment generation. No miner integration. Orphan buffer max 64 shares (per spec). Storage in LevelDB under `sharechain/` prefix.
  Acceptance criteria: (1) `ShareRecord` serializes/deserializes deterministically. (2) Sharechain maintains ordered chain linked by parent refs. (3) Orphans buffered up to 64, resolved when parent arrives. (4) `shareinv`/`getshare`/`share` messages relay between connected regtest nodes. (5) Invalid shares (bad PoW) rejected at relay. (6) Shares gated behind `DeploymentActiveAt(DEPLOYMENT_SHAREPOOL)`.
  Verification: `cmake --build build -j$(nproc) && build/bin/test_bitcoin --run_test=sharechain_tests && python3 test/functional/feature_sharepool_relay.py`
  Required tests: C++ unit tests for serialization, chain insertion, orphan handling, tip selection. Functional test for P2P relay between 2 regtest nodes.
  Dependencies: `POOL-04` (activation boundary must exist for gating).
  Estimated scope: L
  Completion signal: All sharechain unit and functional tests pass; relay measurable between regtest nodes.

- [ ] `POOL-06-GATE` Decision gate: share relay viability

  Spec: `specs/120426-sharepool-protocol.md`
  Why now: Corpus Plan 006. Before building the payout/claim layer, verify that share relay actually works at acceptable bandwidth and latency on a small regtest network that mimics RNG's real topology.
  Codebase evidence: No relay measurements exist.
  Owns: Measurement report documenting: relay bandwidth per node, relay latency (p50/p99), orphan rate, share propagation completeness.
  Integration touchpoints: POOL-05 relay code.
  Scope boundary: Measurement only. Fix relay bugs if found, but do not redesign the protocol.
  Acceptance criteria: (1) Bandwidth < 10 KB/s per node at target share rate. (2) Relay latency < 5s p50, < 10s p99. (3) Orphan rate < 20%. If any threshold breached, document specific fix needed.
  Verification: Regtest network of 4-6 nodes mining at ~10s share interval for 30 minutes; measure with `getpeerinfo` bandwidth counters and share arrival timestamps.
  Required tests: None (measurement gate).
  Dependencies: `POOL-05`.
  Estimated scope: S
  Completion signal: Measurement report committed with all thresholds met, or documented revision plan.

- [ ] `POOL-07` Implement payout commitment and claim program

  Spec: `specs/120426-sharepool-protocol.md`
  Why now: Corpus Plan 007. The payout commitment (Merkle root in coinbase) and claim program (witness v2 spend path) are the consensus-enforcing layer. Without them, shares have no economic meaning.
  Codebase evidence: `src/node/miner.cpp` (`CreateNewBlock()`) is where commitment would be inserted into coinbase. `src/script/interpreter.cpp` handles witness version dispatch — v2 is currently an anyone-can-spend pass-through.
  Owns: New files: `src/consensus/sharepool.{h,cpp}` (reward window computation, Merkle commitment construction), modifications to `src/node/miner.cpp` (insert commitment in coinbase when active), modifications to `src/script/interpreter.cpp` (witness v2 claim verification), modifications to `src/validation.cpp` (verify commitment in `ConnectBlock` when active).
  Integration touchpoints: `src/consensus/params.h` (references constants from spec), `src/wallet/` (future claim scanning in POOL-08).
  Scope boundary: Commitment generation and claim verification. Does not extend the internal miner to produce shares (that's POOL-08). Does not add wallet scanning (that's POOL-08). All new rules gated behind `DeploymentActiveAt(DEPLOYMENT_SHAREPOOL)`.
  Acceptance criteria: (1) `CreateNewBlock()` includes correct Merkle commitment root when sharepool active. (2) Blocks without valid commitment rejected when sharepool active. (3) Claim transactions with valid Merkle proof + signature accepted after 100-block maturity. (4) Invalid claims rejected. (5) Solo miner (only contributor) produces single-leaf commitment. (6) Pre-activation nodes treat witness v2 outputs as valid (soft-fork compatible).
  Verification: `build/bin/test_bitcoin --run_test=sharepool_commitment_tests && python3 test/functional/feature_sharepool_commitment.py`
  Required tests: Unit tests for Merkle construction, commitment validation, claim verification. Functional test for end-to-end: activate on regtest, mine block with commitment, claim after maturity.
  Dependencies: `POOL-05` (sharechain provides shares for reward window), `POOL-06-GATE` (relay must be viable).
  Estimated scope: L
  Completion signal: Commitment and claim tests pass; solo mining with commitment works on regtest.

- [ ] `POOL-08` Extend internal miner and wallet for sharepool integration

  Spec: `specs/120426-internal-miner.md`, `specs/120426-wallet-rpc-surface.md`
  Why now: Corpus Plan 008. The miner must produce shares alongside block attempts, and the wallet must track pooled rewards. This is the user-facing integration layer.
  Codebase evidence: `src/node/internal_miner.h:230-234` defines constants including `HASH_BATCH_SIZE = 10000` and `STALENESS_CHECK_INTERVAL = 1000`. Workers check one target (block difficulty) per hash. Extension adds second target (share difficulty). `src/rpc/mining.cpp:519` defines `getinternalmininginfo`. `src/wallet/rpc/coins.cpp` defines `getbalances`.
  Owns: Modifications to `src/node/internal_miner.{h,cpp}` (dual-target: share + block), new `m_shares_found` counter, share construction and relay on share-meeting hash. Modifications to `src/wallet/` (scan coinbase for v2 commitment, record `PooledRewardEntry`, auto-claim on maturity). New/extended RPCs: `submitshare`, `getsharechaininfo`, `getrewardcommitment`. Extended `getmininginfo` with sharepool fields. Extended `getbalances` with `pooled.pending/claimable`.
  Integration touchpoints: `src/node/sharechain.{h,cpp}` (from POOL-05), `src/consensus/sharepool.{h,cpp}` (from POOL-07), `src/rpc/mining.cpp`, `src/wallet/rpc/coins.cpp`.
  Scope boundary: Miner + wallet + RPC integration for sharepool. Does not change the consensus rules (those are POOL-07). All features gated behind `DeploymentActiveAt(DEPLOYMENT_SHAREPOOL)`.
  Acceptance criteria: (1) Miner produces shares at share target rate when sharepool active. (2) `getinternalmininginfo` includes share count. (3) `getbalances` includes `pooled.pending` and `pooled.claimable`. (4) Wallet auto-claims on maturity. (5) `submitshare` validates and relays. (6) `getsharechaininfo` returns tip/height/orphans/window.
  Verification: `python3 test/functional/feature_sharepool_miner.py && python3 test/functional/feature_sharepool_wallet.py`
  Required tests: Functional test: 2-node regtest, both mining with sharepool active, verify proportional rewards accrue and claims succeed.
  Dependencies: `POOL-07` (commitment/claim must work before miner/wallet can use them).
  Estimated scope: L
  Completion signal: Two-miner regtest scenario passes end-to-end; all new RPCs return expected data.

- [ ] `CHKPT-03` Checkpoint: Regtest end-to-end proof

  Spec: `specs/120426-sharepool-protocol.md`
  Why now: Corpus Plans 009/010. Before any devnet or mainnet work, prove the full sharepool lifecycle works on regtest: activation, share production, relay, commitment, mining, claim.
  Codebase evidence: All preceding POOL tasks.
  Owns: End-to-end regtest proof script and review document.
  Integration touchpoints: All sharepool code.
  Scope boundary: Regtest proof only. No devnet. No mainnet.
  Acceptance criteria: (1) 4-node regtest network. (2) Sharepool activated via BIP9. (3) All nodes produce and relay shares. (4) Blocks contain valid commitments. (5) Claims succeed after maturity. (6) One node with 10% hashrate receives ~10% of rewards over 50 blocks (±5%). (7) Script is reproducible.
  Verification: `python3 test/functional/feature_sharepool_e2e.py` passes.
  Required tests: The e2e functional test IS the test.
  Dependencies: `POOL-08`.
  Estimated scope: M
  Completion signal: Reproducible regtest proof passes; review document committed.

### Tier 4: Operator Onboarding Hardening

- [ ] `OPS-01` Update bootstrap assets for current chain height

  Spec: `specs/120426-operator-onboarding.md`, `specs/120426-release-distribution.md`
  Why now: Bundled bootstrap assets are at height 15244 (chain bundle) and 15091 (UTXO snapshot). If the chain is now at height ~30000+, new operators sync from a stale starting point. Fresh assets cut sync time in half.
  Codebase evidence: `bootstrap/rng-mainnet-15244-datadir.tar.gz` (SHA256 `bf0bfad8...`), `bootstrap/rng-mainnet-15091.utxo` (SHA256 `622cd625...`). EXECPLAN.md shows canary at height 29957 as of 2026-04-10.
  Owns: Updated `bootstrap/` assets at current chain tip. Updated SHA256 hashes in `scripts/load-bootstrap.sh`, README.md, and spec docs.
  Integration touchpoints: `scripts/load-bootstrap.sh` (hash verification), `scripts/install.sh` (bootstrap path), README.md (bootstrap docs), `.github/workflows/release.yml` (asset inclusion).
  Scope boundary: Asset refresh only. Do not change bootstrap loading logic. Do not change assumeutxo framework.
  Acceptance criteria: (1) Chain bundle height > 25000. (2) UTXO snapshot height > 25000. (3) All SHA256 hashes updated in scripts and docs. (4) `scripts/load-bootstrap.sh` loads new assets without error.
  Verification: `sha256sum bootstrap/rng-mainnet-*.tar.gz bootstrap/rng-mainnet-*.utxo` matches documented values. `scripts/load-bootstrap.sh --help` shows new heights.
  Required tests: `python3 test/functional/feature_bootstrap.py` if it exists, or manual verification.
  Dependencies: `SYNC-01` (need merged branch with current chain access).
  Estimated scope: S
  Completion signal: Bootstrap assets refreshed; all hash references updated; load-bootstrap smoke test passes.

- [ ] `OPS-02` Harden Dockerfile default RPC password

  Spec: `specs/120426-operator-onboarding.md`
  Why now: Dockerfile contains `rpcpassword=changeme` — a hardcoded default that becomes a security vulnerability if any container is exposed beyond localhost.
  Codebase evidence: `Dockerfile` contains `rpcpassword=changeme` in the default CMD or entrypoint config.
  Owns: `Dockerfile` — replace hardcoded password with generated random password at container startup, or require environment variable.
  Integration touchpoints: `docker-compose.yml` (may reference password).
  Scope boundary: Fix the default. Do not redesign container architecture.
  Acceptance criteria: `grep "changeme" Dockerfile` returns zero matches. Container either generates random password on first run or errors if `RNG_RPC_PASSWORD` env var is not set.
  Verification: `docker build -t rng-test . && docker run --rm rng-test rngd --version` succeeds. `grep "changeme" Dockerfile` returns nothing.
  Required tests: None (build verification only).
  Dependencies: None.
  Estimated scope: XS
  Completion signal: No hardcoded password in Dockerfile.

### Tier 5: Release and Distribution

- [ ] `REL-01` Validate end-to-end reproducible build pipeline

  Spec: `specs/120426-release-distribution.md`
  Why now: `scripts/build-release.sh` uses PAX format, `SOURCE_DATE_EPOCH`, and normalized ownership for reproducibility, but this has never been verified end-to-end (two independent builds producing identical tarballs).
  Codebase evidence: `scripts/build-release.sh` sets `SOURCE_DATE_EPOCH` from git log, uses `--format=posix` (PAX), normalizes ownership to `root:root`. No CI job verifies reproducibility.
  Owns: Verification that two independent builds from the same commit produce byte-identical tarballs (or document what prevents it and fix).
  Integration touchpoints: `scripts/build-release.sh`, `.github/workflows/release.yml`.
  Scope boundary: Verify and fix. Do not redesign the build system.
  Acceptance criteria: Two clean builds from the same git commit on the same platform produce identical SHA256 for the tarball. If not achievable, document the specific non-determinism and whether it's fixable.
  Verification: `scripts/build-release.sh --output-dir /tmp/build1 && scripts/build-release.sh --output-dir /tmp/build2 && diff <(sha256sum /tmp/build1/*.tar.gz) <(sha256sum /tmp/build2/*.tar.gz)`
  Required tests: None (verification task).
  Dependencies: `SYNC-01`.
  Estimated scope: S
  Completion signal: Reproducibility confirmed or non-determinism documented with fix plan.

---

## Follow-On Work

Items below are real work identified by the specs but either depend on unresolved research, are explicitly future-phase, or are blocked by decisions not yet made.

- [ ] `FUTURE-01` Devnet deployment and adversarial testing (Plan 011)

  Spec: `specs/120426-sharepool-protocol.md`
  Why now: Not now — blocked on `CHKPT-03` (regtest proof must pass first).
  Codebase evidence: No devnet infrastructure exists.
  Owns: Multi-node devnet deployment; adversarial scenarios (eclipse attack, withholding, relay manipulation).
  Integration touchpoints: All sharepool code.
  Scope boundary: Devnet only. Not mainnet.
  Acceptance criteria: Adversarial scenarios documented and tested. No consensus-breaking bugs found, or fixes applied.
  Verification: Devnet runs stable for 48+ hours under adversarial load.
  Required tests: Adversarial test suite.
  Dependencies: `CHKPT-03`.
  Estimated scope: L
  Completion signal: Devnet stability report committed.

- [ ] `FUTURE-02` Mainnet activation preparation (Plan 012)

  Spec: `specs/120426-sharepool-protocol.md`
  Why now: Not now — blocked on `FUTURE-01` (devnet must prove stability first).
  Codebase evidence: `DEPLOYMENT_SHAREPOOL` will be `NEVER_ACTIVE` until this task changes it.
  Owns: BIP9 activation parameters for mainnet (start time, timeout), updated specs, operator docs, release with activation.
  Integration touchpoints: `src/kernel/chainparams.cpp`, all operator scripts, README.md.
  Scope boundary: Activation scheduling and docs. No new features.
  Acceptance criteria: Mainnet activation window defined. All docs updated. Release cut.
  Verification: `rngd --version` includes sharepool activation parameters.
  Required tests: Existing test suite passes.
  Dependencies: `FUTURE-01`.
  Estimated scope: M
  Completion signal: Release tag with mainnet activation parameters.

- [ ] `FUTURE-03` DNS seed operationality verification

  Spec: `specs/120426-network-identity.md`
  Why now: Not urgent — hardcoded seed peers work. But DNS seeds (`seed1.rng.network` etc.) may not resolve, leaving peer discovery dependent on 4 hardcoded IPs.
  Codebase evidence: `src/kernel/chainparams.cpp` references `seed1.rng.network`, `seed2.rng.network`, `seed3.rng.network`. Unclear if these resolve.
  Owns: Verify DNS seed resolution. If non-functional, either fix DNS records or remove from chainparams to avoid misleading timeout delays.
  Integration touchpoints: `src/kernel/chainparams.cpp`.
  Scope boundary: Verify and fix DNS or remove dead entries. Do not build new discovery infrastructure.
  Acceptance criteria: `dig seed1.rng.network` returns valid A records, or entries removed from chainparams.
  Verification: `dig +short seed1.rng.network seed2.rng.network seed3.rng.network`
  Required tests: None.
  Dependencies: None.
  Estimated scope: XS
  Completion signal: DNS seeds either resolve or are removed.

- [ ] `FUTURE-04` Agent wallet and MCP server implementation

  Spec: `specs/120426-wallet-rpc-surface.md`
  Why now: Not now — pure aspirational feature. `specs/agent-integration.md` describes `createagentwallet`, MCP tools, autonomy budgets, webhooks. None exist. Implementation depends on proven core features (wallet, mining, sharepool) being stable first.
  Codebase evidence: Zero agent-specific code in `src/`. No MCP server binary. No webhook infrastructure.
  Owns: Research task — scope the minimum viable agent wallet surface (which RPCs, which MCP tools) based on actual agent usage patterns.
  Integration touchpoints: `src/wallet/rpc/`, potential new `src/mcp/` directory.
  Scope boundary: Research and scoping only. No implementation until sharepool proves stable.
  Acceptance criteria: Scoping document identifying the minimum set of agent-specific features worth building, with effort estimates.
  Verification: Document exists and references concrete codebase insertion points.
  Required tests: None (research).
  Dependencies: `CHKPT-03` (core features must be proven stable first).
  Estimated scope: S
  Completion signal: Scoping document committed.

- [ ] `FUTURE-05` Cross-platform release expansion (Windows, ARM64 native)

  Spec: `specs/120426-release-distribution.md`
  Why now: Not urgent — current releases cover linux-x86_64, linux-arm64, macos-x86_64, macos-arm64. Windows builds exist in CI but no Windows release tarball is cut. ARM64 native testing (vs cross-compile) is untested for RandomX JIT.
  Codebase evidence: `.github/workflows/release.yml` builds 4 platforms. `.github/workflows/ci.yml` includes Windows MSVC and MinGW targets. No Windows release artifact in release.yml.
  Owns: Add Windows release tarball to release pipeline; verify RandomX JIT on native ARM64.
  Integration touchpoints: `scripts/build-release.sh`, `.github/workflows/release.yml`.
  Scope boundary: Release pipeline only. Do not port platform-specific code.
  Acceptance criteria: Windows tarball included in releases. ARM64 RandomX JIT verified functional.
  Verification: Release pipeline produces 5+ platform tarballs.
  Required tests: CI tests pass on all platforms.
  Dependencies: None.
  Estimated scope: M
  Completion signal: Release pipeline produces Windows tarball.

- [ ] `FUTURE-06` Atomic swap protocol implementation

  Spec: None in generated specs (referenced in `specs/swaps.md` which is not in this run's scope).
  Why now: Not now — `specs/swaps.md` describes HTLC-based P2P atomic swaps. This is a significant feature requiring new P2P messages, HTLC construction, chain monitoring, and CLI commands. Depends on stable wallet and network layers.
  Codebase evidence: No swap code exists. No HTLC construction code beyond Bitcoin Core's standard script support.
  Owns: Future implementation of `specs/swaps.md`.
  Integration touchpoints: `src/net_processing.cpp`, `src/wallet/`, new `src/swap/` module.
  Scope boundary: Blocked on stable core. Not in current planning horizon.
  Acceptance criteria: TBD after swap spec review.
  Verification: TBD.
  Required tests: TBD.
  Dependencies: Stable wallet, stable network, proven sharepool.
  Estimated scope: L
  Completion signal: TBD.

---

## Completed / Already Satisfied

- [x] `DONE-01` RandomX PoW integration

  Spec: `specs/120426-randomx-pow.md`
  Codebase evidence: `src/crypto/randomx_hash.{h,cpp}` (347 lines), `src/crypto/randomx/` (vendored v1.2.1), `src/pow.cpp:207-289` (`GetBlockPoWHash`, `CheckBlockProofOfWork`, `GetRandomXSeedHash`). Fixed genesis seed `"RNG Genesis Seed"`, Argon salt `"RNGCHAIN01"`, fast/light mode support, JIT on x86-64/ARM64/RISCV64.
  Verification: `build/bin/test_bitcoin --run_test=randomx_tests` passes. `python3 test/functional/feature_randomx.py` passes.

- [x] `DONE-02` Internal miner v2 architecture

  Spec: `specs/120426-internal-miner.md`
  Codebase evidence: `src/node/internal_miner.{h,cpp}` — 240-line header, ~530-line implementation. Coordinator + N workers, stride-based nonces, event-driven via `CValidationInterface`, lock-free template sharing, exponential backoff, `-mine`/`-mineaddress`/`-minethreads`/`-minerandomx`/`-minepriority` flags, `getinternalmininginfo` RPC at `src/rpc/mining.cpp:519`.
  Verification: `python3 test/functional/feature_internal_miner.py` passes.

- [x] `DONE-03` Network identity and P2P protocol

  Spec: `specs/120426-network-identity.md`
  Codebase evidence: `src/kernel/chainparams.cpp:140-147` — magic `0xB07C010E`, port 8433, HRP `rng`, P2PKH byte 25, protocol version 70100, user agent `/RNG:3.0.0/`, data directory `~/.rng`, config `rng.conf`. Hardcoded seed peers at lines 177-180. DNS seeds at lines 170-173.
  Verification: `build/bin/rngd --version` reports correct identity. Existing P2P and RPC tests pass.

- [x] `DONE-04` Consensus and chain rules

  Spec: `specs/120426-consensus-chain-rules.md`
  Codebase evidence: `src/kernel/chainparams.cpp:100-131` — 2.1M halving, 120s mainnet spacing, 720-block LWMA window, 60-block cut, all BIPs active from height 0, BIP9 with 90% threshold. `src/validation.cpp:1838-1858` — `GetBlockSubsidy()` with tail emission floor at 0.6 RNG (`TAIL_EMISSION = 60000000`). Genesis hash `83a6a482...`.
  Verification: `build/bin/test_bitcoin --run_test=validation_tests` passes.

- [x] `DONE-05` Wallet and RPC surface (base layer)

  Spec: `specs/120426-wallet-rpc-surface.md`
  Codebase evidence: Full Bitcoin Core v29.0 wallet in `src/wallet/` (SQLite-backed descriptor wallet). All standard RPCs: `getnewaddress`, `sendtoaddress`, `getbalance`, `listtransactions`, `getwalletinfo`, `getblockchaininfo`, `getmininginfo`, `getnetworkinfo`, `getpeerinfo`, `getblocktemplate`, `submitblock`, plus RNG-specific `getinternalmininginfo`. Bech32 HRP `rng`, default address type P2WPKH.
  Verification: Existing wallet and RPC functional tests pass.

- [x] `DONE-06` Operator onboarding scripts

  Spec: `specs/120426-operator-onboarding.md`
  Codebase evidence: `scripts/install.sh` (796 lines), `scripts/start-miner.sh` (233 lines), `scripts/doctor.sh` (379 lines), `scripts/load-bootstrap.sh` (380 lines), `scripts/install-public-node.sh` (185 lines), `scripts/install-public-miner.sh` (205 lines), `scripts/build-release.sh`, `scripts/verify-release.sh`. Bootstrap assets in `bootstrap/` at heights 15244/15091.
  Verification: Scripts have been used in live fleet deployment per EXECPLAN.md.

- [x] `DONE-07` Release and distribution pipeline

  Spec: `specs/120426-release-distribution.md`
  Codebase evidence: `.github/workflows/release.yml` (5.3 KB) — 4-platform deterministic builds with SHA256SUMS and build provenance attestation. `.github/workflows/ghcr.yml` — container publishing. `scripts/build-release.sh` — tarball construction with PAX format and normalized ownership.
  Verification: Live releases published; fleet deployed from release artifacts per EXECPLAN.md.

- [x] `DONE-08` QSB operator support (on main branch)

  Spec: `specs/120426-qsb-operator-support.md`
  Codebase evidence: On `main` branch (commit `bf58671eb3`): `src/script/qsb.{h,cpp}`, `src/node/qsb_pool.{h,cpp}`, `src/node/qsb_validation.{h,cpp}`, `contrib/qsb/` (Python builder), `test/functional/feature_qsb_builder.py`, `test/functional/feature_qsb_rpc.py`, `test/functional/feature_qsb_mining.py`, `src/test/qsb_tests.cpp`. Live canary proof: QSB funding tx `363a3e50...` mined at height 29946, QSB spend tx `e562d60c...` mined at height 29947 on `contabo-validator-01`.
  Verification: All QSB tests pass on main. Live transactions confirmed on mainnet.
