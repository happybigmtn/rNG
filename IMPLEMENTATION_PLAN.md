# IMPLEMENTATION_PLAN

Generated: 2026-04-12
Branch at time of planning: `feat/bitcoin-30-qsb` (HEAD `f39bc788f4`)
Main: `8e33f25b30` (ahead of current branch; includes Bitcoin Core v30.2 port with QSB)

---

## Priority Work

### Tier 3: Sharepool Core Implementation

- [!] `POOL-07` Implement payout commitment and claim program

  Spec: `specs/sharepool.md` (authoritative), with `specs/120426-sharepool-protocol.md` retained as historical context only.
  Why now: Corpus Plan 007. The payout commitment (Merkle root in coinbase) and claim program (witness v2 spend path) are the consensus-enforcing layer. Without them, shares have no economic meaning.
  Blocker: The current specs and live code do not yet define a safe multi-claim accounting model for pooled rewards. A single shared reward UTXO can normally be spent only once; the historical simple witness-v2 leaf proof model would make the first valid claimant consume the whole reward output unless the design adds either a residual-output covenant model or explicit per-leaf claim-state accounting. `src/script/interpreter.cpp` still treats unknown witness v2 programs as anyone-can-spend for soft-fork compatibility, and `genesis/plans/007-payout-commitment-and-claim-program.md` only sketches a single-claim witness path. Consensus implementation must wait for that accounting design to be specified.
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

- [!] `POOL-08` Extend internal miner and wallet for sharepool integration

  Spec: `specs/120426-internal-miner.md`, `specs/120426-wallet-rpc-surface.md`
  Why now: Corpus Plan 008. The miner must produce shares alongside block attempts, and the wallet must track pooled rewards. This is the user-facing integration layer.
  Blocker: The task's explicit dependency `POOL-07` is still blocked. Live code now contains the sharepool deployment boundary and sharechain/P2P relay from POOL-04/POOL-05/POOL-06 (`src/consensus/params.h`, `src/node/sharechain.{h,cpp}`, `src/net_processing.cpp`), but there is still no `src/consensus/sharepool.{h,cpp}` payout commitment implementation and no witness-v2 claim accounting model. Wallet auto-claim behavior and reward commitment RPCs cannot be truthfully implemented until the consensus payout/claim contract is specified and landed.
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

- [!] `CHKPT-03` Checkpoint: Regtest end-to-end proof

  Spec: `specs/120426-sharepool-protocol.md`
  Why now: Corpus Plans 009/010. Before any devnet or mainnet work, prove the full sharepool lifecycle works on regtest: activation, share production, relay, commitment, mining, claim.
  Blocker: The task's explicit dependency `POOL-08` remains blocked by `POOL-07`. Live code has the sharepool activation boundary and share relay (`src/consensus/params.h`, `src/node/sharechain.{h,cpp}`, `src/net_processing.cpp`), but it still has no consensus payout commitment/claim implementation, no dual-target share-producing miner, no wallet pooled-balance/auto-claim surface, no `submitshare`/`getsharechaininfo`/`getrewardcommitment` RPCs, and no `test/functional/feature_sharepool_e2e.py`. A regtest end-to-end proof cannot be truthfully implemented until `POOL-07` and `POOL-08` are unblocked and landed.
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

### Tier 5: Release and Distribution

---

## Follow-On Work

Items below are real work identified by the specs but either depend on unresolved research, are explicitly future-phase, or are blocked by decisions not yet made.

- [!] `FUTURE-01` Devnet deployment and adversarial testing (Plan 011)

  Spec: `specs/120426-sharepool-protocol.md`
  Why now: Not now — blocked on `CHKPT-03` (regtest proof must pass first).
  Blocker: The explicit dependency `CHKPT-03` is still blocked by the unresolved sharepool payout/claim contract and missing miner/wallet integration. Devnet deployment cannot start until the regtest end-to-end proof exists.
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

- [!] `FUTURE-02` Mainnet activation preparation (Plan 012)

  Spec: `specs/120426-sharepool-protocol.md`
  Why now: Not now — blocked on `FUTURE-01` (devnet must prove stability first).
  Blocker: The explicit dependency `FUTURE-01` is blocked until `CHKPT-03` exists and devnet stability is proven.
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

- [!] `FUTURE-04` Agent wallet and MCP server implementation

  Spec: `specs/120426-wallet-rpc-surface.md`
  Why now: Not now — pure aspirational feature. `specs/agent-integration.md` describes `createagentwallet`, MCP tools, autonomy budgets, webhooks. None exist. Implementation depends on proven core features (wallet, mining, sharepool) being stable first.
  Blocker: The explicit dependency `CHKPT-03` is blocked. Agent wallet/MCP scoping should wait until the core sharepool feature set has a proven regtest lifecycle.
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

- [!] `FUTURE-06` Atomic swap protocol implementation

  Spec: None in generated specs (referenced in `specs/swaps.md` which is not in this run's scope).
  Why now: Not now — `specs/swaps.md` describes HTLC-based P2P atomic swaps. This is a significant feature requiring new P2P messages, HTLC construction, chain monitoring, and CLI commands. Depends on stable wallet and network layers.
  Blocker: Stable wallet/network/sharepool prerequisites are not met; `POOL-07`, `POOL-08`, `CHKPT-03`, and `FUTURE-01` remain blocked.
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
  Codebase evidence: live checkout includes `scripts/start-miner.sh` (233 lines), `scripts/doctor.sh` (377 lines), `scripts/load-bootstrap.sh` (386 lines), `scripts/install-public-node.sh` (185 lines), `scripts/install-public-miner.sh` (205 lines), `scripts/build-release.sh` (295 lines), and `scripts/verify-release.sh` (225 lines). Root `install.sh` and `scripts/install.sh` remain absent and are tracked in `WORKLIST.md`. Bootstrap assets in `bootstrap/` are at height 29944.
  Verification: Scripts have been used in live fleet deployment per EXECPLAN.md.

- [x] `DONE-07` Release and distribution pipeline

  Spec: `specs/120426-release-distribution.md`
  Codebase evidence: `scripts/build-release.sh` constructs deterministic tarballs with PAX format and normalized ownership; `scripts/check-reproducible-release.sh` verifies same-platform reproducibility. Live `.github/workflows/` currently contains only `ci.yml`; tracked release and GHCR workflows are absent.
  Verification: Same-commit linux-x86_64 reproducibility passes with `scripts/check-reproducible-release.sh`; live releases/fleet deployment remain historical evidence per EXECPLAN.md.

- [x] `DONE-08` QSB operator support (on main branch)

  Spec: `specs/120426-qsb-operator-support.md`
  Codebase evidence: On `main` branch (commit `bf58671eb3`): `src/script/qsb.{h,cpp}`, `src/node/qsb_pool.{h,cpp}`, `src/node/qsb_validation.{h,cpp}`, `contrib/qsb/` (Python builder), `test/functional/feature_qsb_builder.py`, `test/functional/feature_qsb_rpc.py`, `test/functional/feature_qsb_mining.py`, `src/test/qsb_tests.cpp`. Live canary proof: QSB funding tx `363a3e50...` mined at height 29946, QSB spend tx `e562d60c...` mined at height 29947 on `contabo-validator-01`.
  Verification: All QSB tests pass on main. Live transactions confirmed on mainnet.
